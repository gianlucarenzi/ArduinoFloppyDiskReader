#!/usr/bin/python3
"""
waffle-open-image – open a floppy image (.adf, .img, .ima) as a virtual disk.

Used as the desktop handler for disk image files (see install-desktop.sh):

  * AmigaDOS (OFS/FFS) and FAT floppy images are mounted read-write with
    waffle-fuse --image, where real floppies are mounted (/media/$USER,
    /run/media/$USER or ~/Floppy), so they appear as a volume on the desktop;
    files can be added and removed, changes go straight into the image file,
    "Unmount" on the desktop icon closes it;
  * an image that is already mounted just gets a file manager window;
  * anything waffle-fuse cannot mount (NDOS game disks, hard disk images...)
    is handed to the application that opened that file type before
    waffle-fuse was installed (e.g. an emulator or the disk image mounter).
"""

import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
WAFFLE_BIN = os.path.normpath(os.path.join(SCRIPT_DIR, '..', 'waffle-fuse'))
CONFIG_DIR = os.path.join(os.environ.get('XDG_CONFIG_HOME', os.path.expanduser('~/.config')), 'waffle-fuse')
PREVIOUS_HANDLERS = os.path.join(CONFIG_DIR, 'previous-handlers')
MAX_FLOPPY_IMAGE = 4 * 1024 * 1024        # anything bigger is not a floppy image

# waffle-fuse exit codes meaning "this is not something I can mount"
NOT_MOUNTABLE = {4, 6}


def notify(title, msg, error=False):
    try:
        subprocess.Popen(['notify-send', '-a', 'Waffle', '-i', 'dialog-error' if error else 'media-floppy',
                          title, msg], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except FileNotFoundError:
        print(f'{title}: {msg}', file=sys.stderr)


def default_mount_dir():
    """Same rule as the tray daemon: where udisks mounts, if writable, else ~/Floppy."""
    user = os.environ.get('USER') or os.getlogin()
    for base in ('/run/media', '/media'):
        d = os.path.join(base, user)
        if os.path.isdir(d) and os.access(d, os.W_OK | os.X_OK):
            return d
    return os.path.expanduser('~/Floppy')


def open_folder(path):
    for cmd in (['xdg-open', path], ['gio', 'open', path]):
        try:
            subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            return
        except FileNotFoundError:
            continue


def unescape_mount_field(s):
    """/proc/mounts writes space, tab, newline and backslash as octal escapes."""
    import re
    return re.sub(r'\\([0-7]{3})', lambda m: chr(int(m.group(1), 8)), s)


def mounted_at(image):
    """Mountpoint of an already mounted image, or None."""
    try:
        with open('/proc/mounts') as f:
            for line in f:
                src, mnt = line.split()[:2]
                if unescape_mount_field(src) == 'waffle:' + image:
                    return unescape_mount_field(mnt)
    except OSError:
        pass
    return None


def content_type(path):
    try:
        from gi.repository import Gio
        info = Gio.File.new_for_path(path).query_info('standard::content-type', 0, None)
        return info.get_content_type()
    except Exception:
        return None


def previous_handler(ctype):
    try:
        with open(PREVIOUS_HANDLERS) as f:
            for line in f:
                t, _, app = line.strip().partition('=')
                if t == ctype and app:
                    return app
    except OSError:
        pass
    return None


def hand_over(path):
    """Open the file with the application that handled its type before us."""
    ctype = content_type(path)
    app_id = previous_handler(ctype) if ctype else None
    if not app_id:
        return False
    try:
        from gi.repository import Gio
        app = Gio.DesktopAppInfo.new(app_id)
        if app is None:
            return False
        return app.launch([Gio.File.new_for_path(path)], None)
    except Exception:
        return False


def open_image(path):
    path = os.path.realpath(path)
    name = os.path.basename(path)

    mnt = mounted_at(path)
    if mnt:
        open_folder(mnt)
        return 0

    try:
        size = os.path.getsize(path)
    except OSError as e:
        notify('Cannot open image', f'{name}: {e.strerror}', error=True)
        return 1
    if size > MAX_FLOPPY_IMAGE:
        if hand_over(path):
            return 0
        notify('Not a floppy image', f'{name} is too big for a floppy disk.', error=True)
        return 1

    # Without -f waffle-fuse detects the filesystem, mounts, then goes to the
    # background: its exit code tells whether the mount worked.
    r = subprocess.run([WAFFLE_BIN, '--image', path, '--automount', default_mount_dir()],
                       capture_output=True, text=True)
    if r.returncode == 0:
        mnt = next((l.split('mountpoint=', 1)[1] for l in r.stdout.splitlines()
                    if l.startswith('WAFFLE event=mounted mountpoint=')), None)
        ro = ' (read-only)' if 'readonly=1' in r.stdout else ''
        notify('Disk image opened', f'{name}{ro}\nUnmount it from the desktop icon when done.')
        if mnt:
            open_folder(mnt)
        return 0

    if r.returncode in NOT_MOUNTABLE and hand_over(path):
        return 0
    detail = {4: 'it is not an AmigaDOS or FAT disk (NDOS, unformatted or copy-protected)',
              6: 'its filesystem is not valid'}.get(r.returncode)
    if not detail:
        lines = [l for l in r.stderr.splitlines() if l.strip()]
        detail = lines[-1] if lines else f'error {r.returncode}'
    notify('Cannot open disk image', f'{name}: {detail}', error=True)
    return 1


def main():
    if len(sys.argv) < 2:
        print('usage: waffle-open-image.py <image>...', file=sys.stderr)
        return 1
    return max(open_image(p) for p in sys.argv[1:])


if __name__ == '__main__':
    sys.exit(main())
