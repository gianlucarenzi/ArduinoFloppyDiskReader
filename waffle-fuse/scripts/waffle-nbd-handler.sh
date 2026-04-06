#!/bin/bash
# waffle-nbd-handler.sh – Gestore Bash per il ciclo di vita NBD Waffle
#
# Lanciato da waffle-udev.sh (tramite systemd-run) per gestire il
# montaggio automatico dei dischi via NBD.

PORT="$1"
USER="$2"

if [ -z "$PORT" ] || [ -z "$USER" ]; then
    echo "Utilizzo: $0 <porta_seriale> <utente_sessione>"
    exit 1
fi

# Assicuriamoci che il modulo nbd sia caricato
modprobe nbd >/dev/null 2>&1 || true
modprobe affs >/dev/null 2>&1 || true
sleep 1

# ── Allocatori ───────────────────────────────────────────────────────────────

# Trova un device /dev/nbdN libero
NBD_DEV=""
for i in $(seq 0 15); do
    if [ ! -f "/sys/block/nbd$i/pid" ] || [ "$(cat "/sys/block/nbd$i/pid" 2>/dev/null || echo 0)" -eq 0 ]; then
        NBD_DEV="/dev/nbd$i"
        break
    fi
done

if [ -z "$NBD_DEV" ]; then
    echo "ERRORE: Nessun dispositivo /dev/nbdX libero trovato."
    exit 1
fi

# Trova una porta TCP libera per il server NBD locale
NBD_PORT=10809
while ss -tuln | grep -q ":$NBD_PORT "; do
    NBD_PORT=$((NBD_PORT + 1))
done

# ── Percorsi ─────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# Use relative path only in source/dev layout (binary is a regular file next to scripts/).
# When installed via .deb the binary is in /usr/bin/ — fall back to PATH lookup.
WAFFLE_NBD="$SCRIPT_DIR/../waffle-nbd"
if [ ! -f "$WAFFLE_NBD" ] || [ ! -x "$WAFFLE_NBD" ]; then
    WAFFLE_NBD="$(command -v waffle-nbd 2>/dev/null || echo waffle-nbd)"
fi

# ── Stato ────────────────────────────────────────────────────────────────────
MNT=""
CONNECTED=0
PENDING_FORMAT=""
PENDING_DENSITY=""
WATCHER_PID=""
UID_U=$(id -u "$USER")
GID_U=$(id -g "$USER")

# Retrieve graphical session environment from user's systemd --user instance.
# This is needed because the handler runs as root (system slice) and has no
# DISPLAY/WAYLAND_DISPLAY in its environment.
_user_sysenv() {
    XDG_RUNTIME_DIR="/run/user/$UID_U" \
        sudo -u "$USER" systemctl --user show-environment 2>/dev/null
}
DISPLAY_VAR=$(_user_sysenv | grep -m1 '^DISPLAY=')
WAYLAND_VAR=$(_user_sysenv | grep -m1 '^WAYLAND_DISPLAY=')


cleanup() {
    echo "Chiusura sessione per $PORT..."
    [ -n "$WATCHER_PID" ] && kill "$WATCHER_PID" 2>/dev/null
    if [ -n "$MNT" ] && mountpoint -q "$MNT"; then
        umount -l "$MNT" 2>/dev/null
        rmdir "$MNT" 2>/dev/null
    fi
    if [ "$CONNECTED" -eq 1 ]; then
        nbd-client -d "$NBD_DEV" 2>/dev/null
    fi
    # Uccidi il server nbd se ancora vivo
    [ -n "$WAFFLE_PID" ] && kill "$WAFFLE_PID" 2>/dev/null
    exit 0
}
trap cleanup SIGTERM SIGINT EXIT

# ── Loop principale ──────────────────────────────────────────────────────────
echo "Avvio waffle-nbd su $PORT (TCP: 127.0.0.1:$NBD_PORT -> $NBD_DEV)"

