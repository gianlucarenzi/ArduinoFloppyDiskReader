#!/usr/bin/python3
"""
waffle-diskflashback – system tray daemon for the DrawBridge/Waffle floppy reader.

Makes the drive behave like an Amiga disk drive on the Linux desktop:

  * insert a disk  -> it is mounted automatically where udisks would mount it,
                      /media/$USER/<volume> or /run/media/$USER/<volume> (needs
                      scripts/install-system.sh once), else ~/Floppy/<volume>
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

import importlib

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GLib  # noqa: E402

# Gtk.StatusIcon (fallback tray on X11) is deprecated: keep the log quiet
warnings.filterwarnings('ignore', category=DeprecationWarning)


def load_appindicator():
    """StatusNotifierItem support: works on Wayland and X11 (KDE, XFCE, MATE,
    Cinnamon, GNOME with the AppIndicator extension, waybar...)."""
    for ns in ('AyatanaAppIndicator3', 'AppIndicator3'):
        try:
            gi.require_version(ns, '0.1')
            return importlib.import_module('gi.repository.' + ns)
        except (ValueError, ImportError):
            continue
    return None

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


def default_mount_dir():
    """Where udisks mounts removable media - /media/$USER (Debian, Ubuntu) or
    /run/media/$USER (Arch, Fedora...) - when we may create directories there
    (see scripts/install-system.sh), else ~/Floppy."""
    user = os.environ.get('USER') or os.getlogin()
    for base in ('/run/media', '/media'):
        d = os.path.join(base, user)
        if os.path.isdir(d) and os.access(d, os.W_OK | os.X_OK):
            return d
    return os.path.expanduser('~/Floppy')


FLOPPY_IMAGE_SIZES = {901120, 1802240, 737280, 1474560}


def unescape_mount_field(text):
    """/proc/mounts writes space, tab, newline and backslash as octal escapes."""
    import re
    return re.sub(r'\\([0-7]{3})', lambda m: chr(int(m.group(1), 8)), text)


def mounted_images():
    """[(image file, mountpoint)] of the floppy images open as virtual disks."""
    out = []
    try:
        with open('/proc/mounts') as f:
            for line in f:
                src, mnt = (unescape_mount_field(x) for x in line.split()[:2])
                if src.startswith('waffle:') and os.path.isfile(src[7:]):
                    out.append((src[7:], mnt))
    except OSError:
        pass
    return out


def image_driver_running(image):
    """Is a waffle-fuse --image process still serving this file?  (Exact
    argument match: file names may contain regex characters.)"""
    for pid in os.listdir('/proc'):
        if not pid.isdigit():
            continue
        try:
            with open(f'/proc/{pid}/cmdline', 'rb') as f:
                argv = f.read().split(b'\0')
        except OSError:
            continue
        if argv and argv[0].endswith(b'waffle-fuse') and b'--image' in argv and \
                os.fsencode(image) in argv:
            return True
    return False


def unmount_and_wait(mountpoint, image, timeout=30):
    """Unmount a virtual disk and wait until its driver has written everything."""
    if subprocess.run(['fusermount3', '-u', mountpoint], capture_output=True).returncode != 0:
        return False
    import time
    deadline = time.time() + timeout
    while time.time() < deadline and image_driver_running(image):
        time.sleep(0.2)
    return True


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


# ── Tray icon ─────────────────────────────────────────────────────────────────
class Tray:
    """AppIndicator (StatusNotifierItem) when available, else Gtk.StatusIcon on
    X11 (XEmbed trays), else no icon at all: the daemon then works with
    notifications only."""

    def __init__(self, build_menu, on_activate):
        self.build_menu = build_menu
        self.on_activate = on_activate
        self.kind = None
        ai = load_appindicator()
        if ai:
            self.ai = ai
            self.ind = ai.Indicator.new('waffle-diskflashback', ICONS['idle'],
                                        ai.IndicatorCategory.HARDWARE)
            self.ind.set_status(ai.IndicatorStatus.ACTIVE)
            self.ind.set_title('Waffle')
            self.kind = 'indicator'
            self.refresh_menu()
        elif os.environ.get('XDG_SESSION_TYPE') != 'wayland' and os.environ.get('DISPLAY'):
            self.icon = Gtk.StatusIcon()
            self.icon.set_visible(True)
            self.icon.connect('activate', lambda _i: self.on_activate())
            self.icon.connect('popup-menu', self._popup)
            self.kind = 'statusicon'

    def set(self, tooltip, icon):
        path = ICONS.get(icon, ICONS['idle'])
        if self.kind == 'indicator':
            if os.path.exists(path):
                self.ind.set_icon_full(path, tooltip)
            else:
                self.ind.set_icon_full('media-floppy', tooltip)
            self.refresh_menu()
        elif self.kind == 'statusicon':
            self.icon.set_tooltip_text(tooltip)
            if os.path.exists(path):
                self.icon.set_from_file(path)
            else:
                self.icon.set_from_icon_name('media-floppy')

    def refresh_menu(self):
        # An indicator has no click callback: the menu is the whole UI
        if self.kind == 'indicator':
            menu = self.build_menu()
            menu.show_all()
            self.ind.set_menu(menu)

    def _popup(self, icon, button, ts):
        menu = self.build_menu()
        menu.show_all()
        menu.popup(None, None, Gtk.StatusIcon.position_menu, icon, button, ts)


# ── Daemon ────────────────────────────────────────────────────────────────────
class WaffleDaemon:
    def __init__(self, args, have_display=True):
        self.args = args
        self.mount_dir = os.path.expanduser(args.mount_dir or default_mount_dir())
        self.state = None
        self.port = args.port
        self.proc = None          # current waffle-fuse child
        self.driver = None        # the mounted waffle-fuse (subset of proc)
        self.mountpoint = None
        self.info = ''
        self.tooltip = ''
        self.volume = ''              # volume name / label of the current disk
        self.fs_kind = ''             # 'amiga' or 'fat'
        self.job = None               # disk job (dump / write image) in progress or queued
        self.job_win = None

        self.tray = Tray(self._build_menu, self._on_activate) if have_display else None

        self._cleanup_stale_mounts()
        self._find_port()

    # ── UI ────────────────────────────────────────────────────────────────────
    def _set_state(self, state, tooltip, icon):
        self.state = state
        self.tooltip = tooltip
        if self.tray:
            self.tray.set(tooltip, icon)

    def _on_activate(self):
        if self.state == 'mounted' and self.mountpoint:
            open_file_manager(self.mountpoint)

    def _build_menu(self):
        menu = Gtk.Menu()

        def item(label, cb=None, sensitive=True):
            mi = Gtk.MenuItem(label=label)
            mi.set_sensitive(sensitive and cb is not None)
            if cb:
                mi.connect('activate', lambda _w: cb())
            menu.append(mi)

        item(self.info or self.tooltip or 'Waffle')
        menu.append(Gtk.SeparatorMenuItem())
        mounted = self.state == 'mounted'
        item('Open', lambda: open_file_manager(self.mountpoint), mounted)
        item('Eject', self._eject, mounted)
        item('Mount again', self._remount, self.state == 'remove')
        disk_in = self.state in ('mounted', 'remove')
        item('Copy disk to image file…', self._dump_ask, disk_in)
        item('Write image to disk…', self._write_ask, disk_in)
        menu.append(Gtk.SeparatorMenuItem())
        item('Quit', self._quit)
        return menu

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
            self.volume = ev.get('volume') or ''
            self.fs_kind = ev.get('format', '')
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
        if code == EXIT_OK and self.job:
            self._start_job()               # unmounted to free the port for a disk job
        elif code == EXIT_OK:
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

    def _stop_helper(self):
        """Stop a waiting helper (--wait-removal / --wait-disk) to free the port."""
        if self.proc and self.proc is not self.driver:
            self.proc.cancel()
            try:
                self.proc.p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.p.kill()
            self.proc = None

    def _remount(self):
        if self.state != 'remove':
            return
        self._stop_helper()
        self._mount()

    # ── Disk jobs: copy the disk to an image file / write an image to the disk ─
    def _file_dialog(self, title, action, button, filters):
        dlg = Gtk.FileChooserDialog(title=title, action=action)
        dlg.add_buttons('Cancel', Gtk.ResponseType.CANCEL, button, Gtk.ResponseType.ACCEPT)
        for name, patterns in filters:
            flt = Gtk.FileFilter()
            flt.set_name(name)
            for p in patterns:
                flt.add_pattern(p)
                flt.add_pattern(p.upper())
            dlg.add_filter(flt)
        folder = os.path.expanduser('~/WaffleFolder')
        dlg.set_current_folder(folder if os.path.isdir(folder) else os.path.expanduser('~'))
        return dlg

    def _dump_ask(self):
        if self.state not in ('mounted', 'remove'):
            return
        dlg = self._file_dialog('Copy disk to image file', Gtk.FileChooserAction.SAVE, 'Copy',
                                (('Sector image (.adf, .img)', ['*.adf', '*.img', '*.ima']),
                                 ('Flux image for copy-protected disks (.scp)', ['*.scp'])))
        dlg.set_do_overwrite_confirmation(True)
        base = self.volume or ('floppy' if self.fs_kind == 'fat' else 'disk')
        ext = '.img' if self.fs_kind == 'fat' else '.adf'
        dlg.set_current_name(base + ext)
        dlg.set_extra_widget(Gtk.Label(label='.adf / .img: copy of the sectors (DOS and NDOS disks)\n'
                                             '.scp: raw magnetic flux, also for copy-protected disks',
                                       xalign=0))
        resp = dlg.run()
        path = dlg.get_filename()
        dlg.destroy()
        if resp != Gtk.ResponseType.ACCEPT or not path:
            return
        if not os.path.splitext(path)[1]:
            path += ext
        self._queue_job({'kind': 'dump', 'path': path})

    def _write_ask(self):
        if self.state not in ('mounted', 'remove'):
            return
        images = mounted_images()
        dlg = self._file_dialog('Write image to disk', Gtk.FileChooserAction.OPEN, 'Next',
                                (('Floppy images (.adf, .img, .ima)', ['*.adf', '*.img', '*.ima']),))
        if images:                      # propose the image that is open as a virtual disk
            dlg.set_filename(images[0][0])
        resp = dlg.run()
        path = dlg.get_filename()
        dlg.destroy()
        if resp != Gtk.ResponseType.ACCEPT or not path:
            return
        try:
            size = os.path.getsize(path)
        except OSError as e:
            notify('Cannot write image', f'{os.path.basename(path)}: {e.strerror}', error=True)
            return
        if size not in FLOPPY_IMAGE_SIZES:
            notify('Cannot write image', f'{os.path.basename(path)} is not an 880K/1.76M ADF or a '
                                         '720K/1.44M PC floppy image.', error=True)
            return

        # Confirm: everything on the real disk is overwritten
        target = self.volume or 'the disk in the drive'
        dlg = Gtk.MessageDialog(message_type=Gtk.MessageType.WARNING, buttons=Gtk.ButtonsType.NONE,
                                text=f'Write {os.path.basename(path)} to {target}?')
        dlg.format_secondary_text('Everything on the disk in the drive will be overwritten. '
                                  'Only the tracks that differ are rewritten, each one verified.')
        full = Gtk.CheckButton(label='Erase and rewrite every track (slower, like formatting)')
        dlg.get_message_area().pack_start(full, False, False, 0)
        dlg.add_buttons('Cancel', Gtk.ResponseType.CANCEL, 'Write', Gtk.ResponseType.ACCEPT)
        dlg.show_all()
        resp = dlg.run()
        erase = full.get_active()
        dlg.destroy()
        if resp != Gtk.ResponseType.ACCEPT:
            return

        # Close the image if it is open as a virtual disk, so every change is in the file
        for img, mnt in images:
            if img == os.path.realpath(path):
                if not unmount_and_wait(mnt, img):
                    notify('Image in use', 'Close the windows and terminals using the virtual disk '
                                           f'{os.path.basename(mnt)}, then try again.', error=True)
                    return
        self._queue_job({'kind': 'write', 'path': path, 'full': erase})

    def _queue_job(self, job):
        """Free the port (unmount the real disk or stop the waiting helper), then run the job."""
        if self.state not in ('mounted', 'remove'):
            return                          # the disk went away meanwhile
        self.job = job
        # Remount afterwards: the same disk after a dump, the new content after a write
        job['remount'] = self.state == 'mounted' or job['kind'] == 'write'
        if self.state == 'mounted':
            r = subprocess.run(['fusermount3', '-u', self.mountpoint], capture_output=True, text=True)
            if r.returncode != 0:
                self.job = None
                notify('Disk in use', 'Close the windows and terminals using the disk, then try again.',
                       error=True)
                return
            self._set_state('ejecting', 'Waffle: writing pending changes first…', 'mounting')
            # _driver_exit starts the job once the driver has released the port
        else:
            self._stop_helper()
            self._start_job()

    def _start_job(self):
        job = self.job
        name = os.path.basename(job['path'])
        job['tracks'] = 0
        if job['kind'] == 'dump':
            self._set_state('busy', f'Waffle: copying the disk to {name}…', 'mounting')
            self._job_window('Copying disk', f'Copying the disk to\n{job["path"]}')
            # Into a temporary file: an existing file is only replaced by a finished copy
            job['tmp'] = job['path'] + '.part' + os.path.splitext(job['path'])[1]
            args = ['--dump', self.port, job['tmp']]
        else:
            self._set_state('busy', f'Waffle: writing {name} to the disk…', 'mounting')
            self._job_window('Writing image to disk', f'Writing\n{job["path"]}\nto the disk in the drive')
            args = ['--write-image', self.port, job['path'], '--yes'] + (['--full'] if job['full'] else [])
        self.proc = Proc(args, on_event=self._job_event, on_exit=self._job_exit)

    def _job_window(self, title, text):
        win = Gtk.Window(title=title)
        win.set_default_size(420, -1)
        win.set_border_width(12)
        win.set_keep_above(True)
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
        win.add(box)
        lbl = Gtk.Label(label=text, xalign=0)
        lbl.set_line_wrap(True)
        box.pack_start(lbl, False, False, 0)
        bar = Gtk.ProgressBar(show_text=True)
        box.pack_start(bar, False, False, 0)
        cancel = Gtk.Button(label='Cancel')
        cancel.connect('clicked', lambda _b: self._job_cancel())
        btns = Gtk.Box()
        btns.pack_end(cancel, False, False, 0)
        box.pack_start(btns, False, False, 0)
        win.connect('delete-event', lambda *_: True)   # close only through Cancel
        win.show_all()
        self.job_win, self.job_bar = win, bar

    def _job_event(self, ev):
        job = self.job
        if not job:
            return
        name = ev.get('event')
        if name == 'track':
            job['tracks'] += 1
            self.job_bar.set_fraction(min(1.0, job['tracks'] / 160))
            status = ev.get('status', '')
            self.job_bar.set_text(f'cylinder {ev.get("cylinder")}  head {ev.get("head")}'
                                  + (f'  ({status})' if status in ('unchanged', 'failed', 'bad') else ''))
        elif name == 'dumped':
            job['bad'] = int(ev.get('bad', '0') or 0)
        elif name == 'written':
            job['written'] = int(ev.get('tracks', '0') or 0)

    def _job_cancel(self):
        if self.proc and self.job and self.state == 'busy':
            self.job['cancelled'] = True
            self.proc.p.terminate()          # waffle-fuse stops between two tracks

    def _job_exit(self, code):
        job, self.job = self.job, None
        stderr_tail = self.proc.stderr_tail if self.proc else []
        self.proc = None
        if self.job_win:
            self.job_win.destroy()
            self.job_win = None
        path, name = job['path'], os.path.basename(job['path'])
        cancelled = job.get('cancelled', False)

        if job['kind'] == 'dump':
            tmp = job['tmp']
            if os.path.exists(tmp) and code in (EXIT_OK, EXIT_DRIVE) and not cancelled:
                os.replace(tmp, path)
            else:
                try:
                    os.remove(tmp)           # never leave a half-written image around
                except OSError:
                    pass
            if cancelled:
                notify('Copy cancelled', name)
            elif code == EXIT_OK:
                notify('Disk copied', path)
            elif code == EXIT_DRIVE and job.get('bad'):
                notify('Disk copied with errors',
                       f'{path}\n{job["bad"]} sectors could not be read (saved as zeros).', error=True)
            elif code == EXIT_DRIVE and os.path.exists(path):
                notify('Disk copied with errors', f'{path}\nSome tracks had read problems.', error=True)
            else:
                notify('Copy failed', f'{name}: ' + (' '.join(stderr_tail[-1:]) or f'error {code}'), error=True)
        else:
            if cancelled:
                notify('Write stopped', 'The disk now holds part of the image only.', error=True)
            elif code == EXIT_OK:
                n = job.get('written')
                notify('Image written', f'{name} written to the disk' +
                       (f' ({n} tracks rewritten and verified).' if n is not None else '.'))
            else:
                detail = ' '.join(stderr_tail[-1:]).replace('waffle-fuse: ', '') or f'error {code}'
                notify('Write failed', f'{name}: {detail}', error=True)

        if job['remount'] and code != EXIT_NODISK:
            self._mount()
        else:
            self._wait_removal()

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
            if self.state == 'busy' and self.job:
                try:
                    self.proc.p.wait(timeout=30)     # waffle-fuse stops between two tracks
                except subprocess.TimeoutExpired:
                    pass
                if self.job.get('tmp'):
                    try:
                        os.remove(self.job['tmp'])
                    except OSError:
                        pass
        self.loop_quit()


def main():
    ap = argparse.ArgumentParser(description='Automount floppies from a DrawBridge/Waffle drive')
    ap.add_argument('--port', help='serial port (default: autodetect)')
    ap.add_argument('--mount-dir', default=None,
                    help='where disks are mounted, as <dir>/<volume name> (default /media/$USER '
                         'like udisks if writable, see scripts/install-system.sh, else ~/Floppy). '
                         'Must be under /media or your home directory to get desktop icons.')
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

    # Without a display (e.g. started too early) run with notifications only
    have_display = Gtk.init_check(sys.argv)[0] if hasattr(Gtk, 'init_check') else True
    daemon = WaffleDaemon(args, have_display)
    if have_display:
        daemon.loop_quit = Gtk.main_quit
        run = Gtk.main
    else:
        loop = GLib.MainLoop()
        daemon.loop_quit = loop.quit
        run = loop.run
    for sig in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
        GLib.unix_signal_add(GLib.PRIORITY_DEFAULT, sig, lambda *_: daemon._quit() or False)
    run()


if __name__ == '__main__':
    main()
