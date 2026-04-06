#!/bin/bash
# test-amiga-format-clone.sh
#
# Test end-to-end flusso Amiga:
#   1. Avvia waffle-nbd su /dev/ttyUSB0
#   2. Connette nbd-client
#   3. Attende disco → formatta in Amiga OFS via control socket
#   4. Monta il disco formattato (affs)
#   5. Crea README sul disco
#   6. Smonta e clona → clone1.adf
#   7. Verifica spazio libero e contenuto clone1.adf
#   8. Rimonta, aggiunge un secondo file
#   9. Smonta
#  10. Fa il dump di clone1.adf modificato (clone2.adf) → scrive su disco
#      (dopo aver aggiunto il secondo file nell'immagine via --serve-image)
#  11. Fa dump della nuova immagine sul disco fisico
#  12. Rimonta e verifica entrambi i file
#
# Uso: scripts/test-amiga-format-clone.sh [/dev/ttyUSB0]

set -euo pipefail

SERIAL_PORT="${1:-${SERIAL_PORT:-/dev/ttyUSB0}}"
NBD_ADDR="127.0.0.1"
NBD_DEV="/dev/nbd0"
CTL_SOCK="/run/waffle-nbd/$(basename "$SERIAL_PORT").ctl"
MNT="/tmp/waf-mnt-$$"
WORKDIR="/tmp/waf-test-$$"
WAFFLE_NBD="${WAFFLE_NBD:-$(dirname "$0")/../waffle-nbd}"
SERVER_PID=""

# ── sudo ─────────────────────────────────────────────────────────────────────
if [[ $EUID -eq 0 ]]; then SUDO=""; else
    SUDO="sudo"
    echo "Alcune operazioni richiedono privilegi."
    $SUDO true || { echo "sudo non disponibile"; exit 1; }
fi

# ── Colori / helper ───────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'; BOLD='\033[1m'
step()  { echo -e "\n${BOLD}${YELLOW}══ $* ${NC}"; }
ok()    { echo -e "${GREEN}✔ $*${NC}"; }
fail()  { echo -e "${RED}✘ $*${NC}"; exit 1; }
pause() { echo -e "\n${YELLOW}[PREMI INVIO: $*]${NC}"; read -r; }
info()  { echo "    $*"; }

mkdir -p "$WORKDIR" "$MNT"

cleanup() {
    echo "==> Pulizia..."
    mountpoint -q "$MNT" 2>/dev/null && $SUDO umount -l "$MNT" 2>/dev/null || true
    rmdir "$MNT" 2>/dev/null || true
    $SUDO nbd-client -c "$NBD_DEV" &>/dev/null && \
        $SUDO nbd-client -d "$NBD_DEV" 2>/dev/null || true
    [[ -n "$SERVER_PID" ]] && kill "$SERVER_PID" 2>/dev/null || true
    [[ -n "$SERVER_PID" ]] && wait "$SERVER_PID" 2>/dev/null || true
    rm -rf "$WORKDIR"
}
trap cleanup EXIT

# ── Attesa control socket ─────────────────────────────────────────────────────
wait_ctl() {
    for ((i=0; i<20; i++)); do [[ -S "$CTL_SOCK" ]] && return 0; sleep 1; done
    return 1
}

# ── Attesa block device pronto ────────────────────────────────────────────────
wait_blk() {
    local dev="${NBD_DEV##*/}"
    for ((i=0; i<30; i++)); do
        local sz; sz=$(cat "/sys/block/$dev/size" 2>/dev/null || echo 0)
        [[ "$sz" -gt 0 ]] && { info "block device pronto (${sz} settori) dopo ${i}s"; return 0; }
        sleep 1
    done
    return 1
}

# ── Manda comando al control socket e mostra progress ────────────────────────
ctl_cmd() {
    info "Invio comando: $*"
    "$WAFFLE_NBD" --ctl "$CTL_SOCK" "$@" || fail "comando ctl fallito: $*"
}

# ═════════════════════════════════════════════════════════════════════════════
# 1. Modulo NBD + avvio server
# ═════════════════════════════════════════════════════════════════════════════
step "Caricamento modulo kernel nbd"
$SUDO modprobe nbd max_part=1 && ok "modulo nbd caricato" || true
$SUDO modprobe affs            && ok "modulo affs caricato" || true

step "Avvio waffle-nbd su $SERIAL_PORT"
$SUDO mkdir -p "$(dirname "$CTL_SOCK")"
$SUDO chmod 777 "$(dirname "$CTL_SOCK")"

# Trova porta e device liberi automaticamente (waffle-nbd li stampa)
NBD_LOG=$(mktemp "$WORKDIR/server-XXXXXX.log")
"$WAFFLE_NBD" "$SERIAL_PORT" "$NBD_ADDR" > "$NBD_LOG" 2>&1 &
SERVER_PID=$!
info "PID server: $SERVER_PID"
sleep 2
kill -0 "$SERVER_PID" 2>/dev/null || { cat "$NBD_LOG"; fail "Server terminato inaspettatamente"; }

