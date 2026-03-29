#!/usr/bin/env python3
"""
waffle-daemon – system tray daemon for DrawBridge/Waffle floppy reader.

States:
  idle     – polling for serial port, no disk mounted
  mounting – waffle-fuse running, waiting for mount + init
  mounted  – disk mounted; tray shows eject button
  ejecting – sync + unmount in progress

On mount: autodetects format (Amiga OFS/FFS or FAT12/FAT16) and
          density (DD/HD) from waffle-fuse stdout.
On eject: sync, fusermount3 -u, back to idle.
"""

import gi
gi.require_version('Gtk', '3.0')

import os
import sys
import subprocess
import threading
import time
import signal

from gi.repository import Gtk, GLib

# ── Paths ─────────────────────────────────────────────────────────────────────
SCRIPT_DIR  = os.path.dirname(os.path.realpath(__file__))
WAFFLE_BIN  = os.path.normpath(os.path.join(SCRIPT_DIR, '..', 'waffle-fuse'))
ICON_DIR    = os.path.normpath(os.path.join(SCRIPT_DIR, '..', 'icons'))
MOUNTPOINT  = '/tmp/waffle-auto'

SERIAL_CANDIDATES = [
    '/dev/ttyUSB0', '/dev/ttyUSB1',
    '/dev/ttyACM0', '/dev/ttyACM1',
]

POLL_INTERVAL_MS  = 2000   # probe poll interval
CHECK_INTERVAL_MS = 3000   # check mount health when mounted

# ── Tray icon ─────────────────────────────────────────────────────────────────
ICONS = {
    'idle':     os.path.join(ICON_DIR, 'tray-idle.svg'),
    'mounting': os.path.join(ICON_DIR, 'tray-mounting.svg'),
    'mounted':  os.path.join(ICON_DIR, 'tray-mounted.svg'),
}

# ── Notification helper ───────────────────────────────────────────────────────
def notify(title, msg, error=False):
    icon = 'dialog-error' if error else 'media-floppy'
    for cmd in [['notify-send', '-i', icon, title, msg],
                ['zenity', '--notification', '--text', f'{title}: {msg}']]:
        try:
            subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            return
        except FileNotFoundError:
            continue

# ── Mountpoint helpers ────────────────────────────────────────────────────────
def is_mounted(path):
    try:
        return subprocess.run(
            ['mountpoint', '-q', path],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        ).returncode == 0
    except Exception:
        return False

