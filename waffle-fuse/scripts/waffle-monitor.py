#!/usr/bin/env python3
"""
waffle-monitor  —  system-level udev watcher + NBD lifecycle manager
                   for Waffle V2 / DrawBridge floppy adapters.

Runs as root (via the waffle-monitor.service systemd unit).

Flow when a device is plugged in:
  1. Detect udev add event for ttyUSB with matching VID:PID
  2. Start waffle-nbd <tty>            (manages disk presence internally)
  3. Connect nbd-client in a retry loop (server becomes ready once a disk
     is detected — this may take time if the drive is empty)
  4. Mount the block device:
       a. mount -t affs  (Amiga OFS/FFS)
       b. mount -t vfat  (DOS FAT 12/16)
     Mount point: /run/media/<session-user>/waffle-<tty-name>/
  5. Open the file manager (xdg-open) for the session user
  6. On failure: send a desktop notification to the session user

Flow when the device is removed:
  1. Unmount the filesystem (lazy umount)
  2. Disconnect nbd-client  (nbd-client -d /dev/nbdN)
  3. Terminate waffle-nbd

Installation:
  sudo scripts/install-udev.sh
  (copies this file to /usr/local/lib/waffle-fuse/ and installs the
   systemd unit waffle-monitor.service)

Requires:
  pip install pyudev        (or: apt install python3-pyudev)
  apt install nbd-client
"""

from __future__ import annotations

import argparse
import logging
import os
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import Optional

# ── pyudev ────────────────────────────────────────────────────────────────────
try:
    import pyudev
except ImportError:
    sys.exit(
        "ERROR: pyudev not found.\n"
        "  apt install python3-pyudev\n"
        "  pip install pyudev"
    )

# ── Logging ───────────────────────────────────────────────────────────────────
log = logging.getLogger("waffle-monitor")

# ── Device table ──────────────────────────────────────────────────────────────
# FTDI FT230X (Waffle V2) and FT232RL (DrawBridge)
WAFFLE_DEVICES: set[tuple[str, str]] = {
    ("0403", "6015"),
    ("0403", "6001"),
}

# ── Tunables (overridable via CLI) ────────────────────────────────────────────
NBD_HOST            = "127.0.0.1"
NBD_PORT_BASE       = 10809
NBD_CONNECT_RETRIES = 90          # × NBD_RETRY_INTERVAL = 3 min max
NBD_RETRY_INTERVAL  = 2.0         # seconds between nbd-client attempts
FSTYPES             = ("affs", "vfat")  # tried in order on manual fallback

# ── Port allocator ────────────────────────────────────────────────────────────
_used_ports: set[int] = set()
_ports_lock = threading.Lock()


def _alloc_port() -> int:
    with _ports_lock:
        p = NBD_PORT_BASE
        while p in _used_ports:
            p += 1
        _used_ports.add(p)
        return p


def _free_port(p: int) -> None:
    with _ports_lock:
        _used_ports.discard(p)


# ── NBD device allocator ──────────────────────────────────────────────────────
_used_nbds: set[str] = set()
_nbds_lock = threading.Lock()


def _alloc_nbd() -> Optional[str]:
    with _nbds_lock:
        for n in range(16):
            dev = f"/dev/nbd{n}"
            if dev in _used_nbds:
                continue
            pid_path = Path(f"/sys/block/nbd{n}/pid")
            if pid_path.exists():
                try:
                    if int(pid_path.read_text().strip()) > 0:
                        continue  # kernel reports connected
                except (ValueError, OSError):
                    pass
            if Path(dev).exists():
                _used_nbds.add(dev)
                return dev
    return None


def _free_nbd(dev: str) -> None:
    with _nbds_lock:
        _used_nbds.discard(dev)


# ── Find waffle-nbd binary ────────────────────────────────────────────────────
def _find_waffle_nbd(hint: Optional[str] = None) -> Optional[str]:
    candidates: list[Path] = []
    if hint:
        candidates.append(Path(hint))
    # dev-tree: scripts/ is inside waffle-fuse/, binary at waffle-fuse/
    candidates.append(Path(__file__).resolve().parent.parent / "waffle-nbd")
    # installed: lib dir → prefix/bin
    lib_dir = Path(__file__).resolve().parent
    candidates.append(lib_dir.parent.parent / "bin" / "waffle-nbd")
    p = shutil.which("waffle-nbd")
    if p:
        candidates.append(Path(p))
    for c in candidates:
        if c.is_file() and os.access(c, os.X_OK):
            return str(c)
    return None


