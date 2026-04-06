#!/bin/bash
# test-waffle-flow.sh
#
# Test end-to-end del flusso waffle-nbd:
#   1. Avvia il server manualmente su /dev/ttyUSB0
#   2. Connette nbd-client → /dev/nbd0
#   3. Inserimento disco → rilevamento + mount
#   4. Smonta il disco
#   5. Scrive un'immagine di test sul disco fisico (dump)
#   6. Espelle il disco fisicamente
#   7. Reinserisce il disco → rimonta
#   8. Smonta il disco
#   9. Clona il disco in un file (clone)
#  10. Confronta l'immagine originale con quella clonata → devono coincidere
#
# Prerequisiti:
#   - waffle-nbd installato (o usa il binario locale ./waffle-nbd)
#   - nbd-client, modprobe nbd
#   - essere nel gruppo 'dialout' per accedere alla porta seriale
#   - sudo configurato per: modprobe, nbd-client, mount, umount
#     (oppure eseguire direttamente come root)
#
# NOTA: se il servizio udev è attivo, fermalo prima:
#   sudo systemctl stop waffle-nbd@ttyUSB0
#
# Uso: ./test-waffle-flow.sh [immagine-di-test.adf]

set -euo pipefail

# ── Configurazione ────────────────────────────────────────────────────────────
SERIAL_PORT="${SERIAL_PORT:-/dev/ttyUSB0}"
NBD_ADDR="127.0.0.1"
NBD_PORT="10809"
NBD_DEV="/dev/nbd0"
CTL_SOCK="/run/waffle-nbd/ttyUSB0.ctl"
MNT="/tmp/waffle-mnt-$$"
WAFFLE_NBD="${WAFFLE_NBD:-$(dirname "$0")/../waffle-nbd}"
WORKDIR="/tmp/waffle-test-$$"

# Immagine sorgente (primo argomento o scelta interattiva)
SRC_IMAGE="${1:-}"

# ── sudo wrapper ──────────────────────────────────────────────────────────────
# Se già root, SUDO è vuoto; altrimenti usiamo sudo.
if [[ $EUID -eq 0 ]]; then
    SUDO=""
else
    SUDO="sudo"
    # Pre-acquisisce le credenziali sudo una volta sola
    echo "Alcune operazioni richiedono privilegi (modprobe, nbd-client, mount)."
    $SUDO true || { echo "sudo non disponibile o negato"; exit 1; }
fi

# ── Colori ────────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
step()  { echo -e "\n${YELLOW}══ $* ${NC}"; }
ok()    { echo -e "${GREEN}✔ $*${NC}"; }
fail()  { echo -e "${RED}✘ $*${NC}"; exit 1; }
pause() { echo -e "\n${YELLOW}[PREMI INVIO quando pronto: $*]${NC}"; read -r; }

# ── Setup cartella di lavoro ──────────────────────────────────────────────────
mkdir -p "$WORKDIR"
CLONE_IMAGE="$WORKDIR/clone.img"
trap 'cleanup' EXIT

cleanup() {
    echo "==> Pulizia..."
    mountpoint -q "$MNT" 2>/dev/null && $SUDO umount -l "$MNT" 2>/dev/null || true
    rmdir "$MNT" 2>/dev/null || true
    if $SUDO nbd-client -c "$NBD_DEV" &>/dev/null; then
        $SUDO nbd-client -d "$NBD_DEV" 2>/dev/null || true
    fi
    if [[ -n "${SERVER_PID:-}" ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$WORKDIR"
}

# ── Prepara immagine sorgente ─────────────────────────────────────────────────
step "Preparazione immagine sorgente"
if [[ -z "$SRC_IMAGE" ]]; then
    echo "Nessuna immagine specificata come argomento."
    echo "Opzioni:"
    echo "  1) Inserisci il percorso di un'immagine esistente (.adf / .img)"
    echo "  2) Genera un blank PC HD (1.44 MB)"
    echo "  3) Genera un blank Amiga OFS DD (880 KB)"
    echo ""
    read -rp "Scelta [1/2/3] oppure percorso diretto: " USER_INPUT

    case "$USER_INPUT" in
        2)
            SRC_IMAGE="$WORKDIR/source.img"
            echo "Generazione blank PC HD..."
            "$WAFFLE_NBD" --format pc --hd "$SRC_IMAGE"
            ;;
        3)
            SRC_IMAGE="$WORKDIR/source.adf"
            echo "Generazione blank Amiga OFS DD..."
            "$WAFFLE_NBD" --format amiga "$SRC_IMAGE"
            ;;
        1)
            read -rp "Percorso immagine: " SRC_IMAGE
            [[ -f "$SRC_IMAGE" ]] || fail "File non trovato: $SRC_IMAGE"
            ;;
        *)
            SRC_IMAGE="$USER_INPUT"
            [[ -f "$SRC_IMAGE" ]] || fail "File non trovato: $SRC_IMAGE"
            ;;
    esac
