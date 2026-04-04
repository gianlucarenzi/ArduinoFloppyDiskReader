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

# ── Find waffle-fuse binary (needed for --probe below) ───────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WAFFLE_BIN="$SCRIPT_DIR/../waffle-fuse"
if [ ! -x "$WAFFLE_BIN" ]; then
    WAFFLE_BIN="waffle-fuse"
fi

# ── Mountpoint & desktop-integration labels ──────────────────────────────────
case "$FORMAT-$DENSITY" in
    amiga-dd) LABEL="Amiga_Floppy"; DISPLAY_LABEL="Amiga Floppy DD 880K"  ;;
    amiga-hd) LABEL="Amiga_Floppy"; DISPLAY_LABEL="Amiga Floppy HD 1.76M" ;;
    dos-dd)   LABEL="DOS_Floppy";   DISPLAY_LABEL="DOS Floppy DD 720K"    ;;
    dos-hd)   LABEL="DOS_Floppy";   DISPLAY_LABEL="DOS Floppy HD 1.44M"   ;;
    *)        LABEL="Floppy";       DISPLAY_LABEL="Floppy"                 ;;
esac

MNT="/tmp/waffle-${LABEL}"

# ── Toggle: unmount if already mounted ───────────────────────────────────────
# Checked early so we never need to probe the serial port just to unmount.
# waffle-fuse removes the GVfs bookmark from waffle_destroy() automatically,
# regardless of how the unmount is triggered.
if mountpoint -q "$MNT" 2>/dev/null; then
    fusermount3 -u "$MNT" 2>/dev/null || fusermount -u "$MNT" 2>/dev/null || umount "$MNT" 2>/dev/null
    notify "waffle-fuse" "Disk unmounted from $MNT" "media-eject"
    rmdir "$MNT" 2>/dev/null || true
    exit 0
fi

# ── Auto-detect serial port via --probe ──────────────────────────────────────
# Iterate over every ttyUSB* and ttyACM* character device.  For each one,
# ask waffle-fuse to open the port at 2 Mbit and exchange a version command.
# Exit codes: 0=disk present, 1=no disk, 2+=error / not a Waffle device.
# Codes 0 and 1 both confirm a Waffle is responding on that port.
if [ -z "$PORT" ]; then
    set +e   # probe returns non-zero for non-Waffle ports; don't abort
    for candidate in /dev/ttyUSB* /dev/ttyACM*; do
        [ -c "$candidate" ] || continue          # skip if not a char device
        "$WAFFLE_BIN" --probe "$candidate" >/dev/null 2>&1
        case $? in
            0|1) PORT="$candidate"; break ;;     # Waffle found (disk or no disk)
        esac
    done
    set -e
fi

if [ -z "$PORT" ]; then
    notify_error "waffle-fuse" "No Waffle device found on any serial port (probed /dev/ttyUSB* and /dev/ttyACM*)"
    exit 1
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

# Desktop integration: advertise the mount to GVfs-based file managers
# (Nautilus, Thunar, Nemo) via x-gvfs-* options, and to KDE Solid (Dolphin)
# via the fsname which appears in /proc/mounts.
# fsname uses the serial port so concurrent mounts on different ports are unique.
OPT="${OPT:+$OPT,}fsname=waffle:${PORT},subtype=waffle"

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

# Trigger waffle_init by listing the root – this is where format detection
# actually runs. Give it up to 60 more seconds (slow serial reads).
i=0
LS_OK=0
while [ $i -lt 60 ]; do
    if ls "$MNT" >/dev/null 2>&1; then
        LS_OK=1
        break
    fi
    # If the driver died during init (format mismatch), the mountpoint
    # becomes inaccessible even though it was briefly registered.
    if ! kill -0 "$WAFFLE_PID" 2>/dev/null; then
        break
    fi
    sleep 1
    i=$((i+1))
done

if [ $LS_OK -eq 0 ]; then
    # Clean up the dead/broken mount
    fusermount3 -u "$MNT" 2>/dev/null || fusermount -u "$MNT" 2>/dev/null || umount -l "$MNT" 2>/dev/null || true
    rmdir "$MNT" 2>/dev/null || true
    case "$FORMAT" in
        dos)   HINT="Is there an Amiga disk in the drive? Try the Amiga launcher instead." ;;
        amiga) HINT="Is there a PC/DOS disk in the drive? Try the DOS launcher instead." ;;
        *)     HINT="The disk format may not match the selected launcher." ;;
    esac
    notify_error "waffle-fuse" "Cannot read the disk as ${LABEL} format. ${HINT}"
    exit 1
fi

notify "waffle-fuse" "${DISPLAY_LABEL} mounted at $MNT" "media-floppy"

# Open file manager
if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "$MNT" &
elif command -v thunar >/dev/null 2>&1; then
    thunar "$MNT" &
elif command -v caja >/dev/null 2>&1; then
	caja "$MNT" &
elif command -v nautilus >/dev/null 2>&1; then
    nautilus "$MNT" &
elif command -v dolphin >/dev/null 2>&1; then
    dolphin "$MNT" &
fi