# ── USB VID/PID resolution ────────────────────────────────────────────────────
def _usb_ids(device: pyudev.Device) -> tuple[str, str]:
    """Return (vid, pid) for a udev device, walking up to usb_device parent."""
    vid = device.get("ID_VENDOR_ID", "").lower()
    pid = device.get("ID_MODEL_ID", "").lower()
    if vid and pid:
        return vid, pid
    try:
        usb = device.find_parent(subsystem="usb", device_type="usb_device")
        if usb:
            vid = (usb.attributes.get("idVendor") or b"").decode().strip().lower()
            pid = (usb.attributes.get("idProduct") or b"").decode().strip().lower()
    except Exception:
        pass
    return vid, pid


# ── Session-user helpers (called from root context) ───────────────────────────
def _find_session_user() -> Optional[str]:
    """Return the username of the first active graphical session."""
    if shutil.which("loginctl"):
        # 1st pass: prefer x11/wayland sessions
        try:
            out = subprocess.check_output(
                ["loginctl", "list-sessions", "--no-legend"],
                text=True, stderr=subprocess.DEVNULL,
            )
            for line in out.splitlines():
                parts = line.split()
                if len(parts) < 3:
                    continue
                sid, _, uname = parts[0], parts[1], parts[2]
                if uname in ("root", ""):
                    continue
                try:
                    stype = subprocess.check_output(
                        ["loginctl", "show-session", sid, "-p", "Type", "--value"],
                        text=True, stderr=subprocess.DEVNULL,
                    ).strip()
                except subprocess.SubprocessError:
                    stype = ""
                if stype in ("x11", "wayland", "mir"):
                    return uname
        except subprocess.SubprocessError:
            pass
        # 2nd pass: any non-root session
        try:
            out = subprocess.check_output(
                ["loginctl", "list-sessions", "--no-legend"],
                text=True, stderr=subprocess.DEVNULL,
            )
            for line in out.splitlines():
                parts = line.split()
                if len(parts) >= 3 and parts[2] not in ("root", ""):
                    return parts[2]
        except subprocess.SubprocessError:
            pass
    # who fallback
    try:
        out = subprocess.check_output(["who"], text=True, stderr=subprocess.DEVNULL)
        for line in out.splitlines():
            m = re.search(r"\(:[\d.]+\)", line)
            if m:
                user = line.split()[0]
                if user != "root":
                    return user
        for line in out.splitlines():
            user = line.split()[0]
            if user != "root":
                return user
    except subprocess.SubprocessError:
        pass
    return None


def _user_env(username: str) -> dict[str, str]:
    """Build the minimal environment needed to reach the user's desktop session."""
    uid = int(subprocess.check_output(["id", "-u", username], text=True).strip())
    home = subprocess.check_output(
        ["getent", "passwd", username], text=True
    ).split(":")[5]
    return {
        "HOME":                      home,
        "USER":                      username,
        "LOGNAME":                   username,
        "XDG_RUNTIME_DIR":           f"/run/user/{uid}",
        "DBUS_SESSION_BUS_ADDRESS":  f"unix:path=/run/user/{uid}/bus",
        "DISPLAY":                   ":0",
    }


def _run_as_user(username: str, *cmd: str) -> bool:
    """Run a command as the session user (from a root process)."""
    env = _user_env(username)
    env_pairs = " ".join(f"{k}={v}" for k, v in env.items())
    # Use runuser to drop privileges without re-authentication
    shell_cmd = f"exec env {env_pairs} {subprocess.list2cmdline(list(cmd))}"
    r = subprocess.run(
        ["runuser", "-s", "/bin/sh", username, "-c", shell_cmd],
        capture_output=True, timeout=10,
    )
    return r.returncode == 0


def notify_user(
    username: str,
    summary: str,
    body: str = "",
    icon: str = "drive-removable-media-usb",
) -> None:
    """Send a desktop notification to the session user."""
    log.debug("notify → %s: %s — %s", username, summary, body)
    try:
        _run_as_user(username, "notify-send", "-i", icon, summary, body)
    except Exception as exc:
        log.debug("notify-send failed: %s", exc)


# ── NBD kernel module ─────────────────────────────────────────────────────────
_nbd_module_loaded = False
_nbd_module_lock   = threading.Lock()