def do_unmount(path):
    for cmd in [['fusermount3', '-u', path],
                ['fusermount',  '-u', path],
                ['umount', path]]:
        try:
            if subprocess.run(cmd, stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL,
                              timeout=10).returncode == 0:
                return True
        except (FileNotFoundError, subprocess.TimeoutExpired):
            continue
    # Lazy unmount as last resort
    try:
        subprocess.run(['umount', '-l', path],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception:
        pass
    return False

def open_file_manager(path):
    for cmd in [['xdg-open', path], ['thunar', path],
                ['nautilus', path], ['dolphin', path], ['pcmanfm', path]]:
        try:
            subprocess.Popen(cmd, stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL)
            return
        except FileNotFoundError:
            continue

# ── Daemon ────────────────────────────────────────────────────────────────────
class WaffleDaemon:
    def __init__(self):
        self.state      = 'idle'
        self.port       = None
        self.waffle_proc = None
        self.disk_info  = ''      # e.g. "Amiga OFS (read-write)"
        self._probing   = False   # True while a probe thread is running

        self._setup_tray()
        GLib.timeout_add(POLL_INTERVAL_MS, self._poll_tick)

    # ── Tray setup ────────────────────────────────────────────────────────────
    def _setup_tray(self):
        self.tray = Gtk.StatusIcon()
        self._set_icon('idle')
        self._set_tooltip('waffle-fuse: waiting for disk…')
        self.tray.set_visible(True)
        self.tray.connect('activate',    self._on_activate)
        self.tray.connect('popup-menu',  self._on_popup)

    def _set_icon(self, state):
        path = ICONS.get(state, ICONS['idle'])
        try:
            self.tray.set_from_file(path)
        except Exception:
            pass  # icon file missing – not fatal

    def _set_tooltip(self, text):
        self.tray.set_tooltip_text(text)

    # ── Context menu ─────────────────────────────────────────────────────────
    def _make_menu(self):
        menu = Gtk.Menu()

        lbl = Gtk.MenuItem(label=self.disk_info or 'Waiting for disk…')
        lbl.set_sensitive(False)
        menu.append(lbl)

        menu.append(Gtk.SeparatorMenuItem())

        self._item_open = Gtk.MenuItem(label='📂  Open File Manager')
        self._item_open.connect('activate', lambda _: open_file_manager(MOUNTPOINT))
        self._item_open.set_sensitive(self.state == 'mounted')
        menu.append(self._item_open)

        self._item_eject = Gtk.MenuItem(label='⏏  Eject && Unmount')
        self._item_eject.connect('activate', self._on_eject)
        self._item_eject.set_sensitive(self.state == 'mounted')
        menu.append(self._item_eject)

        menu.append(Gtk.SeparatorMenuItem())

        item_quit = Gtk.MenuItem(label='Quit')
        item_quit.connect('activate', self._on_quit)
        menu.append(item_quit)

        menu.show_all()
        return menu

    def _on_activate(self, icon):
        if self.state == 'mounted':
            open_file_manager(MOUNTPOINT)

    def _on_popup(self, icon, button, ts):
        menu = self._make_menu()
        menu.popup(None, None, Gtk.StatusIcon.position_menu, icon, button, ts)

    # ── Polling ───────────────────────────────────────────────────────────────
    def _probe_port(self, port):
        """Run waffle-fuse --probe <port>. Returns True if a disk is present."""
        try:
            r = subprocess.run(
                [WAFFLE_BIN, '--probe', port],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                timeout=15
            )
            return r.returncode == 0  # 0 = disk present
        except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
            return False

    def _poll_tick(self):
        """Called every POLL_INTERVAL_MS while idle."""
        if self.state != 'idle':
            return False  # stop this timer

        # Don't stack probes – skip tick if a probe thread is already running
        if self._probing:
            return True

        ports = [p for p in SERIAL_CANDIDATES if os.path.exists(p)]
        if ports:
            self._probing = True
            threading.Thread(target=self._probe_thread, args=(ports,),
                             daemon=True).start()
        return True  # keep timer alive

    def _probe_thread(self, ports):
        """Background thread: probe each candidate port, idle_add result."""
        for port in ports:
            if self._probe_port(port):
                GLib.idle_add(self._on_disk_detected, port)
                return
        GLib.idle_add(self._on_probe_done)

    def _on_disk_detected(self, port):
        self._probing = False
        self.port = port
        self._start_mount()
        return False

    def _on_probe_done(self):
        self._probing = False
        return False

    def _watch_mounted_tick(self):
        """Called every CHECK_INTERVAL_MS while mounted; detects unexpected unmount."""
        if self.state != 'mounted':
            return False
        if not is_mounted(MOUNTPOINT) or (
                self.waffle_proc and self.waffle_proc.poll() is not None):
            # Disk was removed or process crashed
            GLib.idle_add(self._handle_unexpected_unmount)
            return False
        return True

    # ── Mount flow ────────────────────────────────────────────────────────────
    def _start_mount(self):
        self.state = 'mounting'
        self._set_icon('mounting')
        self._set_tooltip(f'Reading disk on {self.port}…')
        threading.Thread(target=self._mount_thread, daemon=True).start()

    def _mount_thread(self):
        # Always unmount any stale FUSE mountpoint before (re)creating it.
        # A broken FUSE connection makes stat() fail, so do this before makedirs.
        do_unmount(MOUNTPOINT)
        try:
            os.rmdir(MOUNTPOINT)
        except Exception:
            pass
        try:
            os.makedirs(MOUNTPOINT, exist_ok=True)
        except Exception as e:
            GLib.idle_add(self._mount_error, f'Cannot create mountpoint: {e}')
            return

        # Launch waffle-fuse with full autodetect (no format/density forced)
        try:
            proc = subprocess.Popen(
                [WAFFLE_BIN, self.port, MOUNTPOINT],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, bufsize=1
            )
        except FileNotFoundError:
            GLib.idle_add(self._mount_error, 'waffle-fuse binary not found')
            return

        self.waffle_proc = proc

        # Wait for mountpoint to become active (up to 90 s)
        for _ in range(90):
            if is_mounted(MOUNTPOINT):
                break
            if proc.poll() is not None:
                output = proc.stdout.read()
                GLib.idle_add(self._mount_error,
                              f'Driver exited. {output.strip()}')
                return
            time.sleep(1)

        if not is_mounted(MOUNTPOINT):
            proc.kill()
            GLib.idle_add(self._mount_error, 'Timed out waiting for mount.')
            return

        # Trigger waffle_init by listing the root (up to 60 s)
        disk_info = ''
        for _ in range(60):
            try:
                os.listdir(MOUNTPOINT)
                # Collect any output lines waffle-fuse has written so far
                # (non-blocking peek via a reader thread)
                disk_info = self._read_proc_output(proc, timeout=2)
                break
            except OSError:
                if proc.poll() is not None:
                    output = proc.stdout.read()
                    GLib.idle_add(self._mount_error,
                                  f'Format mismatch? {output.strip()}')
                    return
                time.sleep(1)
        else:
            proc.kill()
            GLib.idle_add(self._mount_error,
                          'Filesystem init timed out.')
            return

        GLib.idle_add(self._mount_ok, disk_info)

    def _read_proc_output(self, proc, timeout=2):
        """Read available output from proc.stdout with a deadline."""
        lines = []
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                line = proc.stdout.readline()
                if line:
                    lines.append(line.strip())
            except Exception:
                break
        return ' | '.join(lines)

    def _mount_ok(self, raw_output):
        self.state = 'mounted'
        self._set_icon('mounted')

        # Parse waffle-fuse output for human-readable info
        info = self._parse_disk_info(raw_output)
        self.disk_info = info
        self._set_tooltip(f'Disk mounted: {info}  –  right-click for menu')
        notify('waffle-fuse', f'Disk mounted: {info}')
        open_file_manager(MOUNTPOINT)

        # Start health-check timer
        GLib.timeout_add(CHECK_INTERVAL_MS, self._watch_mounted_tick)
        return False

    def _mount_error(self, msg):
        self.state    = 'idle'
        self.port     = None
        self.waffle_proc = None
        self._probing = False
        self._set_icon('idle')
        self._set_tooltip('waffle-fuse: waiting for disk…')
        self.disk_info = ''
        notify('waffle-fuse: mount failed', msg, error=True)
        do_unmount(MOUNTPOINT)
        try:
            os.rmdir(MOUNTPOINT)
        except Exception:
            pass
        # Re-arm idle poll
        GLib.timeout_add(POLL_INTERVAL_MS, self._poll_tick)
        return False

    def _parse_disk_info(self, raw):
        """Extract human-readable info from waffle-fuse stdout lines."""
        fmt, hd, how = '', '', ''
        for line in raw.splitlines():
            if 'disk opened' in line:
                if 'Amiga/ADF' in line: fmt = 'Amiga'
                elif 'IBM/FAT'  in line: fmt = 'PC/FAT'
                hd = 'HD' if 'HD=yes' in line else 'DD'
            if 'mounted as' in line:
                # e.g. "mounted as Amiga OFS (read-write)"
                how = line.split('mounted as', 1)[-1].strip()
        if how:
            return f'{how}  [{hd}]' if hd else how
        if fmt:
            return f'{fmt} {hd}'
        return 'Disk mounted'

    # ── Eject flow ────────────────────────────────────────────────────────────
    def _on_eject(self, _widget=None):
        if self.state != 'mounted':
            return
        self.state = 'ejecting'
        self._set_icon('idle')
        self._set_tooltip('Ejecting…')
        threading.Thread(target=self._eject_thread, daemon=True).start()

    def _eject_thread(self):
        try:
            subprocess.run(['sync'], timeout=15,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass
        do_unmount(MOUNTPOINT)
        try:
            os.rmdir(MOUNTPOINT)
        except Exception:
            pass
        if self.waffle_proc:
            try:
                self.waffle_proc.wait(timeout=5)
            except Exception:
                self.waffle_proc.kill()
        GLib.idle_add(self._ejected)

    def _ejected(self):
        self.state    = 'idle'
        self.port     = None
        self.waffle_proc = None
        self.disk_info   = ''
        self._probing    = False
        self._set_icon('idle')
        self._set_tooltip('waffle-fuse: waiting for disk…')
        notify('waffle-fuse', 'Disk ejected. Safe to remove.')
        GLib.timeout_add(POLL_INTERVAL_MS, self._poll_tick)
        return False

    def _handle_unexpected_unmount(self):
        self.state    = 'idle'
        self.port     = None
        self.waffle_proc = None
        self.disk_info   = ''
        self._probing    = False
        self._set_icon('idle')
        self._set_tooltip('waffle-fuse: waiting for disk…')
        notify('waffle-fuse', 'Disk disconnected unexpectedly.', error=True)
        try:
            os.rmdir(MOUNTPOINT)
        except Exception:
            pass
        GLib.timeout_add(POLL_INTERVAL_MS, self._poll_tick)
        return False

    # ── Quit ──────────────────────────────────────────────────────────────────
    def _on_quit(self, _widget=None):
        if self.state == 'mounted':
            self._eject_thread()
        self.tray.set_visible(False)
        Gtk.main_quit()

    def run(self):
        signal.signal(signal.SIGTERM, lambda *_: self._on_quit())
        signal.signal(signal.SIGINT,  lambda *_: self._on_quit())
        Gtk.main()


# ── Entry point ───────────────────────────────────────────────────────────────

def _startup_cleanup():
    """Kill orphaned waffle-fuse processes and unmount stale mountpoints."""
    import glob as _glob
    # Find and kill any waffle-fuse processes holding our candidates
    for port in SERIAL_CANDIDATES:
        try:
            lsof = subprocess.run(
                ['lsof', '-t', port],
                capture_output=True, text=True, timeout=5
            )
            for pid_str in lsof.stdout.split():
                try:
                    pid = int(pid_str)
                    os.kill(pid, 9)
                except (ValueError, ProcessLookupError):
                    pass
        except Exception:
            pass
    # Unmount stale FUSE mountpoint
    do_unmount(MOUNTPOINT)
    try:
        os.rmdir(MOUNTPOINT)
    except Exception:
        pass

if __name__ == '__main__':
    import atexit, fcntl

    # Prevent double-start
    LOCK = '/tmp/waffle-daemon.lock'
    lockfile = open(LOCK, 'w')
    try:
        fcntl.flock(lockfile, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        print('waffle-daemon is already running.', file=sys.stderr)
        sys.exit(1)

    _startup_cleanup()

    daemon = WaffleDaemon()

    def _atexit_cleanup():
        """Ensure waffle-fuse subprocess is killed on any exit."""
        if daemon.waffle_proc and daemon.waffle_proc.poll() is None:
            try:
                daemon.waffle_proc.kill()
                daemon.waffle_proc.wait(timeout=3)
            except Exception:
                pass
        do_unmount(MOUNTPOINT)
        try:
            os.rmdir(MOUNTPOINT)
        except Exception:
            pass
        try:
            lockfile.close()
            os.unlink(LOCK)
        except Exception:
            pass

    atexit.register(_atexit_cleanup)

    daemon.run()
    _atexit_cleanup()
