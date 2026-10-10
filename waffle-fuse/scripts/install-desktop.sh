#!/bin/sh
# install-desktop.sh – install the waffle-fuse launchers, icons and tray daemon
#
#   ./scripts/install-desktop.sh            install for the current user
#   ./scripts/install-desktop.sh --no-autostart
#   ./scripts/install-desktop.sh --uninstall
#
# Run once after building waffle-fuse.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WAFFLE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

ICON_DST="$HOME/.local/share/icons/waffle-fuse"
HICOLOR_DST="$HOME/.local/share/icons/hicolor/scalable/devices"
APP_DST="$HOME/.local/share/applications"
AUTOSTART_DST="$HOME/.config/autostart"

AUTOSTART=1
[ "${1:-}" = "--no-autostart" ] && AUTOSTART=0

if [ "${1:-}" = "--uninstall" ]; then
    rm -f "$AUTOSTART_DST/waffle-diskflashback.desktop"
    for name in waffle-diskflashback.desktop waffle-floppy.desktop \
                waffle-amiga-dd.desktop waffle-amiga-hd.desktop \
                waffle-dos-dd.desktop waffle-dos-hd.desktop; do
        rm -f "$APP_DST/$name"
    done
    rm -rf "$ICON_DST"
    rm -f "$HICOLOR_DST/waffle-amiga-floppy.svg" "$HICOLOR_DST/waffle-dos-floppy.svg"
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$APP_DST" 2>/dev/null || true
    echo "Launchers, icons and autostart entry removed."
    exit 0
fi

if [ ! -x "$WAFFLE_DIR/waffle-fuse" ]; then
    echo "Build waffle-fuse first (make)." >&2
    exit 1
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

chmod +x "$WAFFLE_DIR/scripts/waffle-mount.sh" "$WAFFLE_DIR/scripts/waffle-diskflashback.py"

install_desktop() {
    sed -e "s|ICON_PATH_PLACEHOLDER|$ICON_DST|g" \
        -e "s|WAFFLE_DIR_PLACEHOLDER|$WAFFLE_DIR|g" \
        "$WAFFLE_DIR/$1" > "$2"
    echo "Installed $2"
}

for name in waffle-diskflashback.desktop waffle-floppy.desktop \
            waffle-amiga-dd.desktop waffle-amiga-hd.desktop \
            waffle-dos-dd.desktop waffle-dos-hd.desktop; do
    install_desktop "$name" "$APP_DST/$name"
done

if [ $AUTOSTART -eq 1 ]; then
    mkdir -p "$AUTOSTART_DST"
    install_desktop waffle-diskflashback.desktop "$AUTOSTART_DST/waffle-diskflashback.desktop"
fi

command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$APP_DST" 2>/dev/null || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -f -t "$HOME/.local/share/icons" 2>/dev/null || true

if ! /usr/bin/python3 -c "import gi; gi.require_version('Gtk', '3.0')" 2>/dev/null; then
    echo ""
    echo "WARNING: the tray daemon needs PyGObject: sudo apt install python3-gi gir1.2-gtk-3.0"
fi

echo ""
echo "Done."
echo "  - Waffle DiskFlashback (tray): mounts disks automatically on insert under ~/Floppy/<volume>"
[ $AUTOSTART -eq 1 ] && echo "    (starts at login; start it now with: $WAFFLE_DIR/scripts/waffle-diskflashback.py &)"
echo "  - Floppy (DrawBridge/Waffle): one-click mount/eject, format autodetected"
echo "  - waffle-amiga-dd/hd, waffle-dos-dd/hd: same, with forced format/density"
