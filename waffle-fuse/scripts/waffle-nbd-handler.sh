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
WAFFLE_NBD="$SCRIPT_DIR/../waffle-nbd"
[ -x "$WAFFLE_NBD" ] || WAFFLE_NBD="waffle-nbd"

# ── Stato ────────────────────────────────────────────────────────────────────
MNT=""
CONNECTED=0
PENDING_FORMAT=""
PENDING_DENSITY=""
UID_U=$(id -u "$USER")
GID_U=$(id -g "$USER")


cleanup() {
    echo "Chiusura sessione per $PORT..."
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

            echo "--> Avvio del montaggio tra 5 secondi..."
            for i in $(seq 5 -1 1); do
                echo -n "$i... "
                sleep 1
            done
            echo

            case "$FORMAT" in
                affs) LABEL="Amiga_Floppy" ;;
                vfat) LABEL="DOS_Floppy"   ;;
                *)    LABEL="Unknown_Floppy" ;;
            esac

            MNT="/media/$USER/$LABEL"
            mkdir -p "$MNT"
            chown "$USER:" "$MNT"

            OPTS="nosuid"
            case "$FORMAT" in
                affs) OPTS="$OPTS,users,setuid=$UID_U,setgid=$GID_U" ;;
                vfat) OPTS="$OPTS,users,uid=$UID_U,gid=$GID_U,umask=022" ;;
            esac

            if [ "$FORMAT" != "unknown" ]; then
                echo "--> Montaggio $NBD_DEV ($FORMAT) su $MNT..."
                echo "--> Comando: mount -t $FORMAT -o $OPTS $NBD_DEV $MNT"
                if mount -t "$FORMAT" -o "$OPTS" "$NBD_DEV" "$MNT"; then
                    echo "--> Montaggio completato."
                    if command -v sudo >/dev/null; then
                        sudo -u "$USER" DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$UID_U/bus" \
                            xdg-open "$MNT" >/dev/null 2>&1 &
                    fi
                else
                    echo "--> ERRORE: Mount fallito."
                fi
            else
                echo "--> Filesystem sconosciuto, montaggio automatico saltato."
            fi
        fi
    fi

    # 3. Rimozione disco
    if [[ "$line" == *"nbd: disk absent"* ]] || [[ "$line" == *"disk removed"* ]]; then
        echo "--> Disco rimosso, smontaggio in corso..."
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
