#!/bin/sh
# waffle-udev.sh  –  udev helper for automatic Waffle / DrawBridge mounting
#
# Called as root by /etc/udev/rules.d/99-waffle-fuse.rules.
# Identifies the active graphical user, reads ~/.config/waffle.mode to
# choose the backend (FUSE or NBD; default is NBD when file is absent),
# then starts/stops the appropriate service.
#
# Mode file: $HOME/.config/waffle.mode
#   Contents: FUSE   → use waffle-fuse (FUSE filesystem)
#             NBD    → use waffle-nbd  (Network Block Device)
#   Missing  → NBD (default)
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
WAFFLE_NBD_BIN="$(find_bin waffle-nbd)"

# We always need waffle-fuse for --probe.
[ -x "$WAFFLE_FUSE_BIN" ] || { echo "ERROR: waffle-fuse not found"; exit 1; }
echo "  waffle-fuse: $WAFFLE_FUSE_BIN"
echo "  waffle-nbd:  ${WAFFLE_NBD_BIN:-<not found>}"

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
SESSION_GID=$(id -g "$SESSION_USER" 2>/dev/null)
SESSION_HOME=$(getent passwd "$SESSION_USER" | cut -d: -f6)
DBUS_ADDR="unix:path=/run/user/${SESSION_UID}/bus"
XDG_RT="/run/user/${SESSION_UID}"

# ── Read mode from ~/.config/waffle.mode (default: NBD) ──────────────────────
MODE_FILE="${SESSION_HOME}/.config/waffle.mode"
if [ -f "$MODE_FILE" ]; then
    MODE=$(tr '[:lower:]' '[:upper:]' < "$MODE_FILE" | tr -d '[:space:]')
else
    MODE="NBD"
fi
case "$MODE" in
    FUSE|NBD) ;;
    *) echo "  unknown mode '$MODE' in $MODE_FILE, defaulting to NBD"; MODE="NBD" ;;
esac
echo "  mode=$MODE"

PORT_BASE="$(basename "$PORT")"
MNT="/tmp/waffle-${PORT_BASE}"
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

    if [ "$MODE" = "NBD" ]; then
        # ── NBD mode ──────────────────────────────────────────────────────────
        # waffle-nbd manages its own probe/lifecycle internally.
        [ -x "$WAFFLE_NBD_BIN" ] || { echo "ERROR: waffle-nbd not found"; exit 1; }

        systemd-run \
            --unit="$UNIT" \
            --uid="$SESSION_UID" \
            --gid="$SESSION_GID" \
            --setenv="HOME=$SESSION_HOME" \
            --setenv="DBUS_SESSION_BUS_ADDRESS=$DBUS_ADDR" \
            --setenv="XDG_RUNTIME_DIR=$XDG_RT" \
            -- \
            "$WAFFLE_NBD_BIN" "$PORT"
        echo "  systemd-run (NBD) exit=$?"

    else
        # ── FUSE mode ─────────────────────────────────────────────────────────
        # Wait for a disk to be inserted before mounting.
        echo "  waiting for disk..."
        while true; do
            "$WAFFLE_FUSE_BIN" --probe "$PORT" >/dev/null 2>&1
            PROBE=$?
            [ "$PROBE" -eq 0 ] && break
            [ "$PROBE" -gt 1 ] && { echo "  probe error ($PROBE), aborting"; exit 0; }
            sleep 2
        done
        echo "  disk present, mounting at $MNT"
        sleep 2

        mkdir -p "$MNT"
        chown "${SESSION_USER}:" "$MNT"

        systemd-run \
            --unit="$UNIT" \
            --uid="$SESSION_UID" \
            --gid="$SESSION_GID" \
            --setenv="HOME=$SESSION_HOME" \
            --setenv="DBUS_SESSION_BUS_ADDRESS=$DBUS_ADDR" \
            --setenv="XDG_RUNTIME_DIR=$XDG_RT" \
            -- \
            "$WAFFLE_FUSE_BIN" "$PORT" "$MNT" \
                -f -o "fsname=waffle:${PORT},subtype=waffle"
        echo "  systemd-run (FUSE) exit=$?"

        i=0
        while [ "$i" -lt 30 ]; do
            mountpoint -q "$MNT" 2>/dev/null && break
            sleep 1; i=$((i+1))
        done
        if mountpoint -q "$MNT" 2>/dev/null; then
            su -s /bin/sh "$SESSION_USER" -c "
                export HOME='$SESSION_HOME'
                export DBUS_SESSION_BUS_ADDRESS='$DBUS_ADDR'
                export XDG_RUNTIME_DIR='$XDG_RT'
                xdg-open '$MNT' >/dev/null 2>&1 &
            "
        else
            echo "  WARNING: mount not active after 30 s"
        fi
    fi
    ;;

# ── Unplug ────────────────────────────────────────────────────────────────────
remove)
    systemctl stop "${UNIT}.service" 2>/dev/null || true
    sleep 1

    # Clean up any stale FUSE mounts.
    grep "waffle:${PORT} " /proc/mounts 2>/dev/null | awk '{ print $2 }' | \
    while read -r MNT_FOUND; do
        systemd-run --scope -- umount -l "$MNT_FOUND" 2>/dev/null || true
        rmdir "$MNT_FOUND" 2>/dev/null || true
    done
    rmdir "$MNT" 2>/dev/null || true
    ;;

esac