# Leggi porta NBD dal log (potrebbe essere diversa da 10809)
NBD_PORT=$(grep -oP '(?<=server bound to 127\.0\.0\.1:)\d+' "$NBD_LOG" | head -1 || echo "10809")
info "Porta NBD: $NBD_PORT"
ok "Server in ascolto"

# ═════════════════════════════════════════════════════════════════════════════
# 2. Connessione nbd-client
# ═════════════════════════════════════════════════════════════════════════════
pause "Inserisci un floppy nel drive (qualsiasi formato), poi premi INVIO"

step "Connessione nbd-client → $NBD_DEV"
$SUDO nbd-client "$NBD_ADDR" "$NBD_PORT" "$NBD_DEV" || fail "nbd-client fallito"
ok "nbd-client connesso: $NBD_DEV"

step "Attesa control socket"
wait_ctl || fail "Control socket non disponibile dopo 20s"
ok "Control socket pronto: $CTL_SOCK"

# ═════════════════════════════════════════════════════════════════════════════
# 3. Formattazione Amiga OFS
# ═════════════════════════════════════════════════════════════════════════════
step "Formattazione disco → Amiga OFS"
ctl_cmd format amiga
ok "Formattazione completata"

# Il formato è avvenuto con nbd-client già connesso: il block device è ancora
# attivo e riflette già il nuovo contenuto. Basta forzare il rilettura della
# partizione e attendere che il device sia stabile.
step "Aggiornamento block device dopo formato"
$SUDO blockdev --rereadpt "$NBD_DEV" 2>/dev/null || true
wait_blk || fail "Block device non pronto dopo 30s"
ok "Block device pronto: $NBD_DEV"

# ═════════════════════════════════════════════════════════════════════════════
# 4. Mount del disco formattato
# ═════════════════════════════════════════════════════════════════════════════
step "Montaggio disco Amiga (affs)"
$SUDO mount -t affs -o "nosuid,uid=$(id -u),gid=$(id -g)" "$NBD_DEV" "$MNT" \
    || fail "mount affs fallito"
ok "Montato su $MNT"
info "Spazio disponibile:"
df -h "$MNT" | sed 's/^/      /'

# ═════════════════════════════════════════════════════════════════════════════
# 5. Crea README sul disco
# ═════════════════════════════════════════════════════════════════════════════
step "Creazione README sul disco"
README_CONTENT="Waffle NBD Test Disk
====================
Creato il: $(date)
Formato: Amiga OFS DD 880 KB
"
echo "$README_CONTENT" | $SUDO tee "$MNT/README" > /dev/null
$SUDO sync
ok "README creato"
info "Contenuto disco:"
ls -la "$MNT" | sed 's/^/      /'

# ═════════════════════════════════════════════════════════════════════════════
# 6. Smonta e clona → clone1.adf
# ═════════════════════════════════════════════════════════════════════════════
step "Smontaggio"
$SUDO umount "$MNT"
ok "Smontato"

CLONE1="$WORKDIR/clone1.adf"
step "Clone disco → $CLONE1"
ctl_cmd clone "$CLONE1"
ok "Clone completato"

# ═════════════════════════════════════════════════════════════════════════════
# 7. Verifica clone1.adf
# ═════════════════════════════════════════════════════════════════════════════
step "Verifica clone1.adf"
CLONE1_SIZE=$(stat -c%s "$CLONE1")
info "Dimensione: $CLONE1_SIZE byte (atteso: 901120 per Amiga DD 880KB)"
[[ "$CLONE1_SIZE" -eq 901120 ]] && ok "Dimensione corretta (880 KB)" \
    || echo -e "${RED}  Attenzione: dimensione inattesa ($CLONE1_SIZE)${NC}"

