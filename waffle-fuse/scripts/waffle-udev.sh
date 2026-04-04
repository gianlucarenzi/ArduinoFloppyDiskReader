#!/bin/sh
# waffle-udev.sh  –  udev helper for automatic Waffle / DrawBridge mounting
#
# Called as root by /etc/udev/rules.d/99-waffle-fuse.rules.
# Identifies the active graphical user, probes the port to confirm a Waffle
# device is responding, then mounts (add) or unmounts with flush (remove).
#
# Usage (by udev): waffle-udev.sh add|remove /dev/ttyUSBx

ACTION="$1"   # add | remove
PORT="$2"     # full device path, e.g. /dev/ttyUSB0

# Log everything from the start so failures are diagnosable.
UDEV_LOG="/tmp/waffle-udev.log"
exec >>"$UDEV_LOG" 2>&1
echo "$(date '+%F %T') waffle-udev[$ACTION] port=$PORT"

[ -n "$ACTION" ] && [ -n "$PORT" ] || { echo "ERROR: missing args"; exit 1; }

# ── Locate waffle-fuse binary ─────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# Installed layout: /usr/local/lib/waffle-fuse/waffle-udev.sh  (SCRIPT_DIR)
#                   /usr/local/bin/waffle-fuse                  (PREFIX/bin)
# PREFIX is two levels up from SCRIPT_DIR (/usr/local/lib/waffle-fuse -> /usr/local)
PREFIX="$(dirname "$(dirname "$SCRIPT_DIR")")"
WAFFLE_BIN="$PREFIX/bin/waffle-fuse"
# Fallback: dev-tree layout (scripts/ sits inside the project, binary at project root)
[ -x "$WAFFLE_BIN" ] || WAFFLE_BIN="$SCRIPT_DIR/../../waffle-fuse"
[ -x "$WAFFLE_BIN" ] || WAFFLE_BIN="$(command -v waffle-fuse 2>/dev/null)"
[ -x "$WAFFLE_BIN" ] || { echo "ERROR: waffle-fuse not found (looked in $PREFIX/bin)"; exit 1; }
echo "  waffle-fuse binary: $WAFFLE_BIN"

# ── Find the first active non-root user session ───────────────────────────────
find_active_user() {
    if command -v loginctl >/dev/null 2>&1; then
        # 1st pass: prefer graphical sessions
        RESULT=$(loginctl list-sessions --no-legend 2>/dev/null | \
        while read -r SID _ UNAME _; do
            [ "$UNAME" = "root" ] && continue
            TYPE=$(loginctl show-session "$SID" -p Type --value 2>/dev/null)
            case "$TYPE" in
                x11|wayland|mir) printf '%s' "$UNAME"; break ;;
            esac
        done)
        [ -n "$RESULT" ] && { printf '%s' "$RESULT"; return; }
        # 2nd pass: any non-root session (tty, unset, etc.)
        RESULT=$(loginctl list-sessions --no-legend 2>/dev/null | \
        while read -r SID _ UNAME _; do
            [ "$UNAME" = "root" ] && continue
            [ -z "$UNAME" ] && continue
            printf '%s' "$UNAME"; break
        done)
        [ -n "$RESULT" ] && { printf '%s' "$RESULT"; return; }
    fi
    # Fallback 1: who with a display (:0, :1 …)
    RESULT=$(who 2>/dev/null | awk '/\(:[0-9]/ { print $1; exit }')
    [ -n "$RESULT" ] && { printf '%s' "$RESULT"; return; }
    # Fallback 2: first non-root logged-in user
    who 2>/dev/null | awk '$1 != "root" { print $1; exit }'
}

SESSION_USER=$(find_active_user)
echo "  session_user='$SESSION_USER'"
[ -n "$SESSION_USER" ] || { echo "ERROR: no active user session found"; exit 0; }

SESSION_UID=$(id -u "$SESSION_USER" 2>/dev/null) || exit 0
SESSION_HOME=$(getent passwd "$SESSION_USER" | cut -d: -f6)
DBUS_ADDR="unix:path=/run/user/${SESSION_UID}/bus"
XDG_RT="/run/user/${SESSION_UID}"

PORT_BASE="$(basename "$PORT")"
MNT="/tmp/waffle-${PORT_BASE}"

case "$ACTION" in

