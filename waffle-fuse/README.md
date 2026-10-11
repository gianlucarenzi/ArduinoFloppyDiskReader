# waffle-fuse

A FUSE userspace driver for the **DrawBridge / Waffle** floppy reader on Arduino.
Makes the drive behave like an Amiga drive on the Linux desktop:

* **insert a disk** → it mounts automatically in `/media/$USER/<volume name>` (or
  `~/Floppy/<volume name>`, see below) and appears
  as a volume icon on the desktop and in the file manager (like the icon on Workbench);
* **eject** → from the tray icon menu or "Unmount" on the desktop icon:
  pending changes are written, then a message appears saying you can remove the disk;
* **remove the disk** while mounted → it unmounts automatically;
* **double-click on an `.adf` or `.img` image** → it opens as a virtual disk, where you can
  add and remove files;
* **from the tray** copy the disk to an image (`.adf`, `.img` or `.scp`) and write
  an image to a real disk.

Supported formats (auto-detected, including density):

| Disk | Filesystem | Access |
|------|------------|--------|
| Amiga 880 KB (DD) / 1.76 MB (HD) | OFS, FFS, INTL (`DOS0`–`DOS3`) | read + write |
| Amiga with DirCache (`DOS4`/`DOS5`) | | read-only |
| PC 720 KB (DD) / 1.44 MB (HD) | FAT12 (also FAT16), long names VFAT | read + write |

---

## Dependencies

The graphical installer checks them and shows the correct command for your distribution.
Manually:

| Distribution | Compilation | Daemon, tray, notifications, installer |
|---|---|---|
| Debian / Ubuntu | `sudo apt install build-essential pkg-config libfuse3-dev fuse3` | `sudo apt install python3-gi gir1.2-gtk-3.0 gir1.2-ayatanaappindicator3-0.1 libnotify-bin acl` |
| Arch / Manjaro | `sudo pacman -S --needed base-devel pkgconf fuse3` | `sudo pacman -S --needed python-gobject gtk3 libayatana-appindicator libnotify acl` |
| Fedora | `sudo dnf install gcc-c++ make pkgconf fuse3-devel` | `sudo dnf install python3-gobject gtk3 libayatana-appindicator-gtk3 libnotify acl` |
| openSUSE | `sudo zypper install gcc-c++ make pkg-config fuse3-devel` | `sudo zypper install python3-gobject typelib-1_0-Gtk-3_0 typelib-1_0-AyatanaAppIndicator3-0_1 libnotify-tools acl` |

The AppIndicator package is needed for the tray icon on Wayland, GNOME, and KDE; on X11 the
daemon has a fallback that doesn't use it.

The driver accesses the hardware via serial port (`/dev/ttyUSB0` or similar). If your user
cannot open the port, the installer adds it to the group that owns it (`dialout` on Debian
and Ubuntu, `uucp` on Arch); then you must log in again.

---

## Compatibility

**Tested** (with a real Waffle drive, FTDI FT230X):

* Debian 13 (trixie), MATE on X11, libfuse 3.17, gvfs 1.57, udisks2 2.10: reading,
  writing and formatting Amiga DD/HD and DOS 720K/1.44M, daemon in tray, automatic mounting
  in `/media/$USER`, icons on desktop (Caja), hot removal, graphical installer with polkit
  authentication, opening ADF/IMG images with double-click, copying disk to image and writing
  image to disk from tray (each track written is re-read and verified). For NDOS game ADFs it
  was verified that Amiberry is chosen as the program to pass them to, without launching it.

**Expected, not yet tested** — the code is written to work with:

* **Distributions**: Debian 11 and 12, Ubuntu 20.04 and later, Arch and derivatives, Fedora,
  openSUSE. Requirements: libfuse ≥ 3.0 (only 3.0 API is used), C++17 compiler
  (gcc ≥ 7), Python ≥ 3.7 with PyGObject and GTK 3, systemd (tmpfiles and user service),
  polkit. On **Debian 11** all are present: libfuse 3.10, gcc 10, Python 3.9, systemd 247,
  udisks2 2.9 (mounts in `/media/$USER`), `gir1.2-ayatanaappindicator3-0.1`. The only thing
  to check is the custom icon on the desktop, which uses the `IconFile` key of
  `.xdg-volume-info` read by gvfs: if an older version doesn't support it, the generic
  theme icon for removable disks would appear.
