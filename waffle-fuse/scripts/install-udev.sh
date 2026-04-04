#!/bin/sh
# install-udev.sh  –  install the waffle-fuse udev rule and helper script
#
# Must be run as root (or with sudo):
#   sudo scripts/install-udev.sh [--uninstall]
#
# What it does:
#   1. Copies waffle-fuse binary to /usr/local/bin/
#   2. Copies waffle-udev.sh  to /usr/local/lib/waffle-fuse/
#   3. Copies 99-waffle-fuse.rules to /etc/udev/rules.d/
#   4. Installs SVG icons to /usr/share/icons/hicolor/scalable/devices/
#   5. Reloads udev rules and icon cache

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

RULE_SRC="$PROJECT_DIR/rules/99-waffle-fuse.rules"
UDEV_SH_SRC="$SCRIPT_DIR/waffle-udev.sh"
WAFFLE_BIN="$PROJECT_DIR/waffle-fuse"
ICON_DIR="$PROJECT_DIR/icons"

RULE_DST="/etc/udev/rules.d/99-waffle-fuse.rules"
LIB_DIR="/usr/local/lib/waffle-fuse"
UDEV_SH_DST="$LIB_DIR/waffle-udev.sh"
BIN_DST="/usr/local/bin/waffle-fuse"
HICOLOR_ICONS="/usr/share/icons/hicolor/scalable/devices"

if [ "$(id -u)" -ne 0 ]; then
    echo "Error: this script must be run as root." >&2
    echo "  sudo $0 $*" >&2
    exit 1
fi

# ── Uninstall ────────────────────────────────────────────────────────────────
if [ "$1" = "--uninstall" ]; then
    echo "Removing waffle-fuse udev integration..."
    rm -f "$RULE_DST" "$UDEV_SH_DST" "$BIN_DST"
    rm -f "$HICOLOR_ICONS/amiga-floppy.svg" "$HICOLOR_ICONS/dos-floppy.svg"
    rmdir "$LIB_DIR" 2>/dev/null || true
    udevadm control --reload-rules
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f /usr/share/icons/hicolor 2>/dev/null || true
    fi
    echo "Done. Existing mounts are unaffected until next reboot or manual unmount."
    exit 0
fi

# ── Install ──────────────────────────────────────────────────────────────────
[ -f "$RULE_SRC" ]    || { echo "Error: $RULE_SRC not found" >&2; exit 1; }
[ -f "$UDEV_SH_SRC" ] || { echo "Error: $UDEV_SH_SRC not found" >&2; exit 1; }
[ -x "$WAFFLE_BIN" ]  || { echo "Error: $WAFFLE_BIN not found (run 'make' first)" >&2; exit 1; }

echo "Installing waffle-fuse udev integration..."

# Binary
install -m 755 "$WAFFLE_BIN" "$BIN_DST"
echo "  $BIN_DST"

# Helper script
mkdir -p "$LIB_DIR"
install -m 755 "$UDEV_SH_SRC" "$UDEV_SH_DST"
echo "  $UDEV_SH_DST"

# udev rule
install -m 644 "$RULE_SRC" "$RULE_DST"
echo "  $RULE_DST"

# Icons
mkdir -p "$HICOLOR_ICONS"
install -m 644 "$ICON_DIR/amiga-floppy.svg" "$HICOLOR_ICONS/amiga-floppy.svg"
install -m 644 "$ICON_DIR/dos-floppy.svg"   "$HICOLOR_ICONS/dos-floppy.svg"
echo "  $HICOLOR_ICONS/amiga-floppy.svg"
echo "  $HICOLOR_ICONS/dos-floppy.svg"
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f /usr/share/icons/hicolor 2>/dev/null || true
fi

# Reload
udevadm control --reload-rules
echo ""
echo "Done. Plug in the DrawBridge / Waffle device to trigger auto-mount."
echo "To remove:  sudo $0 --uninstall"