else
    [[ -f "$SRC_IMAGE" ]] || fail "File non trovato: $SRC_IMAGE"
fi

ok "Immagine sorgente: $SRC_IMAGE ($(du -h "$SRC_IMAGE" | cut -f1))"
SRC_SHA=$(sha256sum "$SRC_IMAGE" | cut -d' ' -f1)
echo "    SHA256 sorgente: $SRC_SHA"

# ── Carica modulo NBD ─────────────────────────────────────────────────────────
step "Caricamento modulo kernel nbd"
$SUDO modprobe nbd max_part=1 || fail "modprobe nbd fallito"
ok "Modulo nbd caricato"

# ── Avvia server ──────────────────────────────────────────────────────────────
step "Avvio waffle-nbd su $SERIAL_PORT"
$SUDO mkdir -p "$(dirname "$CTL_SOCK")"
$SUDO chmod 777 "$(dirname "$CTL_SOCK")"
"$WAFFLE_NBD" "$SERIAL_PORT" "$NBD_ADDR" "$NBD_PORT" &
SERVER_PID=$!
echo "    PID server: $SERVER_PID"
sleep 2
kill -0 "$SERVER_PID" 2>/dev/null || fail "Server terminato inaspettatamente"
ok "Server in ascolto su $NBD_ADDR:$NBD_PORT"

# Attendi che il control socket sia disponibile (creato dopo la prima connessione NBD)
wait_ctl_socket() {
    local timeout=15
    for ((i=0; i<timeout; i++)); do
        [[ -S "$CTL_SOCK" ]] && return 0
        sleep 1
    done
    return 1
}

# ── Connetti nbd-client ───────────────────────────────────────────────────────
pause "Inserisci un floppy FORMATTATO nel drive, poi premi INVIO"

step "Connessione nbd-client → $NBD_DEV"
$SUDO nbd-client "$NBD_ADDR" "$NBD_PORT" "$NBD_DEV" || fail "nbd-client fallito"
sleep 1
ok "nbd-client connesso: $NBD_DEV"

step "Attesa control socket"
wait_ctl_socket || fail "Control socket $CTL_SOCK non disponibile dopo 15s"
ok "Control socket pronto: $CTL_SOCK"

# ── Funzione: rileva filesystem da /dev/nbd0 ─────────────────────────────────
detect_fs() {
    local MAGIC
    MAGIC=$(dd if="$NBD_DEV" bs=3 count=1 2>/dev/null | od -An -tx1 | tr -d ' \n')
    if [[ "$MAGIC" == "444f53"* ]]; then
        echo "affs"
    else
        echo "vfat"
    fi
}

# ── Rilevamento formato e mount ───────────────────────────────────────────────
step "Rilevamento formato disco"
BLKID=$($SUDO blkid "$NBD_DEV" 2>/dev/null || true)
echo "    blkid: $BLKID"

FS_TYPE=$(detect_fs)
echo "    Tipo filesystem rilevato: $FS_TYPE"

mkdir -p "$MNT"
if [[ "$FS_TYPE" == "affs" ]]; then
    $SUDO mount -t affs "$NBD_DEV" "$MNT" || fail "mount affs fallito"
else
    $SUDO mount -t vfat -o uid="$(id -u)",gid="$(id -g)",utf8 "$NBD_DEV" "$MNT" || fail "mount vfat fallito"
fi
ok "Disco montato su $MNT"
echo "    Contenuto:"
ls -la "$MNT" | head -10

# ── Smonta ───────────────────────────────────────────────────────────────────
step "Smontaggio disco"
$SUDO umount "$MNT"
ok "Disco smontato"