def _ensure_nbd_module() -> bool:
    """
    Load the 'nbd' kernel module if not already present.
    Returns True if /dev/nbd0 is accessible afterwards.
    """
    global _nbd_module_loaded
    with _nbd_module_lock:
        if _nbd_module_loaded:
            return True
        if Path("/dev/nbd0").exists():
            _nbd_module_loaded = True
            return True
        log.info("Loading nbd kernel module...")
        r = subprocess.run(["modprobe", "nbd"], capture_output=True)
        if r.returncode != 0:
            log.error(
                "modprobe nbd failed: %s",
                r.stderr.decode(errors="replace").strip(),
            )
            return False
        # Wait up to 2 s for udev to create /dev/nbdN devices
        for _ in range(20):
            if Path("/dev/nbd0").exists():
                break
            time.sleep(0.1)
        ok = Path("/dev/nbd0").exists()
        if ok:
            log.info("nbd module loaded, /dev/nbd0 is available")
            _nbd_module_loaded = True
        else:
            log.error("nbd module loaded but /dev/nbd0 still not present")
        return ok


# ── Device session ────────────────────────────────────────────────────────────
class DeviceSession:
    """Full NBD + mount lifecycle for one plugged-in device."""

    def __init__(self, tty: str, waffle_nbd_bin: str, session_user: str) -> None:
        self.tty           = tty
        self._bin          = waffle_nbd_bin
        self._user         = session_user
        self.port          = _alloc_port()
        self.nbd_dev: Optional[str]    = None
        self.mount_point: Optional[str] = None
        self._server: Optional[subprocess.Popen] = None
        self._stop         = threading.Event()
        self._thread       = threading.Thread(
            target=self._run, daemon=True, name=f"waffle:{Path(tty).name}"
        )

    def start(self) -> None:
        self._thread.start()

    def stop(self) -> None:
        log.info("[%s] stopping session", self.tty)
        self._stop.set()
        self._cleanup()
        _free_port(self.port)

    # ── Internal ──────────────────────────────────────────────────────────────

    def _run(self) -> None:
        tty = self.tty
        log.info("[%s] session started (port=%d user=%s)", tty, self.port, self._user)

        if not self._start_server():
            return

        _ensure_nbd_module()

        self.nbd_dev = _alloc_nbd()
        if not self.nbd_dev:
            log.error("[%s] no free /dev/nbdN available", tty)
            notify_user(
                self._user,
                "Waffle: no NBD device",
                "No free /dev/nbdN found after loading nbd module.\n"
                "Try: modprobe nbd nbds_max=4",
                "dialog-error",
            )
            self._stop_server()
            return
        log.info("[%s] allocated %s", tty, self.nbd_dev)

        if not self._connect_nbd():
            if not self._stop.is_set():
                notify_user(
                    self._user,
                    "Waffle: connection timeout",
                    f"nbd-client could not connect to {NBD_HOST}:{self.port} "
                    f"after {NBD_CONNECT_RETRIES} attempts.",
                    "dialog-error",
                )
            self._stop_server()
            _free_nbd(self.nbd_dev)
            self.nbd_dev = None
            return

        if not self._stop.is_set():
            self._mount_and_open()

    # ── Step 1: start the NBD server ──────────────────────────────────────────
    def _start_server(self) -> bool:
        cmd = [self._bin, self.tty, NBD_HOST, str(self.port)]
        log.info("[%s] launching: %s", self.tty, " ".join(cmd))
        try:
            self._server = subprocess.Popen(
                cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
            )
            log.info("[%s] waffle-nbd PID=%d", self.tty, self._server.pid)
            return True
        except OSError as exc:
            log.error("[%s] cannot start waffle-nbd: %s", self.tty, exc)
            notify_user(
                self._user,
                "Waffle: server error",
                f"Could not start waffle-nbd:\n{exc}",
                "dialog-error",
            )
            return False

    # ── Step 2: connect nbd-client ────────────────────────────────────────────
    def _connect_nbd(self) -> bool:
        """
        Retry until nbd-client succeeds.

        waffle-nbd spends Phase 1 probing for a floppy; the server only
        accepts NBD clients after a disk is detected (Phase 3 of the state
        machine).  We retry quietly until the server is ready.
        """
        tty, dev = self.tty, self.nbd_dev
        log.info(
            "[%s] waiting for nbd-client: %s:%d → %s", tty, NBD_HOST, self.port, dev
        )
        for attempt in range(1, NBD_CONNECT_RETRIES + 1):
            if self._stop.is_set():
                return False
            if self._server and self._server.poll() is not None:
                log.error("[%s] waffle-nbd exited early", tty)
                return False

            r = subprocess.run(
                ["nbd-client", NBD_HOST, str(self.port), dev],
                capture_output=True,
            )
            if r.returncode == 0:
                log.info("[%s] nbd-client connected (attempt %d)", tty, attempt)
                return True

            log.debug(
                "[%s] nbd-client attempt %d: %s",
                tty, attempt,
                r.stderr.decode(errors="replace").strip(),
            )
            self._stop.wait(timeout=NBD_RETRY_INTERVAL)

        return False

    # ── Step 3: mount ─────────────────────────────────────────────────────────
    def _mount_and_open(self) -> None:
        tty, dev = self.tty, self.nbd_dev
        assert dev is not None

        # Give the kernel a moment to settle after nbd-client connects
        # and to expose the correct block size/geometry via the NBD layer.
        time.sleep(1.0)

        # Ensure filesystem modules are loaded before attempting mount
        for mod in ("affs", "vfat"):
            subprocess.run(["modprobe", mod], capture_output=True)

        # Build mount point under /run/media/<user>/waffle-<tty-name>/
        tty_name = Path(self.tty).name
        mp = Path(f"/run/media/{self._user}/waffle-{tty_name}")
        mp.mkdir(parents=True, exist_ok=True)
        # Chown so the user can browse it
        try:
            uid = int(subprocess.check_output(["id", "-u", self._user], text=True).strip())
            gid = int(subprocess.check_output(["id", "-g", self._user], text=True).strip())
            os.chown(mp, uid, gid)
        except Exception:
            uid = gid = -1

        for fstype in FSTYPES:
            opts = self._mount_opts(fstype, uid, gid)
            cmd  = ["mount", "-t", fstype, dev, str(mp)] + (["-o", opts] if opts else [])
            log.info("[%s] trying: %s", tty, " ".join(cmd))
            r = subprocess.run(cmd, capture_output=True)
            if r.returncode == 0:
                self.mount_point = str(mp)
                log.info("[%s] mounted as %s at %s", tty, fstype, mp)
                notify_user(
                    self._user,
                    "Waffle: disk mounted",
                    f"Mounted as {fstype.upper()} at {mp}",
                    "drive-removable-media",
                )
                self._open_fm(str(mp))
                return
            # Log at INFO level so mount errors are always visible in journald
            log.info(
                "[%s] mount -t %s failed: %s",
                tty, fstype,
                r.stderr.decode(errors="replace").strip(),
            )

        # All attempts failed
        try:
            mp.rmdir()
        except OSError:
            pass
        log.warning("[%s] filesystem not recognised on %s", tty, dev)
        notify_user(
            self._user,
            "Waffle: unknown filesystem",
            f"Could not identify the filesystem on {dev}.\n"
            "The disk may be unformatted or in an unsupported format.",
            "dialog-warning",
        )

    @staticmethod
    def _mount_opts(fstype: str, uid: int, gid: int) -> str:
        if uid < 0:
            return ""
        if fstype == "vfat":
            return f"uid={uid},gid={gid},umask=022"
        if fstype == "affs":
            return f"uid={uid}"
        return ""

    def _open_fm(self, path: str) -> None:
        try:
            _run_as_user(self._user, "xdg-open", path)
        except Exception as exc:
            log.debug("xdg-open failed: %s", exc)

    # ── Cleanup ───────────────────────────────────────────────────────────────

    def _cleanup(self) -> None:
        self._unmount()
        self._disconnect_nbd()
        self._stop_server()

    def _unmount(self) -> None:
        mp = self.mount_point
        if not mp:
            return
        log.info("[%s] unmounting %s", self.tty, mp)
        subprocess.run(["umount", "-l", mp], capture_output=True)
        self.mount_point = None
        try:
            Path(mp).rmdir()
        except OSError:
            pass

    def _disconnect_nbd(self) -> None:
        dev = self.nbd_dev
        if not dev:
            return
        log.info("[%s] disconnecting %s", self.tty, dev)
        subprocess.run(["nbd-client", "-d", dev], capture_output=True)
        _free_nbd(dev)
        self.nbd_dev = None

    def _stop_server(self) -> None:
        proc, self._server = self._server, None
        if proc is None or proc.poll() is not None:
            return
        log.info("[%s] terminating waffle-nbd PID=%d", self.tty, proc.pid)
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


