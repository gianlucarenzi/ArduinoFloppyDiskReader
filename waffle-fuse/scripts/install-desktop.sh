#!/bin/sh
# install-desktop.sh – install the waffle-fuse launchers, icons, tray daemon
# and disk image handler for the current user (no root needed)
#
#   ./scripts/install-desktop.sh                       install everything
#   ./scripts/install-desktop.sh --no-autostart        do not start the tray daemon at login
#   ./scripts/install-desktop.sh --no-image-handler    do not open .adf/.img files with waffle-fuse
#   ./scripts/install-desktop.sh --uninstall
#
# Normally run by the graphical installer (waffle-installer.py).

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WAFFLE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
ICON_DST="$DATA_HOME/icons/waffle-fuse"
HICOLOR_DST="$DATA_HOME/icons/hicolor/scalable/devices"
APP_DST="$DATA_HOME/applications"
MIME_DST="$DATA_HOME/mime"
AUTOSTART_DST="$CONFIG_HOME/autostart"
PREVIOUS_HANDLERS="$CONFIG_HOME/waffle-fuse/previous-handlers"

IMAGE_HANDLER=waffle-open-image.desktop
# Content types floppy images get on the various distributions
IMAGE_TYPES="application/x-amiga-disk-image application/x-amiga-disk-format application/vnd.efi.img application/x-raw-disk-image"

AUTOSTART=1
HANDLER=1
UNINSTALL=0
for arg in "$@"; do
    case "$arg" in
        --no-autostart)     AUTOSTART=0 ;;
        --no-image-handler) HANDLER=0 ;;
        --uninstall)        UNINSTALL=1 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

# Remove "type=waffle-open-image.desktop" from [Default Applications]
drop_default() {
    f="$CONFIG_HOME/mimeapps.list"
    [ -f "$f" ] && sed -i "\|^$1=$IMAGE_HANDLER;*\$|d" "$f"
    return 0
}

# Give the image types back to the applications that had them before
restore_image_defaults() {
    command -v xdg-mime >/dev/null 2>&1 || return 0
    for t in $IMAGE_TYPES; do
        [ "$(xdg-mime query default "$t" 2>/dev/null)" = "$IMAGE_HANDLER" ] || continue
        prev="$(sed -n "s|^$t=||p" "$PREVIOUS_HANDLERS" 2>/dev/null | head -1)"
        drop_default "$t"
        [ -n "$prev" ] && xdg-mime default "$prev" "$t"
    done
    rm -f "$PREVIOUS_HANDLERS"
}

refresh_databases() {
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$APP_DST" 2>/dev/null || true
    command -v update-mime-database >/dev/null 2>&1 && update-mime-database "$MIME_DST" >/dev/null 2>&1 || true
}

if [ $UNINSTALL -eq 1 ]; then
    restore_image_defaults
    rm -f "$AUTOSTART_DST/waffle-diskflashback.desktop"
    for name in waffle-diskflashback.desktop waffle-floppy.desktop $IMAGE_HANDLER \
                waffle-amiga-dd.desktop waffle-amiga-hd.desktop \
                waffle-dos-dd.desktop waffle-dos-hd.desktop; do
        rm -f "$APP_DST/$name"
    done
    rm -f "$MIME_DST/packages/waffle-fuse.xml"
    rm -rf "$ICON_DST"
    rm -f "$HICOLOR_DST/waffle-amiga-floppy.svg" "$HICOLOR_DST/waffle-dos-floppy.svg"
    refresh_databases
    echo "Launchers, icons, image handler and autostart entry removed."
    exit 0
fi

if [ ! -x "$WAFFLE_DIR/waffle-fuse" ]; then
    echo "Build waffle-fuse first (make)." >&2
    exit 1
fi

# Remember the current default application of each image type *before*
# installing our handler: once it is installed it may already show up as the
# (implicit) default, hiding the application that really had the type.
if [ $HANDLER -eq 1 ] && command -v xdg-mime >/dev/null 2>&1; then
    mkdir -p "$(dirname "$PREVIOUS_HANDLERS")"
    touch "$PREVIOUS_HANDLERS"
    for t in $IMAGE_TYPES; do
        cur="$(xdg-mime query default "$t" 2>/dev/null || true)"
        if [ -n "$cur" ] && [ "$cur" != "$IMAGE_HANDLER" ]; then
            sed -i "\|^$t=|d" "$PREVIOUS_HANDLERS"
            echo "$t=$cur" >> "$PREVIOUS_HANDLERS"
        fi
    done
