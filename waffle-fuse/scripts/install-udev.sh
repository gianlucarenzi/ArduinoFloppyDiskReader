#!/bin/sh
# install-udev.sh  –  install waffle-fuse / waffle-nbd and supporting files
#
# System install (requires root):
#   sudo scripts/install-udev.sh [--uninstall]
#
# Local install (no root needed — udev hotplug not available):
#   scripts/install-udev.sh --local [--uninstall]
#
# System install places:
#   1. waffle-fuse binary        → /usr/local/bin/
#   2. waffle-nbd binary         → /usr/local/bin/
#   3. waffle-udev.sh            → /usr/local/lib/waffle-fuse/
#   4. waffle-monitor.py         → /usr/local/lib/waffle-fuse/
#   5. waffle-monitor.service    → /etc/systemd/system/
#   6. udev rule                 → /etc/udev/rules.d/
#   7. polkit rule               → /etc/polkit-1/rules.d/
#   8. SVG icons                 → /usr/share/icons/hicolor/scalable/devices/
#
# Local install places (XDG user dirs, no sudo needed):
#   1. waffle-fuse binary        → ~/.local/bin/
#   2. waffle-nbd binary         → ~/.local/bin/
#   3. waffle-udev.sh            → ~/.local/lib/waffle-fuse/
#   4. waffle-monitor.py         → ~/.local/lib/waffle-fuse/
#   5. SVG icons                 → ~/.local/share/icons/hicolor/scalable/devices/
#   NOTE: udev rule + systemd service require root; hotplug will not work without them.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

RULE_SRC="$PROJECT_DIR/rules/99-waffle-fuse.rules"
POLKIT_RULE_SRC="$SCRIPT_DIR/85-waffle-nbd.rules"
UDEV_SH_SRC="$SCRIPT_DIR/waffle-udev.sh"
MONITOR_PY_SRC="$SCRIPT_DIR/waffle-monitor.py"
MONITOR_SVC_SRC="$SCRIPT_DIR/waffle-monitor.service"
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
            sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
    esac
done

if [ "$LOCAL" = "1" ]; then
    BIN_DST_FUSE="${XDG_BIN_HOME:-$HOME/.local/bin}/waffle-fuse"
    BIN_DST_NBD="${XDG_BIN_HOME:-$HOME/.local/bin}/waffle-nbd"
    LIB_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/waffle-fuse"
    UDEV_SH_DST="$LIB_DIR/waffle-udev.sh"
    MONITOR_PY_DST="$LIB_DIR/waffle-monitor.py"
    HICOLOR_ICONS="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/scalable/devices"
    RULE_DST=""
    POLKIT_RULE_DST=""
    SYSTEMD_DST=""
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
    MONITOR_PY_DST="$LIB_DIR/waffle-monitor.py"
    HICOLOR_ICONS="/usr/share/icons/hicolor/scalable/devices"
    RULE_DST="/etc/udev/rules.d/99-waffle-fuse.rules"
    POLKIT_RULE_DST="/etc/polkit-1/rules.d/85-waffle-nbd.rules"
    SYSTEMD_DST="/etc/systemd/system/waffle-monitor.service"
fi

# ── Uninstall ────────────────────────────────────────────────────────────────
if [ "$UNINSTALL" = "1" ]; then
    echo "Removing waffle-fuse / waffle-nbd / waffle-monitor..."
    # Stop and disable the systemd service before removing files
    if [ -n "$SYSTEMD_DST" ] && [ -f "$SYSTEMD_DST" ]; then
        systemctl stop    waffle-monitor.service 2>/dev/null || true
        systemctl disable waffle-monitor.service 2>/dev/null || true
        rm -f "$SYSTEMD_DST"
        systemctl daemon-reload
        echo "  $SYSTEMD_DST (removed)"
    fi
    rm -f "$BIN_DST_FUSE" "$BIN_DST_NBD" "$UDEV_SH_DST" "$MONITOR_PY_DST"
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
[ -f "$MONITOR_PY_SRC" ]   || { echo "Error: $MONITOR_PY_SRC not found" >&2; exit 1; }
[ -x "$WAFFLE_FUSE_BIN" ]  || { echo "Error: $WAFFLE_FUSE_BIN not found (run 'make' first)" >&2; exit 1; }
[ -x "$WAFFLE_NBD_BIN" ]   || echo "Warning: $WAFFLE_NBD_BIN not found; NBD mode will not work until built."

if [ "$LOCAL" = "1" ]; then
    echo "Installing waffle-fuse locally (user install)..."
else
    echo "Installing waffle-fuse system-wide..."
fi

# Binaries
mkdir -p "$(dirname "$BIN_DST_FUSE")"
install -m 755 "$WAFFLE_FUSE_BIN" "$BIN_DST_FUSE"
echo "  $BIN_DST_FUSE"
if [ -x "$WAFFLE_NBD_BIN" ]; then
    install -m 755 "$WAFFLE_NBD_BIN" "$BIN_DST_NBD"
    echo "  $BIN_DST_NBD"
fi

# Helper script (waffle-udev.sh)
mkdir -p "$LIB_DIR"
install -m 755 "$UDEV_SH_SRC" "$UDEV_SH_DST"
echo "  $UDEV_SH_DST"

# Python monitor (waffle-monitor.py)
install -m 755 "$MONITOR_PY_SRC" "$MONITOR_PY_DST"
echo "  $MONITOR_PY_DST"

# systemd service (system only)
if [ -n "$SYSTEMD_DST" ] && [ -f "$MONITOR_SVC_SRC" ]; then
    install -m 644 "$MONITOR_SVC_SRC" "$SYSTEMD_DST"
    echo "  $SYSTEMD_DST"
    systemctl daemon-reload
    systemctl enable --now waffle-monitor.service
    echo "  waffle-monitor.service enabled and started"
fi

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
    echo "Hotplug (udev + systemd service) is NOT active — to enable it run:  sudo $0"
    echo "To remove:  $0 --local --uninstall"
else
    echo "Done."
    echo "  waffle-monitor.service is running and will auto-start at boot."
    echo "  Plug in the DrawBridge / Waffle device to start the NBD server."
    echo "  View logs: journalctl -u waffle-monitor -f"
    echo "To remove:  sudo $0 --uninstall"
fi
