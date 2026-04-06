#!/bin/bash
# waffle-serve-image.sh – Serve an ADF/IMG file as an NBD block device and
# mount it automatically, just like a real floppy via waffle-nbd-handler.sh.
#
# Usage: waffle-serve-image.sh <image.adf|img> [--ro]
#   --ro   Mount read-only (image file is never modified)
#
# The script:
#  1. Starts waffle-nbd --serve-image (auto-selects free port + device)
#  2. Connects nbd-client once the server is ready
#  3. Auto-detects filesystem (Amiga affs / PC vfat)
#  4. Mounts and opens the file manager
#  5. Watches for user eject; disconnects and cleans up

set -euo pipefail

IMAGE="${1:-}"
READ_ONLY=0
for arg in "$@"; do
    [[ "$arg" == "--ro" ]] && READ_ONLY=1
done

if [ -z "$IMAGE" ] || [[ "$IMAGE" == --* ]]; then
    echo "Usage: $(basename "$0") <image.adf|img> [--ro]" >&2
    exit 1
fi

IMAGE="$(realpath "$IMAGE")"
if [ ! -f "$IMAGE" ]; then
    echo "ERROR: file not found: $IMAGE" >&2
    exit 1
fi

# ── Find waffle-nbd binary ────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WAFFLE_NBD="$SCRIPT_DIR/../waffle-nbd"
if [ ! -x "$WAFFLE_NBD" ]; then
    WAFFLE_NBD="$(command -v waffle-nbd 2>/dev/null || echo waffle-nbd)"
fi

# ── Ensure modules are loaded ─────────────────────────────────────────────────
modprobe nbd  2>/dev/null || true
modprobe affs 2>/dev/null || true

# ── State ─────────────────────────────────────────────────────────────────────
USER="${SUDO_USER:-$USER}"
UID_U=$(id -u "$USER")
GID_U=$(id -g "$USER")
MNT=""
NBD_DEV=""
NBD_PORT=""
WAFFLE_PID=""
WATCHER_PID=""
CONNECTED=0

_user_sysenv() {
    XDG_RUNTIME_DIR="/run/user/$UID_U" \
        sudo -u "$USER" systemctl --user show-environment 2>/dev/null || true
}
DISPLAY_VAR=$(_user_sysenv | grep -m1 '^DISPLAY=' || true)
WAYLAND_VAR=$(_user_sysenv | grep -m1 '^WAYLAND_DISPLAY=' || true)

cleanup() {
    echo "[serve-image] cleanup..."
    [ -n "$WATCHER_PID" ] && kill "$WATCHER_PID" 2>/dev/null || true
    if [ -n "$MNT" ] && mountpoint -q "$MNT" 2>/dev/null; then
        umount -l "$MNT" 2>/dev/null || true
        rmdir "$MNT" 2>/dev/null || true
    fi
    if [ "$CONNECTED" -eq 1 ] && [ -n "$NBD_DEV" ]; then
        nbd-client -d "$NBD_DEV" 2>/dev/null || true
    fi
    [ -n "$WAFFLE_PID" ] && kill "$WAFFLE_PID" 2>/dev/null || true
    exit 0
}
trap cleanup SIGTERM SIGINT EXIT

# ── Start server and parse its output ────────────────────────────────────────
echo "[serve-image] starting server for: $IMAGE"

# Use a temp FIFO so we can read the server output line by line and also
# capture port/device from the first lines.
FIFO=$(mktemp -u /tmp/waffle-serve-XXXXXX)
mkfifo "$FIFO"

stdbuf -oL -eL "$WAFFLE_NBD" --serve-image "$IMAGE" > "$FIFO" 2>&1 &
WAFFLE_PID=$!

FORMAT=""
DENSITY=""

