#!/bin/bash
# test-waffle-flow.sh
#
# Test end-to-end del flusso waffle-nbd:
#   1. Avvia waffle-nbd su /dev/ttyUSB0
#   2. Inserimento disco → connessione nbd-client + mount
#   3. Smonta il disco
#   4. Scrive un'immagine di test sul disco fisico (dump)
#   5. Espelle il disco fisicamente e lo reinserisce
#   6. Rimonta e verifica il contenuto
#   7. Smonta e clona il disco
#   8. Confronta l'immagine originale con quella clonata
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
NBD_DEV="/dev/nbd0"
CTL_SOCK="/run/waffle-nbd/$(basename "$SERIAL_PORT").ctl"
WORKDIR="/tmp/waffle-test-$$"
WAFFLE_NBD="${WAFFLE_NBD:-$(dirname "$0")/../waffle-nbd}"
SERVER_PID=""
MOUNT_FLAGS="rw,users,nosuid,uid=$(id -u),gid=$(id -g)"

# Immagine sorgente (primo argomento o scelta interattiva)
SRC_IMAGE="${1:-}"

# ── sudo wrapper ───────────────────────────────────────────────────────────────
if [[ $EUID -eq 0 ]]; then
    SUDO=""
else
    SUDO="sudo"
    echo "Alcune operazioni richiedono privilegi (modprobe, nbd-client, mount)."
    $SUDO true || { echo "sudo non disponibile o negato"; exit 1; }
fi

# ── Colori / helper ────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'; BOLD='\033[1m'
step()  { echo -e "\n${BOLD}${YELLOW}══ $* ${NC}"; }
ok()    { echo -e "${GREEN}✔ $*${NC}"; }
fail()  { echo -e "${RED}✘ $*${NC}"; exit 1; }
pause() { echo -e "\n${YELLOW}[PREMI INVIO quando pronto: $*]${NC}"; read -r; }
info()  { echo "    $*"; }

mkdir -p "$WORKDIR"
CLONE_IMAGE="$WORKDIR/clone.img"

# ── Pulizia ────────────────────────────────────────────────────────────────────
cleanup() {
    echo "==> Pulizia..."
    while IFS= read -r mp; do
        $SUDO umount -l "$mp" 2>/dev/null || true
    done < <(grep "$WORKDIR" /proc/mounts 2>/dev/null | awk '{print $2}' | sort -r)
    if $SUDO nbd-client -c "$NBD_DEV" &>/dev/null; then
        $SUDO nbd-client -d "$NBD_DEV" 2>/dev/null || true
    fi
    [[ -n "$SERVER_PID" ]] && kill "$SERVER_PID" 2>/dev/null || true
    [[ -n "$SERVER_PID" ]] && wait "$SERVER_PID" 2>/dev/null || true
    rm -rf "$WORKDIR"
}
trap cleanup EXIT

# ── Helper: crea mountpoint temporaneo ────────────────────────────────────────
make_mnt() { local mnt; mnt=$(mktemp -d "$WORKDIR/mnt-XXXXXX"); echo "$mnt"; }

# ── Helper: smonta e rimuove mountpoint ───────────────────────────────────────
do_umount() {
    local mnt="$1"
    $SUDO umount "$mnt" || $SUDO umount -l "$mnt" 2>/dev/null || true
    rmdir "$mnt" 2>/dev/null || true
}

# ── Helper: attesa block device pronto ────────────────────────────────────────
wait_blk() {
    local dev="${NBD_DEV##*/}"
    for ((i=0; i<30; i++)); do
        local sz; sz=$(cat "/sys/block/$dev/size" 2>/dev/null || echo 0)
        [[ "$sz" -gt 0 ]] && { info "block device pronto (${sz} settori) dopo ${i}s"; return 0; }
        sleep 1
    done
    return 1
}

# ── Helper: attesa control socket ─────────────────────────────────────────────
wait_ctl() {
    for ((i=0; i<20; i++)); do [[ -S "$CTL_SOCK" ]] && return 0; sleep 1; done
    return 1
}

# ── Helper: rileva filesystem da /dev/nbd0 ─────────────────────────────────────
detect_fs() {
    local magic
    magic=$(dd if="$NBD_DEV" bs=3 count=1 2>/dev/null | od -An -tx1 | tr -d ' \n')
    [[ "$magic" == "444f53"* ]] && echo "affs" || echo "vfat"
}

# ── Helper: monta il disco con flag corretti ────────────────────────────────────
mount_disk() {
    local dev="$1" mnt="$2" fs; fs=$(detect_fs)
    info "Formato rilevato: $fs"
    if [[ "$fs" == "affs" ]]; then
        $SUDO mount -t affs -o "$MOUNT_FLAGS" "$dev" "$mnt"
    else
        $SUDO mount -t vfat -o "${MOUNT_FLAGS},utf8" "$dev" "$mnt"
    fi
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
            info "Generazione blank PC HD..."
            "$WAFFLE_NBD" --format pc --hd "$SRC_IMAGE"
            ;;
        3)
            SRC_IMAGE="$WORKDIR/source.adf"
            info "Generazione blank Amiga OFS DD..."
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
info "SHA256 sorgente: $SRC_SHA"

