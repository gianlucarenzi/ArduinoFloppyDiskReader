#!/bin/sh
# waffle-mount.sh – portable mount/unmount helper for waffle-fuse
#
# Usage: waffle-mount.sh [amiga|dos] [dd|hd] [/dev/ttyUSBx]
#
# Defaults: format=amiga, density=dd, port=auto-detect

set -e

# ── Arguments ────────────────────────────────────────────────────────────────
FORMAT="${1:-amiga}"   # amiga | dos
DENSITY="${2:-dd}"     # dd | hd
PORT="${3:-}"

# ── Portable notification helper ─────────────────────────────────────────────
notify() {
    TITLE="$1"
    MSG="$2"
    ICON="${3:-dialog-information}"   # freedesktop icon name
    if command -v notify-send >/dev/null 2>&1; then
        notify-send -i "$ICON" "$TITLE" "$MSG"
    elif command -v zenity >/dev/null 2>&1; then
        zenity --info --title="$TITLE" --text="$MSG" &
    elif command -v kdialog >/dev/null 2>&1; then
        kdialog --title "$TITLE" --passivepopup "$MSG" 4
    elif command -v xmessage >/dev/null 2>&1; then
        xmessage -timeout 4 "$TITLE: $MSG" &
    else
        echo "$TITLE: $MSG" >&2
    fi
}

notify_error() {
    TITLE="$1"
    MSG="$2"
    if command -v notify-send >/dev/null 2>&1; then
        notify-send -i dialog-error "$TITLE" "$MSG"
    elif command -v zenity >/dev/null 2>&1; then
        zenity --error --title="$TITLE" --text="$MSG" &
    elif command -v kdialog >/dev/null 2>&1; then
        kdialog --title "$TITLE" --error "$MSG"
    elif command -v xmessage >/dev/null 2>&1; then
        xmessage -timeout 6 "ERROR – $TITLE: $MSG" &
    else
        echo "ERROR – $TITLE: $MSG" >&2
    fi
}

# ── Auto-detect serial port ───────────────────────────────────────────────────
if [ -z "$PORT" ]; then
    for candidate in /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyACM0 /dev/ttyACM1; do
        if [ -e "$candidate" ]; then
            PORT="$candidate"
            break
        fi
    done
fi

if [ -z "$PORT" ]; then
    notify_error "waffle-fuse" "No serial port found (tried /dev/ttyUSB0..1, /dev/ttyACM0..1)"
    exit 1
fi

# ── Mountpoint ───────────────────────────────────────────────────────────────
case "$FORMAT" in
    amiga) LABEL="Amiga_Floppy" ;;
    dos)   LABEL="DOS_Floppy"   ;;
    *)     LABEL="Floppy"       ;;
esac

MNT="/tmp/waffle-${LABEL}"

# ── Find waffle-fuse binary ───────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WAFFLE_BIN="$SCRIPT_DIR/../waffle-fuse"
if [ ! -x "$WAFFLE_BIN" ]; then
    # Fall back to PATH
    WAFFLE_BIN="waffle-fuse"
fi

# ── Toggle: unmount if already mounted ───────────────────────────────────────
if mountpoint -q "$MNT" 2>/dev/null; then
    fusermount3 -u "$MNT" 2>/dev/null || fusermount -u "$MNT" 2>/dev/null || umount "$MNT" 2>/dev/null
    notify "waffle-fuse" "Disk unmounted from $MNT" "media-eject"
    rmdir "$MNT" 2>/dev/null || true
    exit 0
fi

# ── Mount ─────────────────────────────────────────────────────────────────────
mkdir -p "$MNT"

case "$FORMAT" in
    amiga) FUSE_FORMAT="affs" ;;
    dos)   FUSE_FORMAT="fat"  ;;
    *)     FUSE_FORMAT=""     ;;
esac

OPT=""
[ -n "$FUSE_FORMAT" ] && OPT="format=${FUSE_FORMAT}"
[ -n "$DENSITY" ]     && OPT="${OPT:+$OPT,}density=${DENSITY}"

if [ -n "$OPT" ]; then
    "$WAFFLE_BIN" "$PORT" "$MNT" -o "$OPT" &
else
    "$WAFFLE_BIN" "$PORT" "$MNT" &
fi

WAFFLE_PID=$!

# Wait for mount to become active (up to 60 s)
i=0
while [ $i -lt 60 ]; do
    if mountpoint -q "$MNT" 2>/dev/null; then
        break
    fi
    # Check the process is still alive
    if ! kill -0 "$WAFFLE_PID" 2>/dev/null; then
        notify_error "waffle-fuse" "Driver exited unexpectedly. Check that $PORT is connected and the disk is inserted."
        rmdir "$MNT" 2>/dev/null || true
        exit 1
    fi
    sleep 1
    i=$((i+1))
done

if ! mountpoint -q "$MNT" 2>/dev/null; then
    notify_error "waffle-fuse" "Mount timed out on $PORT"
    kill "$WAFFLE_PID" 2>/dev/null || true
    rmdir "$MNT" 2>/dev/null || true
    exit 1
fi

notify "waffle-fuse" "${LABEL} mounted at $MNT" "media-floppy"

# Open file manager if available
if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "$MNT" &
elif command -v thunar >/dev/null 2>&1; then
    thunar "$MNT" &
elif command -v nautilus >/dev/null 2>&1; then
    nautilus "$MNT" &
elif command -v dolphin >/dev/null 2>&1; then
    dolphin "$MNT" &
fi
