#!/bin/sh
# install-udev.sh  –  install waffle-nbd and supporting files
#
# System install (requires root):
#   sudo scripts/install-udev.sh [--uninstall]
#
# Local install (no root needed — udev hotplug not available):
#   scripts/install-udev.sh --local [--uninstall]
#
# System install places:
#   1. waffle-fuse binary        → /usr/local/bin/  (used for --probe)
#   2. waffle-nbd binary         → /usr/local/bin/
#   3. waffle-udev.sh            → /usr/local/lib/waffle-fuse/
#   4. waffle-nbd-handler.sh     → /usr/local/lib/waffle-fuse/
#   5. udev rule                 → /etc/udev/rules.d/
#   6. polkit rule               → /etc/polkit-1/rules.d/
#   7. SVG icons                 → /usr/share/icons/hicolor/scalable/devices/
#
# Local install places (XDG user dirs, no sudo needed):
#   1. waffle-fuse binary        → ~/.local/bin/  (used for --probe)
#   2. waffle-nbd binary         → ~/.local/bin/
#   3. waffle-udev.sh            → ~/.local/share/waffle-fuse/
#   4. waffle-nbd-handler.sh     → ~/.local/share/waffle-fuse/
#   5. SVG icons                 → ~/.local/share/icons/hicolor/scalable/devices/
#   NOTE: udev rule requires root; hotplug will not work without it.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

RULE_SRC="$PROJECT_DIR/rules/99-waffle-fuse.rules"
POLKIT_RULE_SRC="$SCRIPT_DIR/85-waffle-nbd.rules"
UDEV_SH_SRC="$SCRIPT_DIR/waffle-udev.sh"
HANDLER_SH_SRC="$SCRIPT_DIR/waffle-nbd-handler.sh"
WAFFLE_FUSE_BIN="$PROJECT_DIR/waffle-fuse"
WAFFLE_NBD_BIN="$PROJECT_DIR/waffle-nbd"
ICON_DIR="$PROJECT_DIR/icons"

# ── Parse arguments ───────────────────────────────────────────────────────────
LOCAL=0
UNINSTALL=0
for arg in "$@"; do
    case "$arg" in
        --local)     LOCAL=1 ;;
        --uninstall) UNINSTALL=1 ;;
        --help|-h)
            sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
    esac
done

if [ "$LOCAL" = "1" ]; then
    BIN_DST_FUSE="${XDG_BIN_HOME:-$HOME/.local/bin}/waffle-fuse"
    BIN_DST_NBD="${XDG_BIN_HOME:-$HOME/.local/bin}/waffle-nbd"
    LIB_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/waffle-fuse"
    UDEV_SH_DST="$LIB_DIR/waffle-udev.sh"
    HANDLER_SH_DST="$LIB_DIR/waffle-nbd-handler.sh"
    HICOLOR_ICONS="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/scalable/devices"
    RULE_DST=""
    POLKIT_RULE_DST=""
else
    if [ "$(id -u)" -ne 0 ]; then
        echo "Error: system install requires root. Use sudo, or pass --local for a user install." >&2
        echo "  sudo $0" >&2
        echo "  $0 --local" >&2
        exit 1
    fi
    BIN_DST_FUSE="/usr/local/bin/waffle-fuse"
    BIN_DST_NBD="/usr/local/bin/waffle-nbd"
    LIB_DIR="/usr/local/lib/waffle-fuse"
    UDEV_SH_DST="$LIB_DIR/waffle-udev.sh"
    HANDLER_SH_DST="$LIB_DIR/waffle-nbd-handler.sh"
    HICOLOR_ICONS="/usr/share/icons/hicolor/scalable/devices"
    RULE_DST="/etc/udev/rules.d/99-waffle-fuse.rules"
    POLKIT_RULE_DST="/etc/polkit-1/rules.d/85-waffle-nbd.rules"
fi

# ── Uninstall ────────────────────────────────────────────────────────────────
if [ "$UNINSTALL" = "1" ]; then
    echo "Removing waffle-fuse / waffle-nbd..."
    rm -f "$BIN_DST_FUSE" "$BIN_DST_NBD" "$UDEV_SH_DST" "$HANDLER_SH_DST"
    rm -f "$HICOLOR_ICONS/amiga-floppy.svg" "$HICOLOR_ICONS/dos-floppy.svg"
    rmdir "$LIB_DIR" 2>/dev/null || true
    if [ -n "$RULE_DST" ]; then
        rm -f "$RULE_DST"
        udevadm control --reload-rules
    fi
    if [ -n "$POLKIT_RULE_DST" ]; then
        rm -f "$POLKIT_RULE_DST"
        echo "  $POLKIT_RULE_DST (removed)"
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f "${HICOLOR_ICONS%/scalable/devices}" 2>/dev/null || true
    fi
    echo "Done."
    exit 0
