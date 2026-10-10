#!/usr/bin/python3
"""
waffle-installer – graphical installer for waffle-fuse (DrawBridge/Waffle floppy drive)

Works on X11 and Wayland, with any desktop (GNOME, KDE, XFCE, MATE, Cinnamon,
LXQt, sway, Hyprland, i3...) and on Debian/Ubuntu, Arch, Fedora and friends.

  * per user (no privileges): menu launchers and icons, and the tray daemon
    started with the graphical session (XDG autostart, or a systemd user
    service on window managers without XDG autostart);
  * system (asks for the administrator password through polkit/pkexec):
    mount disks under /media/$USER or /run/media/$USER like udisks, and
    serial port access (scripts/install-system.sh).

Without GTK or without a display it runs as a text installer:  --text
"""

import os
import shutil
import signal
import subprocess
import sys
import threading
import time

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
WAFFLE_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, '..'))
WAFFLE_BIN = os.path.join(WAFFLE_DIR, 'waffle-fuse')
DAEMON = os.path.join(SCRIPT_DIR, 'waffle-diskflashback.py')
SYSTEM_SCRIPT = os.path.join(SCRIPT_DIR, 'install-system.sh')
DESKTOP_SCRIPT = os.path.join(SCRIPT_DIR, 'install-desktop.sh')
TMPFILES_CONF = '/etc/tmpfiles.d/waffle-fuse.conf'

HOME = os.path.expanduser('~')
XDG_CONFIG = os.environ.get('XDG_CONFIG_HOME', os.path.join(HOME, '.config'))
AUTOSTART_FILE = os.path.join(XDG_CONFIG, 'autostart', 'waffle-diskflashback.desktop')
SYSTEMD_UNIT_DIR = os.path.join(XDG_CONFIG, 'systemd', 'user')
SYSTEMD_UNIT = 'waffle-diskflashback.service'
ICON_DST = os.path.join(HOME, '.local', 'share', 'icons', 'waffle-fuse')
DAEMON_LOG = os.path.join(HOME, '.cache', 'waffle-diskflashback.log')
USER = os.environ.get('USER') or os.getlogin()

# Window managers / compositors that do not run XDG autostart entries
NO_XDG_AUTOSTART = {'sway', 'hyprland', 'i3', 'river', 'niri', 'wayfire', 'dwm',
                    'bspwm', 'qtile', 'awesome', 'openbox', 'herbstluftwm', 'labwc'}


# ── Environment detection ─────────────────────────────────────────────────────
def distro_family():
    ids = ''
    try:
        with open('/etc/os-release') as f:
            for line in f:
                if line.startswith(('ID=', 'ID_LIKE=')):
                    ids += ' ' + line.split('=', 1)[1].strip().strip('"')
    except OSError:
        pass
    for fam in ('arch', 'fedora', 'suse', 'debian', 'ubuntu'):
        if fam in ids.split():
            return 'debian' if fam == 'ubuntu' else fam
    return 'debian' if shutil.which('apt') else 'arch' if shutil.which('pacman') else \
        'fedora' if shutil.which('dnf') else 'unknown'


PACKAGES = {  # what -> (debian, arch, fedora, suse)
    'gtk':   ('python3-gi gir1.2-gtk-3.0', 'python-gobject gtk3', 'python3-gobject gtk3', 'python3-gobject typelib-1_0-Gtk-3_0'),
    'tray':  ('gir1.2-ayatanaappindicator3-0.1', 'libayatana-appindicator', 'libayatana-appindicator-gtk3', 'typelib-1_0-AyatanaAppIndicator3-0_1'),
    'fuse':  ('fuse3', 'fuse3', 'fuse3', 'fuse3'),
    'notify': ('libnotify-bin', 'libnotify', 'libnotify', 'libnotify-tools'),
    'acl':   ('acl', 'acl', 'acl', 'acl'),
    'build': ('build-essential pkg-config libfuse3-dev', 'base-devel pkgconf fuse3', 'gcc-c++ make pkgconf fuse3-devel', 'gcc-c++ make pkg-config fuse3-devel'),
}
INSTALL_CMD = {'debian': 'sudo apt install', 'arch': 'sudo pacman -S --needed',
               'fedora': 'sudo dnf install', 'suse': 'sudo zypper install'}


