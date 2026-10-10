#!/usr/bin/python3
"""
waffle-diskflashback – system tray daemon for the DrawBridge/Waffle floppy reader.

Makes the drive behave like an Amiga disk drive on the Linux desktop:

  * insert a disk  -> it is mounted automatically under ~/Floppy/<volume name>
                      and shows up as a volume icon on the desktop / file manager
  * eject          -> tray menu "Eject", or "Unmount" on the desktop icon;
                      pending changes are written, then "safe to remove"
  * take it out    -> if it was still mounted it is unmounted automatically

States:
  no-port    no DrawBridge found, polling for the serial port
  waiting    port open, waiting for a disk (waffle-fuse --wait-disk)
  mounting   waffle-fuse reading the disk
  mounted    disk mounted
  ejecting   unmount requested, waiting for pending writes
  remove     disk unmounted (or unreadable): waiting for it to be taken out
"""

import argparse
import glob
import os
import signal
import subprocess
import sys
import threading
import warnings

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GLib  # noqa: E402

# Gtk.StatusIcon is deprecated but still the simplest tray icon on MATE/XFCE
warnings.filterwarnings('ignore', category=DeprecationWarning)

# ── Paths ─────────────────────────────────────────────────────────────────────
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
ICON_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, '..', 'icons'))


def find_waffle_bin():
    local = os.path.normpath(os.path.join(SCRIPT_DIR, '..', 'waffle-fuse'))
    if os.access(local, os.X_OK):
        return local
    for d in os.environ.get('PATH', '').split(os.pathsep):
        p = os.path.join(d, 'waffle-fuse')
        if os.access(p, os.X_OK):
            return p
    return local


WAFFLE_BIN = find_waffle_bin()

ICONS = {
    'idle':     os.path.join(ICON_DIR, 'tray-idle.svg'),
    'mounting': os.path.join(ICON_DIR, 'tray-mounting.svg'),
    'mounted':  os.path.join(ICON_DIR, 'tray-mounted.svg'),
}

# waffle-fuse exit codes
EXIT_OK, EXIT_PORT, EXIT_NODISK, EXIT_UNKNOWN_FMT, EXIT_DRIVE = 0, 2, 3, 4, 5
EXIT_FILESYSTEM, EXIT_REMOVED, EXIT_REMOVED_DIRTY, EXIT_WRITE_ERROR = 6, 7, 8, 9


