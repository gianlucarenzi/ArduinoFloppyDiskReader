#!/bin/bash
# Filesystem tests for waffle-fuse on disk images (no hardware needed).
#
# For every format a reference tree is copied onto a freshly formatted image
# through waffle-fuse, then files are moved, renamed, truncated, appended and
# deleted.  The result is checked three ways:
#   1. through the mounted filesystem itself,
#   2. after a remount (what really reached the image),
#   3. with independent tools: xdftool/xdfscan (amitools) for AmigaDOS,
#      pyfatfs + fsck.fat for FAT.
#
# Requirements: fusermount3, mkfs.fat, fsck.fat; amitools (xdftool, xdfscan)
# and pyfatfs for the independent checks.  Point XDFTOOL, XDFSCAN and
# TEST_PYTHON at them if they are not in PATH (e.g. inside a virtualenv).

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/../waffle-fuse"
XDFTOOL="${XDFTOOL:-xdftool}"
XDFSCAN="${XDFSCAN:-xdfscan}"
TEST_PYTHON="${TEST_PYTHON:-python3}"
MKFS_FAT="$(command -v mkfs.fat || echo /sbin/mkfs.fat)"
FSCK_FAT="$(command -v fsck.fat || echo /sbin/fsck.fat)"

WORK="$(mktemp -d /tmp/waffle-test.XXXXXX)"
MNT="$WORK/mnt"
mkdir -p "$MNT"
PASS=0
FAIL=0
PID=

cleanup() {
    mountpoint -q "$MNT" 2>/dev/null && fusermount3 -u "$MNT"
    [ -n "$PID" ] && wait "$PID" 2>/dev/null
    rm -rf "$WORK"
}
trap cleanup EXIT

ok()   { echo "  ok   $*"; PASS=$((PASS+1)); }
bad()  { echo "  FAIL $*"; FAIL=$((FAIL+1)); }
check() { local what="$1"; shift; if "$@" >"$WORK/check.out" 2>&1; then ok "$what"; else bad "$what"; sed 's/^/       /' "$WORK/check.out" | head -20; fi; }

mount_img() {
    local img="$1"; shift
    "$BIN" --image "$img" "$MNT" -f "$@" >"$WORK/fuse.log" 2>&1 &
    PID=$!
    for _ in $(seq 50); do
        mountpoint -q "$MNT" && return 0
        kill -0 "$PID" 2>/dev/null || break
        sleep 0.1
    done
    echo "mount failed:"; cat "$WORK/fuse.log"
    return 1
}

umount_img() {
    fusermount3 -u "$MNT"
    wait "$PID"
    local rc=$?
    PID=
    return $rc
}

# Deterministic pseudo-random file of a given size
mkfile() { head -c "$2" /dev/urandom > "$1"; }

# Reference tree.  $1 = dir, $2 = 1 if long names (> 30 chars) are allowed
make_ref() {
    local d="$1" long="$2"
    mkdir -p "$d/Dir One/Nested" "$d/dir2" "$d/many"
    for s in 0 1 487 488 489 512 513 5000; do mkfile "$d/f$s" "$s"; done
    mkfile "$d/big40k" 40000            # > 72 blocks: needs an extension block
    mkfile "$d/big150k.bin" 150000      # several extension blocks / long FAT chain
    mkfile "$d/Dir One/inner.dat" 3000
    mkfile "$d/Dir One/Nested/deep.txt" 700
    echo "hello amiga" > "$d/ReadMe.TXT"
    echo "small" > "$d/small.txt"
    echo "lettera" > "$d/Lettera all'ufficio.txt"
    echo "accenti" > "$d/Città è più.txt"
    if [ "$long" = 1 ]; then
        echo "long" > "$d/A very long file name that does not fit in 8.3.text"
    fi
    for i in $(seq 1 40); do echo "file $i" > "$d/many/Entry number $i.txt"; done
}

# Apply the same modifications to the mounted fs ($1) and to the expected tree ($2)
mutate() {
    local m
    for m in "$1" "$2"; do
        mv "$m/small.txt" "$m/Dir One/moved.txt" &&
        mv "$m/dir2" "$m/Dir One/Nested/dir2moved" &&
        rm "$m/f1" &&
        echo "new content" > "$m/f5000" &&
        head -c 777 /dev/zero >> "$m/big40k" &&
        truncate -s 100 "$m/big150k.bin" &&
        truncate -s 3000 "$m/f487" &&
        mv "$m/ReadMe.TXT" "$m/readme.txt" &&
        mkdir "$m/tmpdir" && rmdir "$m/tmpdir" &&
        rm "$m/many/Entry number 7.txt" &&
        mv "$m/many/Entry number 8.txt" "$m/many/Renamed eight.txt" || return 1
    done
}