# ── Plug-in: probe → mount ────────────────────────────────────────────────────
add)
    # Confirm the device actually speaks the Waffle protocol (not just any
    # FTDI chip).  Run as root (we already have full device access).
    # Exit code 0=disk present, 1=no disk, 2+=not a Waffle.
    "$WAFFLE_BIN" --probe "$PORT" >/dev/null 2>&1
    PROBE_INIT=$?
    echo "  initial probe exit=$PROBE_INIT"
    case $PROBE_INIT in
        0|1) ;;          # Waffle confirmed (disk present or absent)
        *)   echo "  not a Waffle device, ignoring"; exit 0 ;;
    esac

    # Wait for a disk to be inserted, polling every 2 seconds.
    # Exit code 0=disk present, 1=no disk, 2+=error/disconnected.
    echo "  waiting for disk..."
    while true; do
        "$WAFFLE_BIN" --probe "$PORT" >/dev/null 2>&1
        PROBE=$?
        [ "$PROBE" -eq 0 ] && break        # disk inserted → proceed
        [ "$PROBE" -gt 1 ] && { echo "  probe error ($PROBE), aborting"; exit 0; }
        sleep 2
    done
    echo "  disk present, mounting at $MNT"

    # Give the Arduino firmware a moment to settle after the last probe
    # before waffle-fuse opens the port for full operation.
    sleep 2

    mkdir -p "$MNT"
    chown "${SESSION_USER}:" "$MNT"

    # Launch waffle-fuse as the user.
    # No format= or density= → full auto-detection in waffle_init().
    # waffle_init() adds the GVfs bookmark and a ~/Desktop eject shortcut.
    export WFUDEV_MNT="$MNT"
    export WFUDEV_HOME="$SESSION_HOME"
    export WFUDEV_DBUS="$DBUS_ADDR"
    export WFUDEV_XDG="$XDG_RT"
    WFUDEV_LOG="/tmp/waffle-fuse-$(basename "$PORT").log"
    export WFUDEV_LOG
    su -s /bin/sh "$SESSION_USER" -c '
        export HOME="$WFUDEV_HOME"
        export DBUS_SESSION_BUS_ADDRESS="$WFUDEV_DBUS"
        export XDG_RUNTIME_DIR="$WFUDEV_XDG"
        # -f (foreground) impedisce il double-fork interno di FUSE che chiude
        # stdin/stdout/stderr. setsid + & gestiscono il distacco dalla sessione.
        setsid "$WFUDEV_BIN" "$WFUDEV_PORT" "$WFUDEV_MNT" \
            -f -o "fsname=waffle:$WFUDEV_PORT,subtype=waffle" \
            >"$WFUDEV_LOG" 2>&1 &
        # Wait up to 30 s for the mount to appear, then open the file manager
        i=0
        while [ "$i" -lt 30 ]; do
            mountpoint -q "$WFUDEV_MNT" 2>/dev/null && break
            sleep 1; i=$((i+1))
        done
        mountpoint -q "$WFUDEV_MNT" 2>/dev/null && \
            xdg-open "$WFUDEV_MNT" >/dev/null 2>&1 &
    '
    ;;

# ── Unplug: flush → unmount ───────────────────────────────────────────────────
remove)
    # Identify the mount by its fsname (set at mount time as waffle:/dev/ttyUSBx).
    # fusermount triggers waffle_destroy() which flushes all dirty tracks to
    # hardware before the serial port disappears, then removes the GVfs bookmark
    # and the ~/Desktop eject shortcut.
    grep "waffle:${PORT} " /proc/mounts 2>/dev/null | awk '{ print $2 }' | \
    while read -r MNT_FOUND; do
        export WFUDEV_MNT="$MNT_FOUND"
        export WFUDEV_HOME="$SESSION_HOME"
        export WFUDEV_DBUS="$DBUS_ADDR"
        export WFUDEV_XDG="$XDG_RT"
        su -s /bin/sh "$SESSION_USER" -c '
            export HOME="$WFUDEV_HOME"
            export DBUS_SESSION_BUS_ADDRESS="$WFUDEV_DBUS"
            export XDG_RUNTIME_DIR="$WFUDEV_XDG"
            fusermount3 -u "$WFUDEV_MNT" 2>/dev/null \
                || fusermount -u "$WFUDEV_MNT" 2>/dev/null \
                || true
        '
        sleep 1   # allow waffle_destroy() to complete the flush
        rmdir "$MNT_FOUND" 2>/dev/null || true
    done
    ;;

esac