def pkg_hint(what):
    fam = distro_family()
    idx = {'debian': 0, 'arch': 1, 'fedora': 2, 'suse': 3}.get(fam)
    if idx is None:
        return f'install the packages providing: {what}'
    return f'{INSTALL_CMD[fam]} {PACKAGES[what][idx]}'


def have_gi_module(ns, ver):
    try:
        r = subprocess.run(['/usr/bin/python3', '-c',
                            f"import gi; gi.require_version('{ns}', '{ver}'); from gi.repository import {ns}"],
                           capture_output=True, timeout=20)
        return r.returncode == 0
    except (OSError, subprocess.TimeoutExpired):
        return False


def udisks_base():
    for d in ('/usr/libexec/udisks2/udisksd', '/usr/lib/udisks2/udisksd',
              '/usr/libexec/udisksd', '/usr/lib/udisksd'):
        try:
            with open(d, 'rb') as f:
                data = f.read()
        except OSError:
            continue
        if b'/run/media/%s' in data:
            return '/run/media'
        if b'/media/%s' in data:
            return '/media'
    return '/run/media' if os.path.isdir('/run/media') else '/media'


def media_ready():
    d = os.path.join(udisks_base(), USER)
    return os.path.isdir(d) and os.access(d, os.W_OK | os.X_OK)


def desktop_names():
    return [d.lower() for d in os.environ.get('XDG_CURRENT_DESKTOP', '').split(':') if d]


def autostart_kind():
    """'xdg' for desktops that run ~/.config/autostart, else 'systemd'."""
    names = desktop_names()
    if not names or any(n in NO_XDG_AUTOSTART for n in names):
        if shutil.which('systemctl'):
            return 'systemd'
    return 'xdg'


def check_environment():
    """List of (ok, label, hint)."""
    items = []
    items.append((os.access(WAFFLE_BIN, os.X_OK), 'waffle-fuse built',
                  f'it will be built now (needs: {pkg_hint("build")})'))
    items.append((bool(shutil.which('fusermount3')), 'FUSE 3 (fusermount3)', pkg_hint('fuse')))
    items.append((have_gi_module('Gtk', '3.0'), 'GTK 3 for Python (tray daemon)', pkg_hint('gtk')))
    tray = have_gi_module('AyatanaAppIndicator3', '0.1') or have_gi_module('AppIndicator3', '0.1')
    session = os.environ.get('XDG_SESSION_TYPE', '?')
    items.append((tray or session == 'x11',
                  'tray icon (AppIndicator)' if tray else 'tray icon (X11 fallback only)',
                  pkg_hint('tray') + ('  [needed on Wayland, GNOME and KDE]' if not tray else '')))
    items.append((bool(shutil.which('notify-send')), 'desktop notifications (notify-send)', pkg_hint('notify')))
    items.append((bool(shutil.which('pkexec')), 'graphical authentication (pkexec/polkit)',
                  'install polkit and a polkit agent, or use sudo in a terminal'))
    items.append((bool(shutil.which('setfacl')), 'ACL tools (setfacl)', pkg_hint('acl')))
    return items


# ── Actions ───────────────────────────────────────────────────────────────────
def run(cmd, log, **kw):
    log('$ ' + ' '.join(cmd))
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, **kw)
    except OSError as e:
        log(str(e))
        return 127
    for line in (p.stdout + p.stderr).splitlines():
        log('  ' + line)
    return p.returncode


def build(log):
    if os.access(WAFFLE_BIN, os.X_OK):
        return True
    log('Building waffle-fuse...')
    if run(['make', '-C', WAFFLE_DIR, '-j4'], log) != 0:
        log('Build failed: ' + pkg_hint('build'))
        return False
    return True


def find_askpass():
    for p in (os.environ.get('SUDO_ASKPASS'), shutil.which('ksshaskpass'), shutil.which('ssh-askpass'),
              shutil.which('lxqt-openssh-askpass'), '/usr/lib/ssh/ssh-askpass',
              '/usr/lib/ssh/x11-ssh-askpass', '/usr/libexec/openssh/ssh-askpass',
              '/usr/libexec/seahorse/ssh-askpass'):
        if p and os.access(p, os.X_OK):
            return p
    return None