# ── Dump: scrivi l'immagine sorgente sul disco fisico ─────────────────────────
step "DUMP: scrittura immagine → disco fisico"
echo "    Invio comando 'dump $SRC_IMAGE' al server..."
"$WAFFLE_NBD" --ctl "$CTL_SOCK" dump "$SRC_IMAGE" || fail "dump fallito"
ok "Immagine scritta sul disco"

# ── Espelli e reinserisci ─────────────────────────────────────────────────────
step "Disconnessione nbd-client"
$SUDO nbd-client -d "$NBD_DEV"
sleep 1
ok "nbd-client disconnesso"

pause "TOGLI il floppy dal drive, poi reinseriscilo, poi premi INVIO"

step "Riconnessione nbd-client"
$SUDO nbd-client "$NBD_ADDR" "$NBD_PORT" "$NBD_DEV" || fail "nbd-client fallito"
sleep 2
wait_ctl_socket || fail "Control socket $CTL_SOCK non disponibile dopo 15s"
ok "nbd-client riconnesso"

# ── Rimonta ───────────────────────────────────────────────────────────────────
step "Rimontaggio disco (verifica che la scrittura sia andata a buon fine)"
REMOUNT_FS=$(detect_fs)
echo "    Formato rilevato: $REMOUNT_FS"
if [[ "$REMOUNT_FS" == "affs" ]]; then
    $SUDO mount -t affs "$NBD_DEV" "$MNT" || fail "rimount affs fallito"
else
    $SUDO mount -t vfat -o uid="$(id -u)",gid="$(id -g)",utf8 "$NBD_DEV" "$MNT" || fail "rimount vfat fallito"
fi
ok "Disco rimontato su $MNT"
echo "    Contenuto dopo dump:"
ls -la "$MNT" | head -10

# ── Smonta di nuovo ───────────────────────────────────────────────────────────
step "Secondo smontaggio"
$SUDO umount "$MNT"
ok "Disco smontato"

# ── Clone: leggi il disco fisico → file ───────────────────────────────────────
step "CLONE: lettura disco fisico → $CLONE_IMAGE"
echo "    Invio comando 'clone $CLONE_IMAGE' al server..."
"$WAFFLE_NBD" --ctl "$CTL_SOCK" clone "$CLONE_IMAGE" || fail "clone fallito"
ok "Disco clonato in $CLONE_IMAGE"

# ── Verifica integrità ────────────────────────────────────────────────────────
step "Verifica: confronto SHA256 sorgente vs clone"

CLONE_SHA=$(sha256sum "$CLONE_IMAGE" | cut -d' ' -f1)

echo "    Sorgente: $SRC_SHA  $(basename "$SRC_IMAGE")"
echo "    Clone:    $CLONE_SHA  $(basename "$CLONE_IMAGE")"
echo ""

if [[ "$SRC_SHA" == "$CLONE_SHA" ]]; then
    ok "✔ Le immagini sono IDENTICHE — test superato!"
else
    echo -e "${RED}✘ Le immagini DIFFERISCONO${NC}"
    echo ""
    echo "    Analisi delle differenze (primi 16 settori diversi):"
    python3 - "$SRC_IMAGE" "$CLONE_IMAGE" <<'PYEOF'
import sys

def find_diffs(f1, f2, sector_size=512, max_diffs=16):
    with open(f1, 'rb') as a, open(f2, 'rb') as b:
        lba = 0
        diffs = 0
        while True:
            sa = a.read(sector_size)
            sb = b.read(sector_size)
            if not sa and not sb:
                break
            if sa != sb:
                print(f"  LBA {lba:4d} (offset 0x{lba*sector_size:06x}): differisce")
                diffs += 1
                if diffs >= max_diffs:
                    print(f"  ... (limitato a {max_diffs} differenze)")
                    break
            lba += 1
        if diffs == 0:
            print("  (nessuna differenza trovata — forse file di dimensioni diverse?)")

find_diffs(sys.argv[1], sys.argv[2])
PYEOF
    echo ""
    echo "Per confronto visivo di un settore:"
    echo "  dd if='$SRC_IMAGE'   bs=512 skip=N count=1 | xxd | head -8"
    echo "  dd if='$CLONE_IMAGE' bs=512 skip=N count=1 | xxd | head -8"
    fail "Test fallito: immagini diverse"
fi