* **Desktop**: GNOME, KDE Plasma, XFCE, MATE, Cinnamon, LXQt (startup with XDG autostart);
  sway, Hyprland, i3 and other window managers (systemd user service bound to
  `graphical-session.target`). X11 and Wayland.
* **Tray icon**: AppIndicator/StatusNotifierItem on KDE, XFCE, MATE, Cinnamon, waybar and
  GNOME with "AppIndicator" extension; on GNOME without extension there's no tray and the daemon
  works with notifications only.
* **Disk icons on desktop**: GIO-based file managers (Caja, Nautilus, Nemo,
  Thunar, PCManFM). Dolphin (KDE) uses Solid and hasn't been tested.

If you test it on another combination, the result is useful to add here.

---

## Building and installing

```bash
cd waffle-fuse
make
./scripts/waffle-installer.py        # or double-click waffle-installer.desktop
```

The graphical installer (GTK; designed for X11 and Wayland, for any desktop and for Debian,
Arch, Fedora and openSUSE: see Compatibility for what has been tested) checks dependencies
and suggests missing packages for your distribution, compiles waffle-fuse if needed, then:

* **for the user**: installs launcher and icons and starts **Waffle DiskFlashback** (the daemon
  with the tray icon) when the graphical session starts, with XDG autostart (GNOME, KDE,
  XFCE, MATE, Cinnamon, LXQt) or with a systemd user service bound to
  `graphical-session.target` on window managers without autostart (sway, Hyprland, i3...);
  if you keep the checkbox active, associates floppy images (`.adf`, `.img`, `.ima`) with
  waffle-fuse, so double-click opens them as virtual disks (see below);
* **for the system**: asks for the administrator password with the standard polkit window
  (`pkexec`; otherwise `sudo -A` with a graphical askpass) and runs
  `scripts/install-system.sh`, to mount disks where udisks mounts them and to grant access
  to the serial port (group `dialout` or `uucp`, only if needed).

It also has an uninstall button. Without graphical interface: `waffle-installer.py --text`.

The tray icon uses AppIndicator/StatusNotifierItem (package
`gir1.2-ayatanaappindicator3-0.1` on Debian, `libayatana-appindicator` on Arch), necessary
on Wayland, GNOME (with the AppIndicator extension) and KDE; on X11 there's a fallback
without it. Without tray the daemon still works, with notifications only.

### The tray menu

| Menu item | What it does |
|---|---|
| **Open** | opens the mounted disk in the file manager (also with a click on the icon) |
| **Eject** | unmounts the disk: writes pending changes, then notifies that you can remove it |
| **Mount again** | remounts an unmounted disk that's still in the drive |
| **Copy disk to image file…** | copies the disk in the drive to a file: `.adf`/`.img` copies the sectors (also of game NDOS disks, as Amiga disk), `.scp` records the raw magnetic flux, even of protected disks. Proposes the volume name and the folder `~/WaffleFolder` if it exists. About 1 minute for a DD `.adf` |
| **Write image to disk…** | writes an `.adf`/`.img`/`.ima` image to the disk in the drive, which is overwritten (asks for confirmation). Proposes the image open as a virtual disk and closes it first, so everything is in the file. By default rewrites only different tracks; the "Erase and rewrite every track" checkbox erases and rewrites them all (about 2.5 minutes for a DD). Each track written is re-read and verified |
| **Quit** | cleanly unmounts the disk and closes the daemon |

During a copy or write the mounted disk is unmounted (the serial port is needed for the
operation) and remounted at the end: after a write with the new content. A window shows
the progress track by track; **Cancel** stops the operation between one track and the next,
never in the middle of a track. The copy is done in a temporary file (`….part`) renamed
only at the end: cancelling doesn't lose a file being overwritten.

### Floppy images as virtual disks

With double-click on an `.adf`, `.img` or `.ima` image (`scripts/waffle-open-image.py`):

* AmigaDOS (OFS/FFS) and FAT images are mounted in read and write mode in the same
  folder as real disks, with the icon on the desktop, and the file manager opens. Changes
  go directly to the image file; close with "Unmount" on the icon. A second double-click
  on an already open image opens only the window. A file without write permission is opened
  read-only;