def run_privileged(args, log):
    """Run install-system.sh as root with graphical authentication.
    Returns the script's exit code, or None if no way to get root."""
    cmd = [SYSTEM_SCRIPT] + args
    if shutil.which('pkexec'):
        rc = run(['pkexec'] + cmd, log)
        if rc not in (126, 127):          # 126 dismissed / 127 not authorized or no agent
            return rc
        log('pkexec did not authorize (no polkit agent, or cancelled)')
    askpass = find_askpass()
    if askpass and shutil.which('sudo'):
        env = dict(os.environ, SUDO_ASKPASS=askpass)
        return run(['sudo', '-A'] + cmd, log, env=env)
    return None


def daemon_pids():
    try:
        out = subprocess.run(['pgrep', '-f', '^/usr/bin/python3 .*waffle-diskflashback'],
                             capture_output=True, text=True).stdout
        return [int(x) for x in out.split()]
    except OSError:
        return []


def stop_daemon(log):
    pids = daemon_pids()
    if not pids:
        return
    log('Stopping the running daemon (pending writes are completed first)...')
    if autostart_kind() == 'systemd':
        subprocess.run(['systemctl', '--user', 'stop', SYSTEMD_UNIT], capture_output=True)
    for pid in pids:
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    for _ in range(120):
        if not daemon_pids():
            return
        time.sleep(0.5)
    log('The daemon did not stop within 60 s')


def start_daemon(log):
    if os.path.isfile(os.path.join(SYSTEMD_UNIT_DIR, SYSTEMD_UNIT)) and autostart_kind() == 'systemd':
        run(['systemctl', '--user', 'start', SYSTEMD_UNIT], log)
        return
    os.makedirs(os.path.dirname(DAEMON_LOG), exist_ok=True)
    with open(DAEMON_LOG, 'a') as logf:
        subprocess.Popen(['/usr/bin/python3', DAEMON], stdout=logf, stderr=logf,
                         stdin=subprocess.DEVNULL, start_new_session=True)
    log('Daemon started (log: ' + DAEMON_LOG + ')')


def install_autostart(log):
    kind = autostart_kind()
    remove_autostart(log, quiet=True)
    if kind == 'xdg':
        os.makedirs(os.path.dirname(AUTOSTART_FILE), exist_ok=True)
        with open(os.path.join(WAFFLE_DIR, 'waffle-diskflashback.desktop')) as f:
            text = f.read()
        text = text.replace('ICON_PATH_PLACEHOLDER', ICON_DST).replace('WAFFLE_DIR_PLACEHOLDER', WAFFLE_DIR)
        with open(AUTOSTART_FILE, 'w') as f:
            f.write(text)
        log('Autostart with the graphical session: ' + AUTOSTART_FILE)
    else:
        os.makedirs(SYSTEMD_UNIT_DIR, exist_ok=True)
        with open(os.path.join(SYSTEMD_UNIT_DIR, SYSTEMD_UNIT), 'w') as f:
            f.write(f"""[Unit]
Description=Waffle DiskFlashback - automount floppies from the DrawBridge/Waffle drive
PartOf=graphical-session.target
After=graphical-session.target

[Service]
ExecStart=/usr/bin/python3 {DAEMON}
Restart=on-failure
RestartSec=5

[Install]
WantedBy=graphical-session.target
""")
        run(['systemctl', '--user', 'daemon-reload'], log)
        run(['systemctl', '--user', 'enable', SYSTEMD_UNIT], log)
        log('Autostart through systemd (graphical-session.target): your compositor must '
            'start graphical-session.target (e.g. uwsm, or "systemctl --user start '
            'graphical-session.target" in its config)')


def remove_autostart(log, quiet=False):
    if os.path.exists(AUTOSTART_FILE):
        os.remove(AUTOSTART_FILE)
        if not quiet:
            log('Removed ' + AUTOSTART_FILE)
    unit = os.path.join(SYSTEMD_UNIT_DIR, SYSTEMD_UNIT)
    if os.path.exists(unit):
        subprocess.run(['systemctl', '--user', 'disable', SYSTEMD_UNIT], capture_output=True)
        os.remove(unit)
        subprocess.run(['systemctl', '--user', 'daemon-reload'], capture_output=True)
        if not quiet:
            log('Removed ' + unit)