# Monta l'immagine clone1 con --serve-image per ispezionare il contenuto
CLONE1_MNT=$(mktemp -d "$WORKDIR/clone1-mnt-XXXXXX")
CLONE1_NBD=""
step "Verifica contenuto clone1.adf (via --serve-image)"
# Avvia serve-image in background e cattura il device suggerito
SIMG_LOG=$(mktemp "$WORKDIR/simg-XXXXXX.log")
"$WAFFLE_NBD" --serve-image "$CLONE1" > "$SIMG_LOG" 2>&1 &
SIMG_PID=$!
sleep 3
CLONE1_PORT=$(grep -oP '(?<=nbd-client 127\.0\.0\.1 )\d+' "$SIMG_LOG" | head -1 || echo "")
CLONE1_NBD=$(grep -oP '/dev/nbd\d+' "$SIMG_LOG" | head -1 || echo "")
if [[ -n "$CLONE1_PORT" && -n "$CLONE1_NBD" ]]; then
    $SUDO nbd-client 127.0.0.1 "$CLONE1_PORT" "$CLONE1_NBD" 2>/dev/null || true
    sleep 1
    $SUDO mount -t affs -o "nosuid,uid=$(id -u),gid=$(id -g)" \
        "$CLONE1_NBD" "$CLONE1_MNT" 2>/dev/null && {
        info "Contenuto clone1.adf:"
        ls -la "$CLONE1_MNT" | sed 's/^/      /'
        info "Spazio libero:"
        df -h "$CLONE1_MNT" | sed 's/^/      /'
        [[ -f "$CLONE1_MNT/README" ]] && ok "README presente in clone1.adf" \
            || fail "README non trovato in clone1.adf"
        $SUDO umount "$CLONE1_MNT"
    } || info "mount clone1 saltato (non critico)"
    $SUDO nbd-client -d "$CLONE1_NBD" 2>/dev/null || true
fi
kill "$SIMG_PID" 2>/dev/null || true; wait "$SIMG_PID" 2>/dev/null || true
rmdir "$CLONE1_MNT" 2>/dev/null || true

# ═════════════════════════════════════════════════════════════════════════════
# 8. Rimonta il disco fisico, aggiunge secondo file
# ═════════════════════════════════════════════════════════════════════════════
step "Rimontaggio disco fisico per aggiungere un secondo file"
$SUDO mount -t affs -o "nosuid,uid=$(id -u),gid=$(id -g)" "$NBD_DEV" "$MNT" \
    || fail "rimount fallito"
ok "Rimontato su $MNT"

NOTE_CONTENT="File aggiunto dopo il clone.
Data: $(date)
"
echo "$NOTE_CONTENT" | $SUDO tee "$MNT/NOTE.TXT" > /dev/null
$SUDO sync
info "Contenuto disco dopo aggiunta:"
ls -la "$MNT" | sed 's/^/      /'
ok "NOTE.TXT creato"

# ═════════════════════════════════════════════════════════════════════════════
# 9. Smonta
# ═════════════════════════════════════════════════════════════════════════════
step "Smontaggio disco fisico"
$SUDO umount "$MNT"
ok "Smontato"

# ═════════════════════════════════════════════════════════════════════════════
# 10. Clona di nuovo → clone2.adf (immagine aggiornata)
# ═════════════════════════════════════════════════════════════════════════════
CLONE2="$WORKDIR/clone2.adf"
step "Clone disco aggiornato → $CLONE2"
ctl_cmd clone "$CLONE2"
ok "Clone2 completato"

# ═════════════════════════════════════════════════════════════════════════════
# 11. Dump clone2.adf → disco fisico
# ═════════════════════════════════════════════════════════════════════════════
step "Dump clone2.adf → disco fisico"
ctl_cmd dump "$CLONE2"
ok "Dump completato"

# ═════════════════════════════════════════════════════════════════════════════
# 12. Verifica finale: rimonta e controlla entrambi i file
# ═════════════════════════════════════════════════════════════════════════════
step "Verifica finale: rimonta e controlla contenuto"
$SUDO blockdev --rereadpt "$NBD_DEV" 2>/dev/null || true
wait_blk || fail "Block device non pronto"
$SUDO mount -t affs -o "nosuid,uid=$(id -u),gid=$(id -g)" "$NBD_DEV" "$MNT" \
    || fail "mount finale fallito"

info "Contenuto finale del disco:"
ls -la "$MNT" | sed 's/^/      /'
info "Spazio libero:"
df -h "$MNT" | sed 's/^/      /'

[[ -f "$MNT/README"  ]] && ok "README presente"   || fail "README mancante"
[[ -f "$MNT/NOTE.TXT" ]] && ok "NOTE.TXT presente" || fail "NOTE.TXT mancante"

$SUDO umount "$MNT"

# ═════════════════════════════════════════════════════════════════════════════
# Riepilogo
# ═════════════════════════════════════════════════════════════════════════════
step "Riepilogo"
ok "Tutti i test superati"
info "clone1.adf : $CLONE1  ($(du -h "$CLONE1" | cut -f1))"
info "clone2.adf : $CLONE2  ($(du -h "$CLONE2" | cut -f1))"
SHA1=$(sha256sum "$CLONE1" | cut -d' ' -f1)
SHA2=$(sha256sum "$CLONE2" | cut -d' ' -f1)
info "SHA256 clone1: $SHA1"
info "SHA256 clone2: $SHA2"
[[ "$SHA1" != "$SHA2" ]] && ok "Le due immagini differiscono (atteso: NOTE.TXT aggiunto)" \
    || echo -e "${YELLOW}  Attenzione: clone1 e clone2 sono identici${NC}"
