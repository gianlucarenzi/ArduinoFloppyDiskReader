#!/bin/sh
# install-udev.sh  –  install the waffle-fuse udev rule and helper script
#
# System install (requires root):
#   sudo scripts/install-udev.sh [--uninstall]
#
# Local install (no root needed — udev hotplug not available):
#   scripts/install-udev.sh --local [--uninstall]
#
# System install places:
#   1. waffle-fuse binary  → /usr/local/bin/
#   2. waffle-udev.sh      → /usr/local/lib/waffle-fuse/
#   3. udev rule           → /etc/udev/rules.d/
#   4. SVG icons           → /usr/share/icons/hicolor/scalable/devices/
#
# Local install places (XDG user dirs, no sudo needed):
#   1. waffle-fuse binary  → ~/.local/bin/
#   2. waffle-udev.sh      → ~/.local/lib/waffle-fuse/
#   3. SVG icons           → ~/.local/share/icons/hicolor/scalable/devices/
#   NOTE: udev rule requires root; hotplug will not work without it.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

RULE_SRC="$PROJECT_DIR/rules/99-waffle-fuse.rules"
UDEV_SH_SRC="$SCRIPT_DIR/waffle-udev.sh"
WAFFLE_BIN="$PROJECT_DIR/waffle-fuse"
ICON_DIR="$PROJECT_DIR/icons"

# ── Parse arguments ───────────────────────────────────────────────────────────
LOCAL=0
UNINSTALL=0
for arg in "$@"; do
    case "$arg" in
        --local)     LOCAL=1 ;;
        --uninstall) UNINSTALL=1 ;;
        --help|-h)
            sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
    esac
done

if [ "$LOCAL" = "1" ]; then
    BIN_DST="${XDG_BIN_HOME:-$HOME/.local/bin}/waffle-fuse"
    LIB_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/waffle-fuse"
    UDEV_SH_DST="$LIB_DIR/waffle-udev.sh"
    HICOLOR_ICONS="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/scalable/devices"
    RULE_DST=""
else
    if [ "$(id -u)" -ne 0 ]; then
        echo "Error: system install requires root. Use sudo, or pass --local for a user install." >&2
        echo "  sudo $0" >&2
        echo "  $0 --local" >&2
        exit 1
    fi
    BIN_DST="/usr/local/bin/waffle-fuse"
    LIB_DIR="/usr/local/lib/waffle-fuse"
    UDEV_SH_DST="$LIB_DIR/waffle-udev.sh"
    HICOLOR_ICONS="/usr/share/icons/hicolor/scalable/devices"
    RULE_DST="/etc/udev/rules.d/99-waffle-fuse.rules"
fi

# ── Uninstall ────────────────────────────────────────────────────────────────
if [ "$UNINSTALL" = "1" ]; then
    echo "Removing waffle-fuse..."
    rm -f "$BIN_DST" "$UDEV_SH_DST"
    rm -f "$HICOLOR_ICONS/amiga-floppy.svg" "$HICOLOR_ICONS/dos-floppy.svg"
    rmdir "$LIB_DIR" 2>/dev/null || true
    if [ -n "$RULE_DST" ]; then
        rm -f "$RULE_DST"
        udevadm control --reload-rules
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f "${HICOLOR_ICONS%/scalable/devices}" 2>/dev/null || true
    fi
    echo "Done."
    exit 0
fi

# ── Install ──────────────────────────────────────────────────────────────────
[ -f "$UDEV_SH_SRC" ] || { echo "Error: $UDEV_SH_SRC not found" >&2; exit 1; }
[ -x "$WAFFLE_BIN" ]  || { echo "Error: $WAFFLE_BIN not found (run 'make' first)" >&2; exit 1; }

if [ "$LOCAL" = "1" ]; then
    echo "Installing waffle-fuse locally (user install)..."
else
    echo "Installing waffle-fuse system-wide..."
fi

# Binary
mkdir -p "$(dirname "$BIN_DST")"
install -m 755 "$WAFFLE_BIN" "$BIN_DST"
echo "  $BIN_DST"

# Helper script
mkdir -p "$LIB_DIR"
install -m 755 "$UDEV_SH_SRC" "$UDEV_SH_DST"
echo "  $UDEV_SH_DST"

# udev rule (system only)
if [ -n "$RULE_DST" ]; then
    [ -f "$RULE_SRC" ] || { echo "Error: $RULE_SRC not found" >&2; exit 1; }
    install -m 644 "$RULE_SRC" "$RULE_DST"
    echo "  $RULE_DST"
    udevadm control --reload-rules
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
    LOCAL_BIN="${XDG_BIN_HOME:-$HOME/.local/bin}"
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
    echo "Done. Plug in the DrawBridge / Waffle device to trigger auto-mount."
    echo "To remove:  sudo $0 --uninstall"
fi