# Usiamo stdbuf per disabilitare il buffering di stdout e stderr di waffle-nbd
stdbuf -oL -eL "$WAFFLE_NBD" "$PORT" 127.0.0.1 "$NBD_PORT" 2>&1 | while read -r line; do
    echo "[waffle-nbd] $line"

    # 1. Il server è pronto (disco inserito) -> Colleghiamo il client
    if [[ "$line" == *"nbd: disk detected"* ]]; then
        echo "--> Disco rilevato, avvio nbd-client..."
        if nbd-client 127.0.0.1 "$NBD_PORT" "$NBD_DEV"; then
            CONNECTED=1
            echo "--> nbd-client connesso con successo."
        else
            echo "--> ERRORE: nbd-client non è riuscito a connettersi."
        fi
    fi

    # 2a. Salva il formato quando arriva l'evento (può precedere GO)
    if [[ "$line" == *"waffle-event:"* ]]; then
        PENDING_FORMAT=$(echo "$line" | sed -n 's/.*format=\([^ ]*\).*/\1/p' | tr -d '\r')
        PENDING_DENSITY=$(echo "$line" | sed -n 's/.*density=\([^ ]*\).*/\1/p' | tr -d '\r')
        echo "--> Rilevato formato: $PENDING_FORMAT ($PENDING_DENSITY)"
    fi

    # 2b. GO ricevuto + formato disponibile -> Procedi al montaggio
    if [[ "$line" == *"nbd: client requested GO"* ]]; then
        echo "--> Comando GO ricevuto dal client."
        if [ -n "$PENDING_FORMAT" ]; then
            FORMAT="$PENDING_FORMAT"
            DENSITY="$PENDING_DENSITY"
            PENDING_FORMAT=""
            PENDING_DENSITY=""

            # Initial spin-up wait: HD disks need longer to stabilise.
            if [ "$DENSITY" = "hd" ]; then
                echo "--> Disco HD: attesa 10s per stabilizzazione motore..."
                sleep 10
            else
                echo "--> Disco DD: attesa 5s per stabilizzazione motore..."
                sleep 5
            fi

            # Wait up to 30 s for the kernel to announce the block device.
            DEV_NAME="${NBD_DEV##*/}"
            SIZE_FILE="/sys/block/$DEV_NAME/size"
            echo "--> In attesa che il block device $NBD_DEV sia pronto (max 30s)..."
            READY=0
            for _w in $(seq 1 30); do
                _sz=$(cat "$SIZE_FILE" 2>/dev/null || echo 0)
                if [ "$_sz" -gt 0 ] 2>/dev/null; then
                    READY=1
                    echo "--> $NBD_DEV pronto (${_sz} settori) dopo ${_w}s."
                    break
                fi
                sleep 1
            done
            if [ "$READY" -eq 0 ]; then
                echo "--> ERRORE: $NBD_DEV non pronto dopo 30s, montaggio saltato."
                PENDING_FORMAT=""
                continue
            fi

            case "$FORMAT" in
                affs) LABEL="Amiga_Floppy" ;;
                vfat) LABEL="DOS_Floppy"   ;;
                *)    LABEL="Unknown_Floppy" ;;
            esac

            if [ "$FORMAT" != "unknown" ]; then
                _dbus="unix:path=/run/user/$UID_U/bus"
                SCRIPT_MOUNTED=0

                if [ "$FORMAT" = "vfat" ]; then
                    # Let udisks2 manage the mount so only one icon (with eject
                    # button) appears in the file manager.  udisks2 places the
                    # mount point under /run/media/$USER/<label>.
                    echo "--> Montaggio $NBD_DEV via udisksctl..."
                    MNT=$(sudo -u "$USER" \
                              DBUS_SESSION_BUS_ADDRESS="$_dbus" \
                              udisksctl mount -b "$NBD_DEV" --no-user-interaction \
                              2>&1 | sed -n 's/.*at \(.*\)\./\1/p' | tr -d '\r')
                    if [ -n "$MNT" ] && mountpoint -q "$MNT" 2>/dev/null; then
                        echo "--> udisksctl montato su $MNT."
                        SCRIPT_MOUNTED=0  # udisks2 owns it; file manager opens automatically
                    else
                        # udisksctl failed (e.g. already mounted) — fall back to
                        # manual mount so the disk is still accessible.
                        MNT=$(findmnt -n -o TARGET "$NBD_DEV" 2>/dev/null)
                        if [ -z "$MNT" ]; then
                            MNT="/media/$USER/$LABEL"
                            mkdir -p "$MNT"
                            chown "$USER:" "$MNT"
                            OPTS="nosuid,users,uid=$UID_U,gid=$GID_U,umask=022"
                            echo "--> Fallback: mount -t vfat -o $OPTS $NBD_DEV $MNT"
                            if mount -t vfat -o "$OPTS" "$NBD_DEV" "$MNT"; then
                                SCRIPT_MOUNTED=1
                            else
                                echo "--> ERRORE: Mount fallito."
                                MNT=""
                            fi
                        else
                            echo "--> $NBD_DEV già montato su $MNT."
                        fi
                    fi
                else
                    # affs and other types: manual mount (udisks2 doesn't support them).
                    MNT=$(findmnt -n -o TARGET "$NBD_DEV" 2>/dev/null)
                    if [ -z "$MNT" ]; then
                        MNT="/media/$USER/$LABEL"
                        OPTS="nosuid,users,setuid=$UID_U,setgid=$GID_U"
                        mkdir -p "$MNT"
                        chown "$USER:" "$MNT"
                        echo "--> Montaggio $NBD_DEV ($FORMAT) su $MNT..."
                        if mount -t "$FORMAT" -o "$OPTS" "$NBD_DEV" "$MNT"; then
                            SCRIPT_MOUNTED=1
                        else
                            echo "--> ERRORE: Mount fallito."
                            MNT=""
                        fi
                    else
                        echo "--> $NBD_DEV già montato su $MNT."
                    fi
                fi

                if [ -n "$MNT" ]; then
                    echo "--> Montaggio completato su $MNT."
                    # Open the file manager only for manually-mounted filesystems.
                    # udisksctl mounts trigger an automatic file manager open via
                    # the udisks2/GVfs D-Bus signal.
                    if [ "$SCRIPT_MOUNTED" -eq 1 ]; then
                        if ! sudo -u "$USER" \
                                DBUS_SESSION_BUS_ADDRESS="$_dbus" \
                                gdbus call --session \
                                    --dest org.freedesktop.FileManager1 \
                                    --object-path /org/freedesktop/FileManager1 \
                                    --method org.freedesktop.FileManager1.ShowFolders \
                                    "['file://$MNT']" "" >/dev/null 2>&1; then
                            echo "--> FileManager1 non disponibile, fallback a xdg-open..."
                            sudo -u "$USER" env \
                                DBUS_SESSION_BUS_ADDRESS="$_dbus" \
                                ${DISPLAY_VAR:+$DISPLAY_VAR} \
                                ${WAYLAND_VAR:+$WAYLAND_VAR} \
                                xdg-open "$MNT" &
                        fi
                    fi

                    # Watch for user-initiated eject (mount point disappears).
                    # When detected: disconnect nbd-client and prompt to remove the disk.
                    _mnt_snap="$MNT"
                    _dbus_snap="$_dbus"
                    (
                        while mountpoint -q "$_mnt_snap" 2>/dev/null; do
                            sleep 2
                        done
                        nbd-client -d "$NBD_DEV" 2>/dev/null
                        sudo -u "$USER" \
                            DBUS_SESSION_BUS_ADDRESS="$_dbus_snap" \
                            notify-send -i drive-removable-media-usb \
                            "Waffle" "You can now remove the disk."
                    ) &
                    WATCHER_PID=$!
                fi
            else
                echo "--> Filesystem sconosciuto, montaggio automatico saltato."
            fi
        fi
    fi

    # 3. Rimozione disco
    if [[ "$line" == *"nbd: disk absent"* ]] || [[ "$line" == *"disk removed"* ]]; then
        echo "--> Disco rimosso, smontaggio in corso..."
        [ -n "$WATCHER_PID" ] && kill "$WATCHER_PID" 2>/dev/null
        WATCHER_PID=""
        if [ -n "$MNT" ]; then
            umount -l "$MNT" 2>/dev/null
            rmdir "$MNT" 2>/dev/null
            MNT=""
        fi
        nbd-client -d "$NBD_DEV" 2>/dev/null
        CONNECTED=0
        PENDING_FORMAT=""
        PENDING_DENSITY=""
        echo "--> Pulizia completata, in attesa di un nuovo disco..."
    fi

done