fi

mkdir -p "$ICON_DST" "$APP_DST"

for icon in amiga-floppy.svg dos-floppy.svg tray-idle.svg tray-mounting.svg tray-mounted.svg; do
    cp "$WAFFLE_DIR/icons/$icon" "$ICON_DST/"
done
# Themed device icons, named in the volume info waffle-fuse shows to file managers
mkdir -p "$HICOLOR_DST"
cp "$WAFFLE_DIR/icons/amiga-floppy.svg" "$HICOLOR_DST/waffle-amiga-floppy.svg"
cp "$WAFFLE_DIR/icons/dos-floppy.svg"   "$HICOLOR_DST/waffle-dos-floppy.svg"
echo "Icons installed to $ICON_DST and $HICOLOR_DST"

chmod +x "$WAFFLE_DIR/scripts/waffle-mount.sh" "$WAFFLE_DIR/scripts/waffle-diskflashback.py" \
         "$WAFFLE_DIR/scripts/waffle-open-image.py"

install_desktop() {
    sed -e "s|ICON_PATH_PLACEHOLDER|$ICON_DST|g" \
        -e "s|WAFFLE_DIR_PLACEHOLDER|$WAFFLE_DIR|g" \
        "$WAFFLE_DIR/$1" > "$2"
    echo "Installed $2"
}

for name in waffle-diskflashback.desktop waffle-floppy.desktop $IMAGE_HANDLER \
            waffle-amiga-dd.desktop waffle-amiga-hd.desktop \
            waffle-dos-dd.desktop waffle-dos-hd.desktop; do
    install_desktop "$name" "$APP_DST/$name"
done

if [ $AUTOSTART -eq 1 ]; then
    mkdir -p "$AUTOSTART_DST"
    install_desktop waffle-diskflashback.desktop "$AUTOSTART_DST/waffle-diskflashback.desktop"
fi

# Disk image types (only adds a type where the system has none)
mkdir -p "$MIME_DST/packages"
cp "$WAFFLE_DIR/mime/waffle-fuse.xml" "$MIME_DST/packages/waffle-fuse.xml"
refresh_databases

# Double-click on a floppy image opens it as a virtual disk.  The previous
# default applications are remembered: waffle-open-image.py hands them the
# images it cannot mount, and --uninstall gives the types back to them.
if [ $HANDLER -eq 1 ] && command -v xdg-mime >/dev/null 2>&1; then
    for t in $IMAGE_TYPES; do
        xdg-mime default "$IMAGE_HANDLER" "$t"     # explicit, not just implied
    done
    echo "Floppy images (.adf, .img, .ima) now open as virtual disks"
elif [ $HANDLER -eq 0 ]; then
    restore_image_defaults
fi

# Running programs (file manager, desktop) rescan an icon theme only when the
# theme directory itself changes: touch it so the new icons show up right away.
# (No icon cache for the user's hicolor: GTK scans it, and a stale cache would
# hide icons added later.)
touch "$DATA_HOME/icons/hicolor"

if ! /usr/bin/python3 -c "import gi; gi.require_version('Gtk', '3.0')" 2>/dev/null; then
    echo ""
    echo "WARNING: the tray daemon needs PyGObject and GTK 3 (see the README)"
fi

echo ""
echo "Done."
echo "  - Waffle DiskFlashback (tray): mounts disks automatically when inserted"
[ $AUTOSTART -eq 1 ] && echo "    (starts at login; start it now with: $WAFFLE_DIR/scripts/waffle-diskflashback.py &)"
echo "  - Floppy (DrawBridge/Waffle): one-click mount/eject, format autodetected"
echo "  - waffle-amiga-dd/hd, waffle-dos-dd/hd: same, with forced format/density"
[ $HANDLER -eq 1 ] && echo "  - double-click an .adf/.img floppy image to open it as a virtual disk"
exit 0
