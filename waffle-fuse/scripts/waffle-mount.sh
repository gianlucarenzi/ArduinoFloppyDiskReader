#!/bin/sh
# waffle-mount.sh – one-click mount/unmount for waffle-fuse (desktop launchers)
#
# Usage: waffle-mount.sh [auto|amiga|dos] [auto|dd|hd] [/dev/ttyUSBx]
#
# Toggle: if a disk from the drive is mounted it is unmounted (pending
# changes are written first), otherwise the inserted disk is mounted under
# $WAFFLE_MOUNT_DIR/<volume name> (default /media/$USER or /run/media/$USER if
# writable, else ~/Floppy), where it shows up as a
# volume on the desktop.  For automatic mounting on insert use the tray
# daemon (waffle-diskflashback.py) instead.

FORMAT="${1:-auto}"    # auto | amiga | dos
DENSITY="${2:-auto}"   # auto | dd | hd
PORT="${3:-}"
# Default: where udisks mounts (/run/media/$USER or /media/$USER) if we may
# create directories there
# (see install-system.sh), otherwise ~/Floppy
MOUNT_DIR="${WAFFLE_MOUNT_DIR:-}"
if [ -z "$MOUNT_DIR" ]; then
    MOUNT_DIR="$HOME/Floppy"
    for base in /run/media /media; do
        if [ -d "$base/$USER" ] && [ -w "$base/$USER" ]; then MOUNT_DIR="$base/$USER"; break; fi
    done
fi

# ── Notifications ────────────────────────────────────────────────────────────
notify() {
    if command -v notify-send >/dev/null 2>&1; then
        notify-send -a Waffle -i "${3:-media-floppy}" "$1" "$2"
    elif command -v zenity >/dev/null 2>&1; then
        zenity --info --title="$1" --text="$2" &
    else
        echo "$1: $2" >&2
    fi
}
notify_error() { notify "$1" "$2" dialog-error; }

# ── waffle-fuse binary ───────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WAFFLE_BIN="$SCRIPT_DIR/../waffle-fuse"
[ -x "$WAFFLE_BIN" ] || WAFFLE_BIN="waffle-fuse"

# ── Serial port: first one that answers like a DrawBridge ────────────────────
if [ -z "$PORT" ]; then
    for candidate in /dev/serial/by-id/* /dev/ttyUSB* /dev/ttyACM*; do
        [ -e "$candidate" ] || continue
        "$WAFFLE_BIN" --probe "$candidate" >/dev/null 2>&1
        case $? in 0|1) PORT="$candidate"; break ;; esac
    done
fi
if [ -z "$PORT" ]; then
    notify_error "Waffle" "DrawBridge not found (is it plugged in, or in use by another program?)"
    exit 1
fi

# ── Toggle: unmount if mounted ───────────────────────────────────────────────
# /proc/mounts escapes spaces as \040
MNT="$(awk -v src="waffle:$PORT" '$1 == src { print $2; exit }' /proc/mounts | sed 's/\\040/ /g')"
if [ -n "$MNT" ]; then
    if ! fusermount3 -u "$MNT" 2>/dev/null; then
        notify_error "Disk in use" "Close the windows and terminals using $MNT, then try again."
        exit 1
    fi
    # waffle-fuse writes pending changes, then removes the mountpoint and exits
    i=0
    while [ -d "$MNT" ] && [ $i -lt 120 ]; do sleep 1; i=$((i+1)); done
    notify "Disk ejected" "All changes written: the disk can be removed." media-eject
    exit 0
fi

# ── Mount ────────────────────────────────────────────────────────────────────
OPT=""
case "$FORMAT"  in amiga) OPT="format=affs" ;; dos) OPT="format=fat" ;; esac
case "$DENSITY" in dd|hd) OPT="${OPT:+$OPT,}density=$DENSITY" ;; esac

# Without -f waffle-fuse detects the disk, mounts, then goes to the
# background: its exit code tells whether the mount worked.
if [ -n "$OPT" ]; then
    OUT="$("$WAFFLE_BIN" "$PORT" --automount "$MOUNT_DIR" -o "$OPT" 2>&1)"
else
    OUT="$("$WAFFLE_BIN" "$PORT" --automount "$MOUNT_DIR" 2>&1)"
fi
RC=$?

case $RC in
    0)
        MNT="$(printf '%s\n' "$OUT" | sed -n 's/^WAFFLE event=mounted mountpoint=//p')"
        VOL="$(basename "$MNT")"
        notify "Disk inserted" "$VOL"
        if command -v xdg-open >/dev/null 2>&1; then xdg-open "$MNT" & fi
        ;;
    2) notify_error "Waffle" "Cannot open $PORT (in use by another program?)" ;;
    3) notify_error "Waffle" "No disk in the drive." ;;
    4) notify_error "Cannot use this disk" "Not a DOS disk: unformatted, NDOS (game) or copy-protected." ;;
    5) notify_error "Cannot use this disk" "Cannot read track 0: the disk may be damaged." ;;
    6) notify_error "Cannot use this disk" "The filesystem on the disk is not valid." ;;
    *) notify_error "Waffle" "Mount failed: $(printf '%s\n' "$OUT" | grep -v '^WAFFLE' | tail -1)" ;;
esac
exit $RC