def do_install(opts, log):
    """opts: dict(media, autostart, images, start_now).  Returns (ok, notes)."""
    notes = []
    if not build(log):
        return False, ['waffle-fuse could not be built']
    run([DESKTOP_SCRIPT, '--no-autostart'] + ([] if opts.get('images', True) else ['--no-image-handler']), log)

    if opts['media']:
        if media_ready() and os.path.exists(TMPFILES_CONF):
            log(f'Mount directory {udisks_base()}/{USER} already set up')
        else:
            log('System setup: the administrator password is needed...')
            rc = run_privileged([USER], log)
            if rc is None:
                cmd = f'sudo {SYSTEM_SCRIPT} {USER}'
                log('No graphical authentication available. Run in a terminal:\n  ' + cmd)
                notes.append('System setup not done: run in a terminal\n' + cmd)
            elif rc == 10:
                notes.append('You were added to the serial port group: log out and in again.')
            elif rc != 0:
                notes.append('System setup failed (see the log).')

    if opts['autostart']:
        install_autostart(log)
    else:
        remove_autostart(log)

    if opts['start_now']:
        stop_daemon(log)
        start_daemon(log)

    target = os.path.join(udisks_base(), USER) if media_ready() else os.path.join(HOME, 'Floppy')
    notes.insert(0, f'Disks will be mounted under {target}/<volume name>.')
    return True, notes


def do_uninstall(log):
    notes = []
    stop_daemon(log)
    remove_autostart(log)
    run([DESKTOP_SCRIPT, '--uninstall'], log)
    if os.path.exists(TMPFILES_CONF):
        log('Removing the system setup: the administrator password is needed...')
        rc = run_privileged(['--uninstall'], log)
        if rc is None:
            notes.append(f'Run in a terminal: sudo {SYSTEM_SCRIPT} --uninstall')
    return True, notes


# ── Text mode ─────────────────────────────────────────────────────────────────
def text_main():
    def log(msg):
        print(msg)

    def ask(q, default=True):
        a = input(f'{q} [{"Y/n" if default else "y/N"}] ').strip().lower()
        return default if not a else a.startswith('y')

    print('waffle-fuse installer (text mode)\n')
    missing = [(label, hint) for ok, label, hint in check_environment() if not ok]
    if not missing:
        print('System check passed: OK')
    for label, hint in missing:
        print(f'Missing {label}\n    {hint}')
    print()
    if ask('Uninstall instead of install?', False):
        _, notes = do_uninstall(log)
    else:
        opts = {
            'media': ask(f'Mount disks under {udisks_base()}/{USER} like USB sticks (needs the admin password)?'),
            'autostart': ask('Start the tray daemon with the graphical session?'),
            'images': ask('Open .adf/.img floppy images as virtual disks (double-click)?'),
            'start_now': ask('Start (or restart) the daemon now?'),
        }
        _, notes = do_install(opts, log)
    print('\n' + '\n'.join(notes))


