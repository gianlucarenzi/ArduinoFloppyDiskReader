#!/bin/bash
# test-amiga-format-clone.sh
#
# Test end-to-end flusso Amiga:
#   1. Avvia waffle-nbd su /dev/ttyUSB0
#   2. Connette nbd-client (inserire disco quando richiesto)
#   3. Formatta il disco → Amiga OFS via control socket
#   4. Riconnette nbd-client per aggiornare la geometria (720KB→880KB)
#   5. Monta il disco formattato (affs), crea README
#   6. Smonta e clona → clone1.adf; verifica dimensione e contenuto
#   7. Rimonta, aggiunge NOTE.TXT
#   8. Smonta e clona → clone2.adf
#   9. Dump clone2.adf → disco fisico; riconnette e verifica entrambi i file
#
# Uso: scripts/test-amiga-format-clone.sh [/dev/ttyUSB0]

set -euo pipefail

# ── Configurazione ─────────────────────────────────────────────────────────────
SERIAL_PORT="${1:-${SERIAL_PORT:-/dev/ttyUSB0}}"
NBD_ADDR="127.0.0.1"
NBD_DEV="/dev/nbd0"
CTL_SOCK="/run/waffle-nbd/$(basename "$SERIAL_PORT").ctl"
WORKDIR="/tmp/waf-test-$$"
WAFFLE_NBD="${WAFFLE_NBD:-$(dirname "$0")/../waffle-nbd}"
SERVER_PID=""
MOUNT_FLAGS="rw,users,nosuid,uid=$(id -u),gid=$(id -g)"

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