# ── Helpers ───────────────────────────────────────────────────────────────────
def notify(title, msg, error=False):
    icon = 'dialog-error' if error else 'media-floppy'
    try:
        subprocess.Popen(['notify-send', '-a', 'Waffle', '-i', icon, title, msg],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except FileNotFoundError:
        print(f'{title}: {msg}', file=sys.stderr)


def open_file_manager(path):
    for cmd in (['xdg-open', path], ['caja', path], ['nautilus', path],
                ['thunar', path], ['dolphin', path], ['pcmanfm', path]):
        try:
            subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            return
        except FileNotFoundError:
            continue


def candidate_ports():
    """Serial ports that could be a DrawBridge, stable by-id names first."""
    seen, ports = set(), []
    for p in sorted(glob.glob('/dev/serial/by-id/*')) + \
            sorted(glob.glob('/dev/ttyUSB*')) + sorted(glob.glob('/dev/ttyACM*')):
        real = os.path.realpath(p)
        if real not in seen and os.path.exists(real):
            seen.add(real)
            ports.append(p)
    return ports


def parse_event(line):
    """'WAFFLE event=mounted mountpoint=/x y' -> {'event': 'mounted', 'mountpoint': '/x y'}"""
    if not line.startswith('WAFFLE '):
        return None
    fields = {}
    rest = line[7:].rstrip('\n')
    while rest:
        key, _, rest = rest.partition('=')
        if key in ('volume', 'mountpoint'):   # always last: the value may contain anything
            fields[key] = rest
            break
        val, _, rest = rest.partition(' ')
        fields[key] = val
    return fields


class Proc:
    """A waffle-fuse child whose stdout events and exit are delivered on the GTK loop."""

    def __init__(self, args, on_event=None, on_exit=None):
        self.p = subprocess.Popen([WAFFLE_BIN] + args, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True, bufsize=1)
        self.on_event, self.on_exit = on_event, on_exit
        self.stderr_tail = []
        self.cancelled = False
        threading.Thread(target=self._stderr, daemon=True).start()
        threading.Thread(target=self._stdout, daemon=True).start()

    def _stderr(self):
        for line in self.p.stderr:
            self.stderr_tail = (self.stderr_tail + [line.strip()])[-5:]

    def _stdout(self):
        for line in self.p.stdout:
            ev = parse_event(line)
            if ev and self.on_event:
                GLib.idle_add(self._deliver_event, ev)
        code = self.p.wait()
        GLib.idle_add(self._deliver_exit, code)

    def _deliver_event(self, ev):
        if not self.cancelled:
            self.on_event(ev)
        return False

    def _deliver_exit(self, code):
        if not self.cancelled and self.on_exit:
            self.on_exit(code)
        return False

    def cancel(self):
        """Stop a waiting helper (never used on a mounted driver)."""
        self.cancelled = True
        if self.p.poll() is None:
            self.p.terminate()


# ── Daemon ────────────────────────────────────────────────────────────────────
class WaffleDaemon:
    def __init__(self, args):
        self.args = args
        self.mount_dir = os.path.expanduser(args.mount_dir)
        self.state = None
        self.port = args.port
        self.proc = None          # current waffle-fuse child
        self.driver = None        # the mounted waffle-fuse (subset of proc)
        self.mountpoint = None
        self.info = ''

        self.tray = Gtk.StatusIcon()
        self.tray.set_visible(True)
        self.tray.connect('activate', self._on_activate)
        self.tray.connect('popup-menu', self._on_popup)

        self._cleanup_stale_mounts()
        self._find_port()

    # ── UI ────────────────────────────────────────────────────────────────────
    def _set_state(self, state, tooltip, icon):
        self.state = state
        self.tray.set_tooltip_text(tooltip)
        try:
            self.tray.set_from_file(ICONS[icon])
        except Exception:
            self.tray.set_from_icon_name('media-floppy')

    def _on_activate(self, _icon):
        if self.state == 'mounted' and self.mountpoint:
            open_file_manager(self.mountpoint)

    def _on_popup(self, icon, button, ts):
        menu = Gtk.Menu()

        def item(label, cb=None, sensitive=True):
            mi = Gtk.MenuItem(label=label)
            mi.set_sensitive(sensitive and cb is not None)
            if cb:
                mi.connect('activate', lambda _w: cb())
            menu.append(mi)

        item(self.info or self.tray.get_tooltip_text())
        menu.append(Gtk.SeparatorMenuItem())
        mounted = self.state == 'mounted'
        item('Open', lambda: open_file_manager(self.mountpoint), mounted)
        item('Eject', self._eject, mounted)
        item('Mount again', self._remount, self.state == 'remove')
        menu.append(Gtk.SeparatorMenuItem())
        item('Quit', self._quit)
        menu.show_all()
        menu.popup(None, None, Gtk.StatusIcon.position_menu, icon, button, ts)

    # ── Port discovery ────────────────────────────────────────────────────────
    def _find_port(self):
        self._set_state('no-port', 'Waffle: looking for the DrawBridge…', 'idle')
        self.info = ''
        if self.port:
            self._wait_disk()
            return
        threading.Thread(target=self._probe_ports, daemon=True).start()

    def _probe_ports(self):
        for port in candidate_ports():
            try:
                r = subprocess.run([WAFFLE_BIN, '--probe', port], capture_output=True, timeout=20)
            except (OSError, subprocess.TimeoutExpired):
                continue
            if r.returncode in (0, 1):     # answered: it is a DrawBridge
                GLib.idle_add(self._port_found, port)
                return
        GLib.timeout_add_seconds(3, self._retry_find_port)

    def _retry_find_port(self):
        if self.state == 'no-port':
            self._find_port()
        return False

    def _port_found(self, port):
        if self.state == 'no-port':
            self.port = port
            self._wait_disk()
        return False

    # ── Waiting for a disk ────────────────────────────────────────────────────
    def _wait_disk(self):
        self._set_state('waiting', f'Waffle: insert a disk ({os.path.basename(self.port)})', 'idle')
        self.info = ''
        self.proc = Proc(['--wait-disk', self.port], on_exit=self._wait_disk_done)

    def _wait_disk_done(self, code):
        self.proc = None
        if code == 0:
            self._mount()
        else:
            # Port gone or busy (e.g. WaffleCopyPRO is using it): look again later
            if not self.args.port:
                self.port = None
            GLib.timeout_add_seconds(3, self._retry_after_port_error)

    def _retry_after_port_error(self):
        if self.state == 'waiting':
            self._find_port()
        return False

    # ── Mounting ──────────────────────────────────────────────────────────────
    def _mount(self):
        self._set_state('mounting', 'Waffle: reading disk…', 'mounting')
        args = [self.port, '--automount', self.mount_dir, '-f']
        if self.args.ro:
            args += ['-o', 'ro']
        self.proc = self.driver = Proc(args, on_event=self._driver_event, on_exit=self._driver_exit)

    def _driver_event(self, ev):
        name = ev.get('event')
        if name == 'detected':
            vol = ev.get('volume') or 'unnamed'
            kind = 'Amiga' if ev.get('format') == 'amiga' else 'PC'
            ro = ', read-only' if ev.get('readonly') == '1' else ''
            self.info = f'{vol} – {kind} {ev.get("fs", "")} {ev.get("density", "")}{ro}'
        elif name == 'mounted':
            self.mountpoint = ev.get('mountpoint')
            self._set_state('mounted', f'Waffle: {self.info}', 'mounted')
            notify('Disk inserted', self.info)
            if self.args.open:
                open_file_manager(self.mountpoint)
        elif name == 'write-error':
            notify('Disk write error',
                   'Some changes could not be written. Do not remove the disk; '
                   'check that it is not write-protected or damaged.', error=True)

    def _driver_exit(self, code):
        stderr_tail = self.driver.stderr_tail if self.driver else []
        self.proc = self.driver = None
        self.mountpoint = None
        if code == EXIT_OK:
            notify('Disk ejected', 'All changes written: the disk can be removed.')
            self._wait_removal()
        elif code == EXIT_REMOVED:
            notify('Disk removed', self.info or 'The disk was unmounted.')
            self._wait_disk()
        elif code == EXIT_REMOVED_DIRTY:
            notify('Disk removed too early',
                   'The disk was taken out while changes were still being written: '
                   'they are lost.', error=True)
            self._wait_disk()
        elif code == EXIT_NODISK:
            self._wait_disk()
        elif code == EXIT_PORT:
            self._find_port()
        else:
            msgs = {
                EXIT_UNKNOWN_FMT: 'Not a DOS disk: unformatted, NDOS (game) or copy-protected.',
                EXIT_DRIVE: 'Cannot read track 0: the disk may be damaged.',
                EXIT_FILESYSTEM: 'The filesystem on the disk is not valid.',
                EXIT_WRITE_ERROR: 'Some changes could not be written to the disk.',
            }
            detail = msgs.get(code, ' '.join(stderr_tail) or f'error {code}')
            notify('Cannot use this disk', detail, error=True)
            self.info = detail
            self._wait_removal()

    # ── Eject ─────────────────────────────────────────────────────────────────
    def _eject(self):
        if self.state != 'mounted' or not self.mountpoint:
            return
        r = subprocess.run(['fusermount3', '-u', self.mountpoint], capture_output=True, text=True)
        if r.returncode != 0:
            notify('Disk in use',
                   'Close the windows and terminals that are using the disk, then eject again.',
                   error=True)
            return
        # waffle-fuse now writes any pending changes and exits (see _driver_exit)
        self._set_state('ejecting', 'Waffle: writing changes, do not remove the disk…', 'mounting')

    # ── After eject: wait for the disk to be taken out ────────────────────────
    def _wait_removal(self):
        self._set_state('remove', f'Waffle: remove the disk ({self.info})' if self.info
                        else 'Waffle: remove the disk', 'idle')
        self.proc = Proc(['--wait-removal', self.port], on_exit=self._removal_done)

    def _removal_done(self, code):
        self.proc = None
        if code == 0:
            self._wait_disk()
        else:
            GLib.timeout_add_seconds(3, self._retry_after_port_error_remove)

    def _retry_after_port_error_remove(self):
        if self.state == 'remove':
            self._find_port()
        return False

    def _remount(self):
        if self.state != 'remove':
            return
        if self.proc:
            self.proc.cancel()
            try:
                self.proc.p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.p.kill()
            self.proc = None
        self._mount()

    # ── Startup / quit ────────────────────────────────────────────────────────
    def _cleanup_stale_mounts(self):
        """Remove mountpoints left behind by a crashed driver (ENOTCONN)."""
        if not os.path.isdir(self.mount_dir):
            return
        for name in os.listdir(self.mount_dir):
            path = os.path.join(self.mount_dir, name)
            try:
                os.stat(path)
            except OSError:
                subprocess.run(['fusermount3', '-u', '-z', path], capture_output=True)
            try:
                os.rmdir(path)   # only succeeds for empty, unmounted directories
            except OSError:
                pass

    def _quit(self):
        if self.driver and self.mountpoint:
            # Unmount cleanly so pending writes reach the disk
            subprocess.run(['fusermount3', '-u', self.mountpoint], capture_output=True)
            try:
                self.driver.p.wait(timeout=60)
            except subprocess.TimeoutExpired:
                pass
        elif self.proc:
            self.proc.cancel()
        Gtk.main_quit()


def main():
    ap = argparse.ArgumentParser(description='Automount floppies from a DrawBridge/Waffle drive')
    ap.add_argument('--port', help='serial port (default: autodetect)')
    ap.add_argument('--mount-dir', default='~/Floppy',
                    help='where disks are mounted, as <dir>/<volume name> (default ~/Floppy). '
                         'Must be inside your home directory to get desktop icons.')
    ap.add_argument('--open', action='store_true', help='open a file manager window on insert')
    ap.add_argument('--ro', action='store_true', help='always mount read-only')
    args = ap.parse_args()

    import fcntl
    lock_path = os.path.join(os.environ.get('XDG_RUNTIME_DIR', '/tmp'), 'waffle-diskflashback.lock')
    lock = open(lock_path, 'w')
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        print('waffle-diskflashback is already running.', file=sys.stderr)
        sys.exit(1)

    daemon = WaffleDaemon(args)
    for sig in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
        GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, sig, lambda *_: daemon._quit() or False)
    Gtk.main()


if __name__ == '__main__':
    main()