* images that waffle-fuse cannot mount (NDOS game disks, protected disks, images larger
  than a floppy) are passed to the program that opened them before (e.g. Amiberry for
  ADFs or the desktop image mounter).

The association follows XDG standards (shared-mime-info, `mimeapps.list`), so it applies
to all desktops: the interested types are `application/x-amiga-disk-image`,
`application/x-amiga-disk-format`, `application/vnd.efi.img` and
`application/x-raw-disk-image`. Programs associated before are remembered in
`~/.config/waffle-fuse/previous-handlers` and reassociated on uninstall.
Manually: `scripts/install-desktop.sh --no-image-handler` removes only the association.

Warning: changes made on a virtual disk go into the file itself. To keep an original ADF
intact, open a copy.

### Where disks are mounted

The mount folder takes the name of the Amiga volume or FAT label; if the disk
has no name it's called `FloppyDDAFFS`, `FloppyHDAFFS`, `FloppyDDDOS` or `FloppyHDDOS`.
It's created where udisks mounts USB sticks, `/media/$USER` (Debian, Ubuntu) or
`/run/media/$USER` (Arch, Fedora...), if the system part is installed; otherwise in
`~/Floppy`.

udisks2 creates that folder by itself, owned by root with an ACL read-only for the user,
and only when mounting a block device; waffle-fuse runs as user and can't create folders
there. `scripts/install-system.sh` (launched by the installer, or manually with `sudo`,
`--uninstall` to remove) installs `/etc/tmpfiles.d/waffle-fuse.conf`, with which systemd
creates the folder at every startup with an ACL that also gives the user write permission,
and applies it immediately. udisks2 sets its permissions only when creating the folder, so
it keeps working as before.

Daemon options: `--port /dev/ttyUSB0` (default: autodetect), `--mount-dir DIR`
(default as above; must be under `/media`, `/run/media` or in the home for icons to appear
on the desktop), `--open` (opens the file manager on insertion), `--ro` (always mounts
read-only).

To use the port with WaffleCopyPRO first close the daemon (menu → Quit):
the serial port is open exclusively.

---

## How it behaves (like trackdisk.device)

* **Track cache**: a full track is always read; subsequent reads on the same track don't
  touch the disk.
* **Deferred write**: writes go to cache and the track is written about 1 second after
  the last change (option `flushdelay`), then **re-read and verified**
  (up to 3 attempts). Copying a file doesn't rewrite the same track dozens of times.
* **No invented data**: an unreadable sector is never "guessed". The file containing it
  gives an I/O error, and a track with unreadable sectors is never rewritten, so corrupted
  sectors are not overwritten with zeros.
* **Motor**: turns off after 3 seconds of inactivity and turns back on when needed.
* **Removal**: with the disk stopped the drive is checked every 3 seconds (like
  trackdisk.device; adjustable with `diskpoll`). If the disk
  disappears, the volume is unmounted. If there were still pending changes an error message
  arrives, because those changes are lost: wait for the "you can remove" message
  before taking out the disk, as on Amiga.
* **Desktop**: in the disk root a `.xdg-volume-info` and a
  `.waffle-icon.svg` *virtual* file appear (read-only, never written to disk) with the volume
  name and Amiga or DOS icon, which file managers use for the desktop icon. The icon is
  given as a file (`IconFile`), because with a GTK theme name it would prefer the generic
  "removable disk" icon from the active theme. An already-present `.xdg-volume-info` on the
  disk is hidden.
* **No trash on floppy**: `.Trash`/`.Trash-<uid>` folders are rejected, so the file manager
  offers to delete permanently instead of filling the disk with deleted files (option `trash`
  to re-enable it).
* **Unmounting**: after `fusermount3 -u` (or "Unmount"/"Eject") the process writes what's
  left, frees the drive and removes the mount directory.

---

## Manual use

```
waffle-fuse <serial-port> <mountpoint> [options]
waffle-fuse <serial-port> --automount <dir> [options]     # mounts in <dir>/<volume name>
waffle-fuse --image <file.adf|file.img> <mountpoint> [options]
waffle-fuse --probe <port>          # 0 = disk present, 1 = no disk, 2 = error
waffle-fuse --wait-disk <port>      # waits for disk insertion
waffle-fuse --wait-removal <port>   # waits for disk removal
```