# ── Pulizia ────────────────────────────────────────────────────────────────────
cleanup() {
    echo "==> Pulizia..."
    # Smonta qualsiasi mountpoint ancora attivo sotto WORKDIR
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

# ── Helper: crea mountpoint temporaneo (come utente, non root) ────────────────
make_mnt() {
    local mnt; mnt=$(mktemp -d "$WORKDIR/mnt-XXXXXX")
    echo "$mnt"
}

# ── Helper: smonta e rimuove mountpoint ───────────────────────────────────────
do_umount() {
    local mnt="$1"
    $SUDO umount "$mnt" || $SUDO umount -l "$mnt" 2>/dev/null || true
    rmdir "$mnt" 2>/dev/null || true
}

# ── Helper: monta affs su $NBD_DEV in un nuovo mountpoint ────────────────────
mount_affs() {
    local dev="$1" mnt="$2"
    $SUDO mount -t affs -o "$MOUNT_FLAGS" "$dev" "$mnt"
}

# ── Helper: attesa control socket ─────────────────────────────────────────────
wait_ctl() {
    for ((i=0; i<20; i++)); do [[ -S "$CTL_SOCK" ]] && return 0; sleep 1; done
    return 1
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

# ── Helper: invia comando al control socket ────────────────────────────────────
ctl_cmd() {
    info "Invio comando: $*"
    "$WAFFLE_NBD" --ctl "$CTL_SOCK" "$@" || fail "comando ctl fallito: $*"
}

# ── Helper: disconnetti e riconnetti nbd-client ────────────────────────────────
# Il server dopo format/dump salta Phase 1+2 (porta già aperta) ed è pronto
# quasi immediatamente. Proviamo più volte con intervallo breve.
reconnect_nbd() {
    $SUDO nbd-client -d "$NBD_DEV" 2>/dev/null || true
    info "Attesa server pronto per nuova connessione..."
    for ((i=0; i<20; i++)); do
        sleep 1
        if $SUDO nbd-client "$NBD_ADDR" "$NBD_PORT" "$NBD_DEV" 2>/dev/null; then
            return 0
        fi
    done
    return 1
}

# ═════════════════════════════════════════════════════════════════════════════
# 1. Moduli kernel + avvio server
# ═════════════════════════════════════════════════════════════════════════════
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
ok "Server in ascolto"

# ═════════════════════════════════════════════════════════════════════════════
# 2. Prima connessione nbd-client
# ═════════════════════════════════════════════════════════════════════════════
pause "Inserisci un floppy nel drive (qualsiasi formato), poi premi INVIO"

step "Connessione nbd-client → $NBD_DEV"
$SUDO nbd-client "$NBD_ADDR" "$NBD_PORT" "$NBD_DEV" || fail "nbd-client fallito"
wait_blk || fail "Block device non pronto dopo 30s"
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

# ═════════════════════════════════════════════════════════════════════════════
# 4. Riconnessione per aggiornare la geometria NBD (720KB PC → 880KB Amiga)
# ═════════════════════════════════════════════════════════════════════════════
step "Aggiornamento geometria NBD (720KB → 880KB)"
reconnect_nbd || fail "Riconnessione NBD fallita dopo 20s"
wait_blk || fail "Block device non pronto dopo 30s"
ok "Riconnesso: $NBD_DEV — $(( $(cat /sys/block/${NBD_DEV##*/}/size) * 512 / 1024 )) KB"

# ═════════════════════════════════════════════════════════════════════════════
# 5. Primo mount: crea README
# ═════════════════════════════════════════════════════════════════════════════
step "Montaggio disco Amiga (affs)"
MNT1=$(make_mnt)
mount_affs "$NBD_DEV" "$MNT1" || fail "mount affs fallito"
ok "Montato su $MNT1"
df -h "$MNT1" | sed 's/^/    /'

step "Creazione README"
cat > "$MNT1/README" <<EOF
Waffle NBD Test Disk
====================
Creato il: $(date)
Formato: Amiga OFS DD 880 KB
EOF
sync
ok "README creato"
ls -la "$MNT1" | sed 's/^/    /'

# ═════════════════════════════════════════════════════════════════════════════
# 6. Smonta + clone1.adf + verifica
# ═════════════════════════════════════════════════════════════════════════════
step "Smontaggio"
do_umount "$MNT1"
ok "Smontato"

CLONE1="$WORKDIR/clone1.adf"
step "Clone → $CLONE1"
ctl_cmd clone "$CLONE1"
CLONE1_SIZE=$(stat -c%s "$CLONE1")
info "Dimensione: $CLONE1_SIZE byte (atteso 901120 per Amiga DD 880KB)"
[[ "$CLONE1_SIZE" -eq 901120 ]] && ok "Dimensione corretta (880 KB)" \
    || echo -e "${YELLOW}  Attenzione: dimensione inattesa ($CLONE1_SIZE)${NC}"

step "Verifica contenuto clone1.adf (via --serve-image)"
SIMG_LOG=$(mktemp "$WORKDIR/simg-XXXXXX.log")
"$WAFFLE_NBD" --serve-image "$CLONE1" > "$SIMG_LOG" 2>&1 &
SIMG_PID=$!
sleep 3
CLONE1_PORT=$(grep -oP '(?<=nbd-client 127\.0\.0\.1 )\d+' "$SIMG_LOG" | head -1 || true)
CLONE1_NBD=$(grep  -oP '/dev/nbd\d+'                        "$SIMG_LOG" | head -1 || true)
if [[ -n "$CLONE1_PORT" && -n "$CLONE1_NBD" ]]; then
    $SUDO nbd-client 127.0.0.1 "$CLONE1_PORT" "$CLONE1_NBD" 2>/dev/null || true
    sleep 1
    MNT_C1=$(make_mnt)
    if $SUDO mount -t affs -o "$MOUNT_FLAGS" "$CLONE1_NBD" "$MNT_C1" 2>/dev/null; then
        info "Contenuto clone1.adf:"; ls -la "$MNT_C1" | sed 's/^/    /'
        df -h "$MNT_C1" | sed 's/^/    /'
        [[ -f "$MNT_C1/README" ]] && ok "README presente in clone1.adf" \
            || fail "README non trovato in clone1.adf"
        do_umount "$MNT_C1"
    else
        rmdir "$MNT_C1" 2>/dev/null || true
        info "mount clone1 non riuscito (non critico)"
    fi
    $SUDO nbd-client -d "$CLONE1_NBD" 2>/dev/null || true
fi
kill "$SIMG_PID" 2>/dev/null || true; wait "$SIMG_PID" 2>/dev/null || true

# ═════════════════════════════════════════════════════════════════════════════
# 7. Secondo mount: aggiunge NOTE.TXT
# ═════════════════════════════════════════════════════════════════════════════
step "Rimontaggio disco fisico"
MNT2=$(make_mnt)
mount_affs "$NBD_DEV" "$MNT2" || fail "rimount fallito"
ok "Rimontato su $MNT2"

cat > "$MNT2/NOTE.TXT" <<EOF
File aggiunto dopo il clone.
Data: $(date)
EOF
sync
info "Contenuto dopo aggiunta:"; ls -la "$MNT2" | sed 's/^/    /'
ok "NOTE.TXT creato"

# ═════════════════════════════════════════════════════════════════════════════
# 8. Smonta + clone2.adf
# ═════════════════════════════════════════════════════════════════════════════
step "Smontaggio"
do_umount "$MNT2"
ok "Smontato"

CLONE2="$WORKDIR/clone2.adf"
step "Clone → $CLONE2"
ctl_cmd clone "$CLONE2"
ok "Clone2 completato"

# ═════════════════════════════════════════════════════════════════════════════
# 9. Dump clone2.adf → disco fisico + verifica finale
# ═════════════════════════════════════════════════════════════════════════════
step "Dump clone2.adf → disco fisico"
ctl_cmd dump "$CLONE2"
ok "Dump completato"

step "Riconnessione dopo dump"
reconnect_nbd || fail "Riconnessione dopo dump fallita"
wait_blk || fail "Block device non pronto"

step "Verifica finale"
MNT3=$(make_mnt)
mount_affs "$NBD_DEV" "$MNT3" || fail "mount finale fallito"
ok "Montato su $MNT3"
info "Contenuto finale:"; ls -la "$MNT3" | sed 's/^/    /'
df -h "$MNT3" | sed 's/^/    /'

[[ -f "$MNT3/README"   ]] && ok "README presente"   || fail "README mancante"
[[ -f "$MNT3/NOTE.TXT" ]] && ok "NOTE.TXT presente" || fail "NOTE.TXT mancante"

do_umount "$MNT3"

# ═════════════════════════════════════════════════════════════════════════════
# Riepilogo
# ═════════════════════════════════════════════════════════════════════════════
step "Riepilogo"
ok "Tutti i test superati"
info "clone1.adf : $(du -h "$CLONE1" | cut -f1)  $CLONE1"
info "clone2.adf : $(du -h "$CLONE2" | cut -f1)  $CLONE2"
SHA1=$(sha256sum "$CLONE1" | cut -d' ' -f1)
SHA2=$(sha256sum "$CLONE2" | cut -d' ' -f1)
info "SHA256 clone1: $SHA1"
info "SHA256 clone2: $SHA2"
[[ "$SHA1" != "$SHA2" ]] \
    && ok "Le due immagini differiscono (atteso: NOTE.TXT aggiunto)" \
    || echo -e "${YELLOW}  Attenzione: clone1 e clone2 sono identici${NC}"