# ── udev monitor ──────────────────────────────────────────────────────────────
class WaffleMonitor:

    def __init__(self, waffle_nbd_bin: str) -> None:
        self._bin      = waffle_nbd_bin
        self._sessions: dict[str, DeviceSession] = {}
        self._lock     = threading.Lock()
        self._quit     = threading.Event()

    def run(self) -> None:
        ctx     = pyudev.Context()
        monitor = pyudev.Monitor.from_netlink(ctx)
        monitor.filter_by("tty")
        monitor.start()

        log.info("Waffle Monitor started")
        log.info(
            "  devices : %s",
            ", ".join(f"{v}:{p}" for v, p in sorted(WAFFLE_DEVICES)),
        )
        log.info("  binary  : %s", self._bin)
        log.info("  nbd host: %s  port base: %d", NBD_HOST, NBD_PORT_BASE)

        for device in iter(monitor.poll, None):
            if self._quit.is_set():
                break
            action  = device.action
            devnode = device.device_node
            if not devnode:
                continue
            vid, pid = _usb_ids(device)
            if (vid, pid) not in WAFFLE_DEVICES:
                continue
            log.info("udev %s: %s  %s:%s", action, devnode, vid, pid)
            if action == "add":
                self._on_add(devnode)
            elif action == "remove":
                self._on_remove(devnode)

    def _on_add(self, tty: str) -> None:
        user = _find_session_user()
        if not user:
            log.warning("[%s] no active user session found — proceeding without notifications", tty)
            user = "root"

        with self._lock:
            if tty in self._sessions:
                log.warning("[%s] already active — ignoring duplicate add", tty)
                return
            session = DeviceSession(tty, self._bin, user)
            self._sessions[tty] = session

        if user != "root":
            notify_user(
                user,
                "Waffle: device connected",
                f"{Path(tty).name} detected.\n"
                "Insert a floppy disk — it will be mounted automatically.",
                "drive-removable-media-usb",
            )
        session.start()

    def _on_remove(self, tty: str) -> None:
        with self._lock:
            session = self._sessions.pop(tty, None)
        if not session:
            log.debug("[%s] remove event — no active session", tty)
            return
        user = session._user
        session.stop()
        if user and user != "root":
            notify_user(
                user,
                "Waffle: device removed",
                f"{Path(tty).name} disconnected.",
                "drive-removable-media-usb",
            )

    def stop_all(self) -> None:
        self._quit.set()
        with self._lock:
            sessions, self._sessions = list(self._sessions.values()), {}
        for s in sessions:
            s.stop()