Tools for the disk (without mounting):

```
waffle-fuse --format <port> <type> <dd|hd> [name] [--quick] [--yes] [--force]
waffle-fuse --format-image <file> <type> <dd|hd> [name]   # creates an empty image
waffle-fuse --scan <port>                # surface map: good sectors per track
waffle-fuse --dump <port> <file.adf|img|scp> [--format amiga|dos] [--density dd|hd]
            [--tracks N] [--revs N]       # copies entire disk to an image
waffle-fuse --write-image <port> <file.adf|img> [--full] [--yes] [--force]
                                          # writes an image to disk: by default re-reads
                                          # each track and rewrites only the different ones
```

`--dump` chooses the copy type from the extension: `.adf`/`.img` copies sectors of the
recognized format (a non-DOS disk is copied as an Amiga disk, unreadable sectors become
zeros and are counted); `.scp` records the magnetic flux
(`--tracks`, default 80, up to 84; `--revs`, revolutions per track, default 3, up to 5).
`--write-image` rejects a DD image on an HD disk (or vice versa) without `--force`.
`--dump`, `--write-image` and `--format` stop between tracks with Ctrl+C or
SIGTERM, leaving the drive in a clean state.

`<type>`: `ffs` (or `amiga`), `ofs`, `amiga-intl`, `ofs-intl`, `dos`. Formatting
erases each track, rewrites it and re-reads it to verify (about 2 minutes), then
creates the filesystem: AmigaDOS like AmigaOS Format (DD 880K, HD 1.76M), FAT12 like
MS-DOS (720K, 1.44M). `--quick` rewrites only tracks with filesystem structures
and requires an already formatted disk. Without `--yes` it asks to type YES.
A DD disk is not formatted as HD (or vice versa) without `--force`.

```bash
./waffle-fuse --format /dev/ttyUSB0 ffs dd "Work"
./waffle-fuse --format /dev/ttyUSB0 dos hd "DATA"
```

The disk is read and recognized **before** mounting: with a wrong or
unreadable disk the command exits with an error and doesn't leave hanging mount points.

### Options (`-o key[=value],...`)

| Option | Description |
|--------|-------------|
| `format=fat\|affs` | Force filesystem type (default: autodetect) |
| `density=dd\|hd`  | Force density (default: autodetect, trying both) |
| `ro`              | Mount read-only |
| `flushdelay=MS`   | Deferred write delay (default 1000 ms) |
| `noverify`        | Don't re-read written tracks (faster, less safe) |
| `diskpoll=MS`     | Removal check interval (default 3000 ms, like trackdisk.device) |
| `trash`           | Allow file managers to create `.Trash-<uid>` trash on disk |
| `novolumeinfo`    | No virtual `.xdg-volume-info` (name and icon on desktop) |
| `debug_hw`        | Hardware/serial debug output |
| `-f`              | Stay in foreground |

### Examples

```bash
# Mount inserted disk in ~/Floppy/<volume name>, in background
./waffle-fuse /dev/ttyUSB0 --automount ~/Floppy

# In a fixed mount point, in foreground with debug
mkdir -p /tmp/floppy
./waffle-fuse /dev/ttyUSB0 /tmp/floppy -f -o debug_hw

# Unmount (writes pending changes)
fusermount3 -u ~/Floppy/Workbench3.1

# Work on an ADF/IMG image instead of the drive
./waffle-fuse --image workbench.adf /tmp/wb
```

There's also `scripts/waffle-mount.sh [auto|amiga|dos] [auto|dd|hd] [port]`, used by
the menu launchers: if the disk is mounted it unmounts, otherwise it mounts.

### Exit codes

| Code | Meaning |
|------|---------|
| 0 | unmounted normally, all changes written |
| 1 | error in arguments or mounting |
| 2 | serial port cannot be opened (disconnected or in use) |
| 3 | no disk in drive |
| 4 | non-DOS disk: not formatted, NDOS (games) or protected |
| 5 | drive error reading track 0 |
| 6 | invalid filesystem |
| 7 | disk removed: unmounted automatically |
| 8 | disk removed with pending changes (lost) |
| 9 | some changes were not written |

A line is printed to stdout for each event (`WAFFLE event=detected ...`,
`WAFFLE event=mounted mountpoint=...`, `WAFFLE event=removed`, ...), read by the daemon.

