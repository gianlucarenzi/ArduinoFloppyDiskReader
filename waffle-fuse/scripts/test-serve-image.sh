#!/bin/bash
# test-serve-image.sh – Test automatico di waffle-serve-image.sh
#
# Avvia il server su un'immagine ADF/IMG, attende il montaggio,
# verifica il contenuto, poi smonta e controlla la pulizia.
#
# Uso: scripts/test-serve-image.sh <image.adf|img>

set -euo pipefail

IMAGE="${1:-}"
if [ -z "$IMAGE" ] || [ ! -f "$IMAGE" ]; then
    echo "Uso: $(basename "$0") <image.adf|img>" >&2
    exit 1
fi
IMAGE="$(realpath "$IMAGE")"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SERVE_SCRIPT="$SCRIPT_DIR/waffle-serve-image.sh"

if [ ! -x "$SERVE_SCRIPT" ]; then
    echo "ERRORE: $SERVE_SCRIPT non trovato o non eseguibile" >&2
    exit 1
fi

# ── Helper ────────────────────────────────────────────────────────────────────
GREEN='\033[0;32m'; RED='\033[0;31m'; RESET='\033[0m'; BOLD='\033[1m'
step() { echo -e "\n${BOLD}══ $* ${RESET}"; }
ok()   { echo -e "${GREEN}✔ $*${RESET}"; }
fail() { echo -e "${RED}✘ $*${RESET}"; sudo kill "$SERVE_PID" 2>/dev/null || true; exit 1; }

# Determina se serve sudo
if [ "$(id -u)" -ne 0 ]; then
    echo "Alcune operazioni richiedono privilegi (modprobe, nbd-client, mount)."
    SUDO="sudo"
else
    SUDO=""
fi

# ── Moduli ────────────────────────────────────────────────────────────────────
step "Caricamento moduli kernel"
$SUDO modprobe nbd  2>/dev/null || true
$SUDO modprobe affs 2>/dev/null || true
ok "Moduli caricati"

# ── Avvio serve-image ─────────────────────────────────────────────────────────
step "Avvio waffle-serve-image.sh su $(basename "$IMAGE")"

LOG=$(mktemp /tmp/waffle-serve-XXXXXX.log)
$SUDO "$SERVE_SCRIPT" "$IMAGE" > "$LOG" 2>&1 &
SERVE_PID=$!
echo "    PID: $SERVE_PID  log: $LOG"

# ── Attesa montaggio ──────────────────────────────────────────────────────────
step "Attesa montaggio (max 30s)"
MNT=""
for i in $(seq 1 30); do
    MNT=$(grep -oP '(?<=mounted at ).*' "$LOG" 2>/dev/null | tail -1 || true)
    # anche udisksctl: "udisksctl mounted at /run/media/..."
    [ -z "$MNT" ] && MNT=$(grep -oP '(?<=Mounted .* at )\S+' "$LOG" 2>/dev/null | tail -1 || true)
    if [ -n "$MNT" ] && mountpoint -q "$MNT" 2>/dev/null; then
        break
    fi
    MNT=""
    sleep 1
    echo -n "."
done
echo

if [ -z "$MNT" ]; then
    echo "--- log ---"
    cat "$LOG"
    fail "Montaggio non avvenuto entro 30s"
fi
ok "Montato su $MNT"

# ── Verifica device NBD ───────────────────────────────────────────────────────
step "Verifica device NBD"
NBD_DEV=$(grep -oP '/dev/nbd\d+' "$LOG" | head -1 || true)
if [ -n "$NBD_DEV" ]; then
    ok "Device: $NBD_DEV"
    lsblk "$NBD_DEV" 2>/dev/null | sed 's/^/    /' || true
else
    echo "    (device NBD non estratto dal log)"
fi

# ── Contenuto ────────────────────────────────────────────────────────────────
step "Contenuto del mount point"
ls -la "$MNT" | head -15 | sed 's/^/    /'
FILE_COUNT=$(find "$MNT" -maxdepth 1 -not -name '.' | wc -l)
ok "$FILE_COUNT file/dir al primo livello"

# ── Smontaggio ────────────────────────────────────────────────────────────────
step "Smontaggio"
$SUDO umount "$MNT" 2>/dev/null || $SUDO umount -l "$MNT" 2>/dev/null || true

# Attendi che il watcher nel script se ne accorga e faccia cleanup
sleep 4

if mountpoint -q "$MNT" 2>/dev/null; then
    fail "Mountpoint ancora attivo dopo umount"
fi
ok "Smontato correttamente"

# ── Verifica pulizia ──────────────────────────────────────────────────────────
step "Verifica pulizia processo"
$SUDO kill "$SERVE_PID" 2>/dev/null || true
wait "$SERVE_PID" 2>/dev/null || true

if [ -n "$NBD_DEV" ]; then
    PID_FILE="/sys/block/${NBD_DEV##*/}/pid"
    PID_VAL=$(cat "$PID_FILE" 2>/dev/null || echo "")
    if [ -z "$PID_VAL" ] || [ "$PID_VAL" -eq 0 ] 2>/dev/null; then
        ok "$NBD_DEV liberato"
    else
        echo "    Attenzione: $NBD_DEV ancora in uso (pid=$PID_VAL)"
    fi
fi

# ── Riepilogo ─────────────────────────────────────────────────────────────────
step "Riepilogo"
ok "Test completato con successo"
echo "    Immagine: $IMAGE"
echo "    Device:   ${NBD_DEV:-n/d}"
echo "    Mount:    $MNT"
echo ""
echo "    Log completo: $LOG"