run_suite() {
    local name="$1" img="$2" kind="$3" long="$4"
    echo "== $name"
    rm -rf "$WORK/ref" "$WORK/exp" "$WORK/out"
    make_ref "$WORK/ref" "$long"
    cp -a "$WORK/ref" "$WORK/exp"

    mount_img "$img" || { bad "$name: mount"; return; }
    check "copy reference tree" cp -r "$WORK/ref/." "$MNT/"
    check "tree matches while mounted" diff -r "$WORK/ref" "$MNT"
    check "move/rename/truncate/append/delete" mutate "$MNT" "$WORK/exp"
    check "modified tree matches while mounted" diff -r "$WORK/exp" "$MNT"
    if [ "$kind" = affs ] && [ "$long" = 0 ]; then
        if touch "$MNT/This name is definitely longer than thirty chars" 2>/dev/null; then
            bad "names > 30 chars must be rejected"
        else
            ok "names > 30 chars rejected"
        fi
    fi
    # Disk full: the copy must fail cleanly and leave a consistent filesystem
    local avail; avail=$(df --output=avail -B1 "$MNT" | tail -1)
    mkfile "$WORK/toobig" $((avail + 20000))
    if cp "$WORK/toobig" "$MNT/toobig" 2>/dev/null; then bad "disk full reported"; else ok "disk full reported"; fi
    rm -f "$MNT/toobig"
    check "tree intact after disk full" diff -r "$WORK/exp" "$MNT"
    check "unmount" umount_img

    mount_img "$img" || { bad "$name: remount"; return; }
    check "tree matches after remount" diff -r "$WORK/exp" "$MNT"
    umount_img

    if [ "$kind" = affs ]; then
        if command -v "$XDFSCAN" >/dev/null 2>&1; then
            check "xdfscan: no errors or warnings" bash -c "out=\$('$XDFSCAN' -l 2 '$img' 2>&1); echo \"\$out\"; ! echo \"\$out\" | grep -qiE 'error|warn'"
            mkdir -p "$WORK/out"
            "$XDFTOOL" "$img" unpack "$WORK/out" >/dev/null 2>&1
            local vol; vol="$(ls "$WORK/out" | grep -v xdfmeta | head -1)"
            check "xdftool extraction matches" diff -r -x '*.xdfmeta' "$WORK/exp" "$WORK/out/$vol"
        else
            echo "  skip independent AmigaDOS checks (amitools not found)"
        fi
    else
        check "fsck.fat: clean" "$FSCK_FAT" -n "$img"
        if "$TEST_PYTHON" -c "import pyfatfs" 2>/dev/null; then
            check "pyfatfs extraction matches" bash -c "'$TEST_PYTHON' '$HERE/extract.py' '$img' '$WORK/out' && diff -r '$WORK/exp' '$WORK/out'"
        else
            echo "  skip pyfatfs check (not installed)"
        fi
    fi
}

# ── Amiga ────────────────────────────────────────────────────────────────────
if command -v "$XDFTOOL" >/dev/null 2>&1; then
    for t in "ofs DD" "ffs DD" "ffs+intl DD" "ffs HD"; do
        set -- $t
        img="$WORK/amiga-$1-$2.adf"
        if [ "$2" = HD ]; then
            "$XDFTOOL" "$img" create size=1760Ki + format "Test Volume" "$1" >/dev/null
        else
            "$XDFTOOL" "$img" format "Test Volume" "$1" >/dev/null
        fi
        run_suite "Amiga $1 $2" "$img" affs 0
    done
else
    echo "== Amiga: skipped (xdftool not found, set XDFTOOL)"
fi

# ── FAT ──────────────────────────────────────────────────────────────────────
for kb in 720 1440; do
    img="$WORK/fat-$kb.img"
    "$MKFS_FAT" -C -n "WAFFLE" "$img" "$kb" >/dev/null
    run_suite "FAT ${kb}K" "$img" fat 1
done

# ── Images formatted by waffle-fuse itself ───────────────────────────────────
for t in "ofs dd" "ffs hd" "amiga-intl dd"; do
    set -- $t
    img="$WORK/wf-$1-$2.adf"
    "$BIN" --format-image "$img" "$1" "$2" "Test Volume" 2>/dev/null
    run_suite "waffle-fuse format: $1 $2" "$img" affs 0
done
for d in dd hd; do
    img="$WORK/wf-dos-$d.img"
    "$BIN" --format-image "$img" dos "$d" "WAFFLE" 2>/dev/null
    run_suite "waffle-fuse format: dos $d" "$img" fat 1
done

# ── Read-only mount ──────────────────────────────────────────────────────────
echo "== read-only image"
img="$WORK/ro.img"
"$MKFS_FAT" -C "$img" 720 >/dev/null
mount_img "$img" -o ro || bad "ro mount"
if touch "$MNT/x" 2>/dev/null; then bad "write refused on ro mount"; else ok "write refused on ro mount"; fi
umount_img

# ── Automount directory naming ───────────────────────────────────────────────
echo "== automount"
"$BIN" --image "$WORK/fat-720.img" --automount "$WORK/auto" -f >"$WORK/auto.log" 2>&1 &
PID=$!
for _ in $(seq 50); do mountpoint -q "$WORK/auto/WAFFLE" && break; sleep 0.1; done
if mountpoint -q "$WORK/auto/WAFFLE"; then
    ok "mounted on <dir>/<volume label>"
    grep -q "^WAFFLE event=mounted mountpoint=$WORK/auto/WAFFLE" "$WORK/auto.log" && ok "mounted event" || bad "mounted event"
    fusermount3 -u "$WORK/auto/WAFFLE"; wait "$PID"; PID=
    [ ! -e "$WORK/auto/WAFFLE" ] && ok "mountpoint removed after unmount" || bad "mountpoint removed after unmount"
else
    bad "automount"; cat "$WORK/auto.log"
fi

echo
echo "passed: $PASS  failed: $FAIL"
[ "$FAIL" -eq 0 ]
