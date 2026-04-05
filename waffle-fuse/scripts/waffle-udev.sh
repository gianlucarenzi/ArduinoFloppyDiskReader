#!/bin/sh
# waffle-udev.sh  –  udev helper for automatic Waffle / DrawBridge mounting
#
# Called as root by /etc/udev/rules.d/99-waffle-fuse.rules.
# Identifies the active graphical user, then starts/stops the NBD handler.
#
# Usage (by udev): waffle-udev.sh add|remove /dev/ttyUSBx

ACTION="$1"   # add | remove
PORT="$2"     # full device path, e.g. /dev/ttyUSB0

UDEV_LOG="/tmp/waffle-udev.log"
exec >>"$UDEV_LOG" 2>&1
echo "$(date '+%F %T') waffle-udev[$ACTION] port=$PORT"

[ -n "$ACTION" ] && [ -n "$PORT" ] || { echo "ERROR: missing args"; exit 1; }

# ── Locate binaries ───────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PREFIX="$(dirname "$(dirname "$SCRIPT_DIR")")"

find_bin() {
    NAME="$1"
    BIN="$PREFIX/bin/$NAME"
    [ -x "$BIN" ] || BIN="$SCRIPT_DIR/../../$NAME"
    [ -x "$BIN" ] || BIN="$(command -v "$NAME" 2>/dev/null)"
    printf '%s' "$BIN"
}

WAFFLE_FUSE_BIN="$(find_bin waffle-fuse)"

# waffle-fuse is needed for --probe.
[ -x "$WAFFLE_FUSE_BIN" ] || { echo "ERROR: waffle-fuse not found"; exit 1; }
echo "  waffle-fuse: $WAFFLE_FUSE_BIN"

# ── Find the first active non-root user session ───────────────────────────────
find_active_user() {
    if command -v loginctl >/dev/null 2>&1; then
        RESULT=$(loginctl list-sessions --no-legend 2>/dev/null | \
        while read -r SID _ UNAME _; do
            [ "$UNAME" = "root" ] && continue
            TYPE=$(loginctl show-session "$SID" -p Type --value 2>/dev/null)
            case "$TYPE" in
                x11|wayland|mir) printf '%s' "$UNAME"; break ;;
            esac
        done)
        [ -n "$RESULT" ] && { printf '%s' "$RESULT"; return; }
        RESULT=$(loginctl list-sessions --no-legend 2>/dev/null | \
        while read -r SID _ UNAME _; do
            [ "$UNAME" = "root" ] && continue
            [ -z "$UNAME" ] && continue
            printf '%s' "$UNAME"; break
        done)
        [ -n "$RESULT" ] && { printf '%s' "$RESULT"; return; }
    fi
    RESULT=$(who 2>/dev/null | awk '/\(:[0-9]/ { print $1; exit }')
    [ -n "$RESULT" ] && { printf '%s' "$RESULT"; return; }
    who 2>/dev/null | awk '$1 != "root" { print $1; exit }'
}

SESSION_USER=$(find_active_user)
echo "  session_user='$SESSION_USER'"
[ -n "$SESSION_USER" ] || { echo "ERROR: no active user session found"; exit 0; }

SESSION_UID=$(id -u "$SESSION_USER" 2>/dev/null) || exit 0
SESSION_HOME=$(getent passwd "$SESSION_USER" | cut -d: -f6)

PORT_BASE="$(basename "$PORT")"
UNIT="waffle-${PORT_BASE}"

case "$ACTION" in

# ── Plug-in ───────────────────────────────────────────────────────────────────
add)
    # Confirm the device speaks the Waffle protocol.
    "$WAFFLE_FUSE_BIN" --probe "$PORT" >/dev/null 2>&1
    PROBE_INIT=$?
    echo "  initial probe exit=$PROBE_INIT"
    case $PROBE_INIT in
        0|1) ;;
        *)   echo "  not a Waffle device, ignoring"; exit 0 ;;
    esac

    # Stop any leftover unit from a previous run.
    systemctl stop  "${UNIT}.service" 2>/dev/null || true
    systemctl reset-failed "${UNIT}.service" 2>/dev/null || true

    WAFFLE_HANDLER="$SCRIPT_DIR/waffle-nbd-handler.sh"
    [ -x "$WAFFLE_HANDLER" ] || { echo "ERROR: waffle-nbd-handler.sh not found"; exit 1; }

    systemd-run \
        --unit="$UNIT" \
        -- \
        "$WAFFLE_HANDLER" "$PORT" "$SESSION_USER"
    echo "  systemd-run (NBD handler) exit=$?"
    ;;

# ── Unplug ────────────────────────────────────────────────────────────────────
remove)
    systemctl stop "${UNIT}.service" 2>/dev/null || true
    ;;

esac
