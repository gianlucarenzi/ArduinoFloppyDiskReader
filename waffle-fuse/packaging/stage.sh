#!/bin/sh
# stage.sh – assemble the installed waffle-fuse tree (what goes to /opt/waffle-fuse)
#
#   packaging/stage.sh <waffle-fuse binary> <output dir>
#
# The tree mirrors the source layout: the scripts find the binary, icons,
# MIME types and launcher templates relative to scripts/..

set -e

BIN="$1"
OUT="$2"
if [ ! -x "$BIN" ] || [ -z "$OUT" ]; then
    echo "Usage: $0 <waffle-fuse binary> <output dir>" >&2
    exit 1
fi
SRC="$(cd "$(dirname "$0")/.." && pwd)"

umask 022
rm -rf "$OUT"
mkdir -p "$OUT/scripts" "$OUT/icons" "$OUT/mime"
install -m 755 "$BIN" "$OUT/waffle-fuse"
install -m 755 "$SRC"/scripts/*.py "$SRC"/scripts/*.sh "$OUT/scripts/"
install -m 644 "$SRC"/icons/*.svg "$OUT/icons/"
install -m 644 "$SRC"/mime/*.xml "$OUT/mime/"
install -m 644 "$SRC"/*.desktop "$OUT/"
install -m 644 "$SRC/README.md" "$OUT/"
echo "Staged waffle-fuse in $OUT"