# ── Moduli + avvio server ──────────────────────────────────────────────────────
step "Caricamento moduli kernel (nbd, affs)"
$SUDO modprobe nbd max_part=1 && ok "nbd caricato" || true
$SUDO modprobe affs            && ok "affs caricato" || true

step "Avvio waffle-nbd su $SERIAL_PORT"
$SUDO mkdir -p "$(dirname "$CTL_SOCK")"
$SUDO chmod 777 "$(dirname "$CTL_SOCK")"

NBD_LOG=$(mktemp "$WORKDIR/server-XXXXXX.log")
"$WAFFLE_NBD" "$SERIAL_PORT" "$NBD_ADDR" > "$NBD_LOG" 2>&1 &
SERVER_PID=$!
info "PID server: $SERVER_PID"
sleep 2
kill -0 "$SERVER_PID" 2>/dev/null || { cat "$NBD_LOG"; fail "Server terminato inaspettatamente"; }

NBD_PORT=$(grep -oP '(?<=server bound to 127\.0\.0\.1:)\d+' "$NBD_LOG" | head -1 || echo "10809")
info "Porta NBD: $NBD_PORT"
ok "Server in ascolto su $NBD_ADDR:$NBD_PORT"

# ── Prima connessione ──────────────────────────────────────────────────────────
pause "Inserisci un floppy FORMATTATO nel drive, poi premi INVIO"

step "Connessione nbd-client → $NBD_DEV"
$SUDO nbd-client "$NBD_ADDR" "$NBD_PORT" "$NBD_DEV" || fail "nbd-client fallito"
wait_blk || fail "Block device non pronto dopo 30s"
ok "nbd-client connesso: $NBD_DEV"

step "Attesa control socket"
wait_ctl || fail "Control socket $CTL_SOCK non disponibile dopo 20s"
ok "Control socket pronto: $CTL_SOCK"

# ── Mount iniziale ─────────────────────────────────────────────────────────────
step "Mount disco"
MNT1=$(make_mnt)
mount_disk "$NBD_DEV" "$MNT1" || fail "mount fallito"
ok "Disco montato su $MNT1"
info "Contenuto:"; ls -la "$MNT1" | head -10 | sed 's/^/    /'

# ── Smonta ────────────────────────────────────────────────────────────────────
step "Smontaggio disco"
do_umount "$MNT1"
ok "Disco smontato"

# ── Dump ──────────────────────────────────────────────────────────────────────
step "DUMP: scrittura immagine → disco fisico"
info "Invio comando: dump $SRC_IMAGE"
"$WAFFLE_NBD" --ctl "$CTL_SOCK" dump "$SRC_IMAGE" || fail "dump fallito"
ok "Immagine scritta sul disco"

# ── Espelli e reinserisci ──────────────────────────────────────────────────────
step "Disconnessione nbd-client"
$SUDO nbd-client -d "$NBD_DEV" 2>/dev/null || true
ok "nbd-client disconnesso"

pause "TOGLI il floppy dal drive, poi reinseriscilo, poi premi INVIO"

step "Riconnessione nbd-client"
$SUDO nbd-client "$NBD_ADDR" "$NBD_PORT" "$NBD_DEV" || fail "nbd-client fallito"
wait_blk || fail "Block device non pronto dopo 30s"
wait_ctl || fail "Control socket non disponibile dopo 20s"
ok "nbd-client riconnesso: $NBD_DEV"

# ── Rimonta e verifica ─────────────────────────────────────────────────────────
step "Rimontaggio disco (verifica contenuto dopo dump)"
MNT2=$(make_mnt)
mount_disk "$NBD_DEV" "$MNT2" || fail "rimount fallito"
ok "Disco rimontato su $MNT2"
info "Contenuto dopo dump:"; ls -la "$MNT2" | head -10 | sed 's/^/    /'

step "Secondo smontaggio"
do_umount "$MNT2"
ok "Disco smontato"

# ── Clone ─────────────────────────────────────────────────────────────────────
step "CLONE: lettura disco fisico → $CLONE_IMAGE"
info "Invio comando: clone $CLONE_IMAGE"
"$WAFFLE_NBD" --ctl "$CTL_SOCK" clone "$CLONE_IMAGE" || fail "clone fallito"
ok "Disco clonato in $CLONE_IMAGE"

# ── Verifica integrità ────────────────────────────────────────────────────────
step "Verifica: confronto SHA256 sorgente vs clone"
CLONE_SHA=$(sha256sum "$CLONE_IMAGE" | cut -d' ' -f1)
info "Sorgente: $SRC_SHA  $(basename "$SRC_IMAGE")"
info "Clone:    $CLONE_SHA  $(basename "$CLONE_IMAGE")"

if [[ "$SRC_SHA" == "$CLONE_SHA" ]]; then
    ok "Le immagini sono IDENTICHE — test superato!"
else
    echo -e "${RED}✘ Le immagini DIFFERISCONO${NC}"
    info "Analisi delle differenze (primi 16 settori diversi):"
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
    fail "Test fallito: immagini diverse"
fi