# ── Entry point ───────────────────────────────────────────────────────────────
def main() -> None:
    global NBD_PORT_BASE, NBD_CONNECT_RETRIES

    if os.geteuid() != 0:
        sys.exit(
            "ERROR: waffle-monitor must run as root.\n"
            "  Start via:  systemctl start waffle-monitor.service"
        )

    parser = argparse.ArgumentParser(
        description="Waffle Monitor — NBD lifecycle manager for Waffle/DrawBridge adapters",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="See waffle-monitor.service for the recommended systemd setup.",
    )
    parser.add_argument(
        "--waffle-nbd", metavar="PATH",
        help="path to waffle-nbd binary (auto-detected if omitted)",
    )
    parser.add_argument(
        "--port", type=int, default=NBD_PORT_BASE, metavar="N",
        help=f"base NBD TCP port (default: {NBD_PORT_BASE})",
    )
    parser.add_argument(
        "--retries", type=int, default=NBD_CONNECT_RETRIES, metavar="N",
        help=f"max nbd-client connection attempts (default: {NBD_CONNECT_RETRIES})",
    )
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)-7s %(name)s: %(message)s",
        datefmt="%H:%M:%S",
    )

    NBD_PORT_BASE       = args.port
    NBD_CONNECT_RETRIES = args.retries

    nbd_bin = _find_waffle_nbd(args.waffle_nbd)
    if not nbd_bin:
        sys.exit(
            "ERROR: waffle-nbd not found.\n"
            "  Build with 'make waffle-nbd', or use --waffle-nbd PATH."
        )

    monitor = WaffleMonitor(nbd_bin)

    def _shutdown(sig, _frame) -> None:
        log.info("Signal %d — shutting down", sig)
        monitor.stop_all()
        sys.exit(0)

    signal.signal(signal.SIGTERM, _shutdown)
    signal.signal(signal.SIGINT,  _shutdown)

    monitor.run()


if __name__ == "__main__":
    main()