fi

# ── Install ──────────────────────────────────────────────────────────────────
[ -f "$UDEV_SH_SRC" ]      || { echo "Error: $UDEV_SH_SRC not found" >&2; exit 1; }
[ -f "$HANDLER_SH_SRC" ]   || { echo "Error: $HANDLER_SH_SRC not found" >&2; exit 1; }
[ -x "$WAFFLE_FUSE_BIN" ]  || { echo "Error: $WAFFLE_FUSE_BIN not found (run 'make' first)" >&2; exit 1; }
[ -x "$WAFFLE_NBD_BIN" ]   || echo "Warning: $WAFFLE_NBD_BIN not found; NBD mode will not work until built."

if [ "$LOCAL" = "1" ]; then
    echo "Installing waffle-nbd locally (user install)..."
else
    echo "Installing waffle-nbd system-wide..."
fi

# Binaries
mkdir -p "$(dirname "$BIN_DST_FUSE")"
install -m 755 "$WAFFLE_FUSE_BIN" "$BIN_DST_FUSE"
echo "  $BIN_DST_FUSE"
if [ -x "$WAFFLE_NBD_BIN" ]; then
    install -m 755 "$WAFFLE_NBD_BIN" "$BIN_DST_NBD"
    echo "  $BIN_DST_NBD"
fi

# Helper scripts
mkdir -p "$LIB_DIR"
install -m 755 "$UDEV_SH_SRC" "$UDEV_SH_DST"
echo "  $UDEV_SH_DST"
install -m 755 "$HANDLER_SH_SRC" "$HANDLER_SH_DST"
echo "  $HANDLER_SH_DST"

# udev rule (system only)
if [ -n "$RULE_DST" ]; then
    [ -f "$RULE_SRC" ] || { echo "Error: $RULE_SRC not found" >&2; exit 1; }
    install -m 644 "$RULE_SRC" "$RULE_DST"
    echo "  $RULE_DST"
    udevadm control --reload-rules
fi

# polkit rule (system only) – allows users to unmount /dev/nbd* without password
if [ -n "$POLKIT_RULE_DST" ]; then
    [ -f "$POLKIT_RULE_SRC" ] || { echo "Error: $POLKIT_RULE_SRC not found" >&2; exit 1; }
    mkdir -p "$(dirname "$POLKIT_RULE_DST")"
    install -m 644 "$POLKIT_RULE_SRC" "$POLKIT_RULE_DST"
    echo "  $POLKIT_RULE_DST"
fi

# Icons
mkdir -p "$HICOLOR_ICONS"
install -m 644 "$ICON_DIR/amiga-floppy.svg" "$HICOLOR_ICONS/amiga-floppy.svg"
install -m 644 "$ICON_DIR/dos-floppy.svg"   "$HICOLOR_ICONS/dos-floppy.svg"
echo "  $HICOLOR_ICONS/amiga-floppy.svg"
echo "  $HICOLOR_ICONS/dos-floppy.svg"
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f "${HICOLOR_ICONS%/scalable/devices}" 2>/dev/null || true
fi

echo ""
if [ "$LOCAL" = "1" ]; then
    # Add ~/.local/bin to PATH in ~/.bashrc if not already present
    BASHRC="$HOME/.bashrc"
    PATH_LINE='export PATH="$HOME/.local/bin:$PATH"'
    if [ -f "$BASHRC" ] && grep -qF '.local/bin' "$BASHRC"; then
        echo "  ~/.bashrc already contains .local/bin in PATH — skipped."
    else
        printf '\n# Added by waffle-fuse install\n%s\n' "$PATH_LINE" >> "$BASHRC"
        echo "  Added .local/bin to PATH in ~/.bashrc"
        echo "  Run: source ~/.bashrc  (or open a new terminal)"
    fi
    echo ""
    echo "Done."
    echo "Hotplug (udev) is NOT active — to enable it run:  sudo $0"
    echo "To remove:  $0 --local --uninstall"
else
    echo "Done."
    echo "  Plug in the DrawBridge / Waffle device to start the NBD server."
    echo "  View logs: cat /tmp/waffle-udev.log  or  journalctl -u waffle-ttyUSB0"
    echo "To remove:  sudo $0 --uninstall"
fi