---

## Testing

Filesystems are tested on disk images, without hardware:

```bash
make test
```

The test copies a file tree onto freshly formatted images (OFS, FFS, FFS-INTL,
FFS HD, FAT 720K and 1.44M), then moves, renames, truncates, appends, deletes and fills the
disk. Finally it compares the result three times: from the mount, after a fresh mount and with
independent tools (`xdfscan`/`xdftool` from [amitools](https://github.com/cnvogelg/amitools)
for AmigaDOS, `fsck.fat` and [pyfatfs](https://github.com/nathanhi/pyfatfs) for FAT). If
the tools are in a virtualenv:

```bash
python3 -m venv ~/waffle-venv && ~/waffle-venv/bin/pip install amitools pyfatfs "setuptools<70"
XDFTOOL=~/waffle-venv/bin/xdftool XDFSCAN=~/waffle-venv/bin/xdfscan \
TEST_PYTHON=~/waffle-venv/bin/python make test
```

---

## Troubleshooting

**"Transport endpoint is not connected" on a mount directory** (in `/media/$USER`,
`/run/media/$USER` or `~/Floppy`): it's left over from a crash. Clean it with
`fusermount3 -uz <directory>`; the daemon does it by itself on startup.

**On the desktop the disk has a USB stick icon**: the file manager doesn't use the icon
provided by the virtual disk (see Compatibility); name and operation don't change. After
installing the icons it may help to restart the file manager (`caja -q`, `nautilus -q`).

**The tray icon doesn't appear**: on Wayland, GNOME and KDE the AppIndicator package is needed
(see Dependencies); on GNOME also the "AppIndicator and KStatusNotifierItem" extension.

**"cannot open device"**: check `ls -l /dev/ttyUSB0`, that you're in the `dialout` group
and that no other program (WaffleCopyPRO, another `waffle-fuse`) is using the port.

**"Disk in use" on eject**: a file manager window or terminal is inside the
disk. Close them and try again.

**Disk recognized incorrectly**: force format and density, for example
`-o format=affs,density=dd`.

---

## Architecture

```
waffle_fuse.cpp          – main(): detects disk, chooses filesystem, mounts, events
├── disk_cache.h/cpp     – IDiskImage; WaffleDisk (per-track cache, deferred
│   │                      and verified write, motor, removal detection); ImageDisk (file)
│   └── ADFWriter        – MFM Amiga and IBM encode/decode, serial I/O
├── fat_fs.h/cpp         – FAT12/FAT16 with long VFAT names
├── format.h/cpp         – empty AmigaDOS OFS/FFS and FAT12 images (DD and HD)
└── affs_fs.h/cpp        – Amiga OFS/FFS (ISO-8859-1 ↔ UTF-8 names, dates in local time)
scripts/
├── waffle-installer.py     – graphical installer (GTK or text): user + system via polkit
├── install-system.sh       – system part (root): /media or /run/media, serial port
├── install-desktop.sh      – launcher, icons and autostart (used by installer)
├── waffle-diskflashback.py – tray daemon: automount, eject, disk copy and write
├── waffle-open-image.py    – opens a floppy image as a virtual disk (double-click)
└── waffle-mount.sh         – mount/unmount with a click (launcher)
waffle-installer.desktop    – opens the installer with a double-click
waffle-open-image.desktop   – handler for .adf/.img/.ima images (not shown in menu)
mime/waffle-fuse.xml        – MIME types for *.adf and *.ima where the system doesn't define them
icons/                      – disk icons (also embedded in the binary) and tray icon
tests/run-tests.sh          – filesystem tests on images
```

---

## Known limitations

* On AmigaDOS names can be at most 30 characters (ISO-8859-1); links (hard
  and soft) can be read but not modified.
* Amiga volumes with unvalidated bitmap (disk to "validate") or with wrong root block
  checksum are mounted read-only (the reason is written to the log): validate them on
  an Amiga or in an emulator. To repair a disk from a good image there's `--write-image`.
* FAT32, sectors other than 512 bytes and Amiga partitioned disks (RDB) are not supported.
* A disk change faster than the check interval (3 s) may not be noticed (the drive signals the
  change only with the DSKCHG line): always unmount before changing the disk.
* `.ipf` files are not supported.