# ── GTK mode ──────────────────────────────────────────────────────────────────
def gtk_main():
    import gi
    gi.require_version('Gtk', '3.0')
    from gi.repository import Gtk, GLib

    win = Gtk.Window(title='waffle-fuse installer')
    win.set_default_size(640, 560)
    win.set_border_width(12)
    win.connect('destroy', Gtk.main_quit)
    icon = os.path.join(WAFFLE_DIR, 'icons', 'amiga-floppy.svg')
    if os.path.exists(icon):
        win.set_icon_from_file(icon)

    box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
    win.add(box)

    title = Gtk.Label()
    title.set_markup('<span size="x-large" weight="bold">waffle-fuse</span>\n'
                     'Amiga and PC floppies from the DrawBridge/Waffle drive, on your desktop')
    title.set_xalign(0)
    box.pack_start(title, False, False, 0)

    # Environment check: one line when everything is there, else only what is missing
    frame = Gtk.Frame(label=' System check ')
    grid = Gtk.Grid(column_spacing=8, row_spacing=2, margin=8)
    frame.add(grid)
    box.pack_start(frame, False, False, 0)
    missing = [(label, hint) for ok, label, hint in check_environment() if not ok]
    if not missing:
        lbl = Gtk.Label(label='System check passed: OK', xalign=0)
        grid.attach(lbl, 0, 0, 1, 1)
    for row, (label, hint) in enumerate(missing):
        lbl = Gtk.Label(xalign=0)
        lbl.set_markup('Missing ' + GLib.markup_escape_text(label) +
                       f'\n<small>{GLib.markup_escape_text(hint)}</small>')
        lbl.set_line_wrap(True)
        lbl.set_selectable(True)
        grid.attach(lbl, 0, row, 1, 1)

    # Options
    c_media = Gtk.CheckButton(label=f'Mount disks under {udisks_base()}/{USER}/<volume> like USB sticks '
                                    '(asks for the administrator password)')
    c_media.set_active(True)
    c_auto = Gtk.CheckButton(label='Start the tray daemon when I log in to the graphical session '
                                   f'({"XDG autostart" if autostart_kind() == "xdg" else "systemd user service"})')
    c_auto.set_active(True)
    c_img = Gtk.CheckButton(label='Open .adf/.img floppy images as virtual disks when double-clicked '
                                  '(images that cannot be mounted still open with their usual application)')
    c_img.set_active(True)
    c_now = Gtk.CheckButton(label='Start (or restart) the daemon now')
    c_now.set_active(True)
    for c in (c_media, c_auto, c_img, c_now):
        c.get_child().set_line_wrap(True)
        box.pack_start(c, False, False, 0)

    # Log
    view = Gtk.TextView(editable=False, cursor_visible=False, monospace=True)
    view.set_wrap_mode(Gtk.WrapMode.WORD_CHAR)
    buf = view.get_buffer()
    scroll = Gtk.ScrolledWindow()
    scroll.set_vexpand(True)
    scroll.add(view)
    box.pack_start(scroll, True, True, 0)

    buttons = Gtk.Box(spacing=8)
    b_uninst = Gtk.Button(label='Uninstall')
    b_close = Gtk.Button(label='Close')
    b_inst = Gtk.Button(label='Install')
    b_inst.get_style_context().add_class('suggested-action')
    buttons.pack_start(b_uninst, False, False, 0)
    buttons.pack_end(b_inst, False, False, 0)
    buttons.pack_end(b_close, False, False, 0)
    box.pack_start(buttons, False, False, 0)
    b_close.connect('clicked', lambda _b: Gtk.main_quit())

    def log(msg):
        def append():
            buf.insert(buf.get_end_iter(), msg + '\n')
            view.scroll_to_mark(buf.get_insert(), 0, False, 0, 0)
            return False
        GLib.idle_add(append)

    def finished(ok, notes):
        for w in (b_inst, b_uninst, b_close):
            w.set_sensitive(True)
        dlg = Gtk.MessageDialog(transient_for=win, modal=True,
                                message_type=Gtk.MessageType.INFO if ok else Gtk.MessageType.ERROR,
                                buttons=Gtk.ButtonsType.OK,
                                text='Done' if ok else 'Something went wrong')
        dlg.format_secondary_text('\n\n'.join(notes))
        dlg.run()
        dlg.destroy()
        return False

    def work(fn):
        for w in (b_inst, b_uninst, b_close):
            w.set_sensitive(False)

        def thread():
            try:
                ok, notes = fn()
            except Exception as e:      # report instead of dying silently
                ok, notes = False, [str(e)]
                log(f'ERROR: {e}')
            GLib.idle_add(finished, ok, notes)
        threading.Thread(target=thread, daemon=True).start()

    b_inst.connect('clicked', lambda _b: work(lambda: do_install(
        {'media': c_media.get_active(), 'autostart': c_auto.get_active(),
         'images': c_img.get_active(), 'start_now': c_now.get_active()}, log)))
    b_uninst.connect('clicked', lambda _b: work(lambda: do_uninstall(log)))

    win.show_all()
    Gtk.main()


def main():
    if '--text' in sys.argv:
        return text_main()
    if not (os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY')):
        return text_main()
    try:
        import gi
        gi.require_version('Gtk', '3.0')
        from gi.repository import Gtk  # noqa: F401
    except (ImportError, ValueError):
        msg = 'GTK 3 for Python is missing: ' + pkg_hint('gtk')
        for cmd in (['zenity', '--error', '--text', msg], ['kdialog', '--error', msg],
                    ['notify-send', 'waffle-fuse installer', msg]):
            if shutil.which(cmd[0]):
                subprocess.run(cmd)
                break
        if sys.stdin.isatty():
            print(msg + '\nContinuing in text mode.\n')
            return text_main()
        return 1
    gtk_main()


if __name__ == '__main__':
    sys.exit(main())