while IFS= read -r line; do
    echo "[waffle-nbd] $line"

    # Extract port from: "nbd: server bound to 127.0.0.1:10810"
    if [[ "$line" == *"nbd: server bound to"* ]] && [ -z "$NBD_PORT" ]; then
        NBD_PORT=$(echo "$line" | sed 's/.*:\([0-9]*\)$/\1/')
    fi

    # Extract nbd device and port from the connect hint:
    # "nbd: connect with: sudo nbd-client 127.0.0.1 10810 /dev/nbd0"
    if [[ "$line" == *"nbd: connect with:"* ]] && [ -z "$NBD_DEV" ]; then
        NBD_DEV=$(echo "$line" | awk '{print $NF}')
        NBD_PORT=$(echo "$line" | awk '{print $(NF-1)}')
    fi

    # Extract format from: "nbd: format=Amiga size=880 KB"
    if [[ "$line" == *"nbd: format="* ]] && [ -z "$FORMAT" ]; then
        if echo "$line" | grep -qi "amiga"; then
            FORMAT="affs"
        else
            FORMAT="vfat"
        fi
    fi

    # Server ready: disk is being served → connect nbd-client
    if [[ "$line" == *"nbd: disk detected"* ]] || \
       [[ "$line" == *"accepting connections"* ]]; then
        if [ "$CONNECTED" -eq 0 ] && [ -n "$NBD_PORT" ] && [ -n "$NBD_DEV" ]; then
            echo "[serve-image] connecting nbd-client on $NBD_DEV port $NBD_PORT..."
            if nbd-client 127.0.0.1 "$NBD_PORT" "$NBD_DEV" 2>/dev/null; then
                CONNECTED=1
                echo "[serve-image] nbd-client connected: $NBD_DEV"
            else
                echo "[serve-image] ERROR: nbd-client failed" >&2
                break
            fi
        fi
    fi

    # After GO: wait for block device to be ready, then mount
    if [[ "$line" == *"nbd: client requested GO"* ]] && [ "$CONNECTED" -eq 1 ] \
       && [ -z "$MNT" ] && [ -n "$FORMAT" ]; then

        # Wait up to 30 s for the kernel to announce the block device size.
        # /sys/block/nbdX/size contains the number of 512-byte sectors;
        # it stays 0 until nbd-client negotiation is fully complete and
        # udev has processed the block device event.
        DEV_NAME="${NBD_DEV##*/}"   # e.g. "nbd0"
        SIZE_FILE="/sys/block/$DEV_NAME/size"
        echo "[serve-image] waiting for block device $NBD_DEV to be ready..."
        READY=0
        for _w in $(seq 1 30); do
            _sz=$(cat "$SIZE_FILE" 2>/dev/null || echo 0)
            if [ "$_sz" -gt 0 ] 2>/dev/null; then
                READY=1
                echo "[serve-image] block device ready (${_sz} sectors) after ${_w}s"
                break
            fi
            sleep 1
        done
        if [ "$READY" -eq 0 ]; then
            echo "[serve-image] ERROR: block device $NBD_DEV not ready after 30s" >&2
            break
        fi

        IMG_BASE=$(basename "$IMAGE")
        IMG_NAME="${IMG_BASE%.*}"
        LABEL="${IMG_NAME:0:32}"  # keep label short

        _dbus="unix:path=/run/user/$UID_U/bus"
        SCRIPT_MOUNTED=0

        if [ "$FORMAT" = "vfat" ]; then
            echo "[serve-image] mounting $NBD_DEV as vfat via udisksctl..."
            MNT=$(sudo -u "$USER" \
                      DBUS_SESSION_BUS_ADDRESS="$_dbus" \
                      udisksctl mount -b "$NBD_DEV" --no-user-interaction \
                      2>&1 | sed -n 's/.*at \(.*\)\./\1/p' | tr -d '\r' || true)
            if [ -n "$MNT" ] && mountpoint -q "$MNT" 2>/dev/null; then
                echo "[serve-image] udisksctl mounted at $MNT"
                SCRIPT_MOUNTED=0
            else
                MNT="/media/$USER/$LABEL"
                mkdir -p "$MNT"
                chown "$USER:" "$MNT"
                RO_OPT=""; [ "$READ_ONLY" -eq 1 ] && RO_OPT=",ro"
                OPTS="nosuid,users,uid=$UID_U,gid=$GID_U,umask=022$RO_OPT"
                if mount -t vfat -o "$OPTS" "$NBD_DEV" "$MNT"; then
                    SCRIPT_MOUNTED=1
                else
                    echo "[serve-image] ERROR: mount vfat failed" >&2
                    MNT=""
                fi
            fi
        else
            # affs (Amiga) — manual mount
            MNT="/media/$USER/$LABEL"
            mkdir -p "$MNT"
            chown "$USER:" "$MNT"
            RO_OPT=""; [ "$READ_ONLY" -eq 1 ] && RO_OPT=",ro"
            OPTS="nosuid,users,setuid=$UID_U,setgid=$GID_U$RO_OPT"
            echo "[serve-image] mounting $NBD_DEV as affs on $MNT..."
            if mount -t affs -o "$OPTS" "$NBD_DEV" "$MNT"; then
                SCRIPT_MOUNTED=1
            else
                echo "[serve-image] ERROR: mount affs failed" >&2
                MNT=""
            fi
        fi

        if [ -n "$MNT" ]; then
            echo "[serve-image] mounted at $MNT"
            if [ "$SCRIPT_MOUNTED" -eq 1 ]; then
                if ! sudo -u "$USER" \
                        DBUS_SESSION_BUS_ADDRESS="$_dbus" \
                        gdbus call --session \
                            --dest org.freedesktop.FileManager1 \
                            --object-path /org/freedesktop/FileManager1 \
                            --method org.freedesktop.FileManager1.ShowFolders \
                            "['file://$MNT']" "" >/dev/null 2>&1; then
                    sudo -u "$USER" env \
                        DBUS_SESSION_BUS_ADDRESS="$_dbus" \
                        ${DISPLAY_VAR:+$DISPLAY_VAR} \
                        ${WAYLAND_VAR:+$WAYLAND_VAR} \
                        xdg-open "$MNT" &
                fi
            fi

            # Watch for user eject
            _mnt_snap="$MNT"
            _dbus_snap="$_dbus"
            (
                while mountpoint -q "$_mnt_snap" 2>/dev/null; do
                    sleep 2
                done
                nbd-client -d "$NBD_DEV" 2>/dev/null || true
                sudo -u "$USER" \
                    DBUS_SESSION_BUS_ADDRESS="$_dbus_snap" \
                    notify-send -i drive-removable-media \
                    "Waffle Image" "Image unmounted: $(basename "$IMAGE")" 2>/dev/null || true
            ) &
            WATCHER_PID=$!
        fi
    fi

done < "$FIFO"

rm -f "$FIFO"
