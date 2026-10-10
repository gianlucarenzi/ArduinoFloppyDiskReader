#!/bin/sh
# install-system.sh – system-wide setup for waffle-fuse (needs root)
#
#   pkexec /path/to/install-system.sh [user...]   (what the graphical installer does)
#   sudo   ./scripts/install-system.sh [user...]  default user: the one running sudo/pkexec
#   sudo   ./scripts/install-system.sh --uninstall
#
# 1. Mount directory like udisks.  udisks2 mounts removable media under
#    /media/<user> (Debian, Ubuntu) or /run/media/<user> (Arch, Fedora, openSUSE...),
#    creating that directory itself (root, 0700, ACL user:r-x) and only when it
#    mounts a block device.  waffle-fuse runs as the user and cannot create
#    directories there, so /etc/tmpfiles.d/waffle-fuse.conf makes systemd
#    create it at every boot with an ACL that also lets the user write.
#    udisks2 only sets its own ACL when it creates the directory itself, so it
#    keeps working as before.
# 2. Serial port access: adds the user to the group owning the USB serial
#    devices (dialout on Debian, uucp on Arch) if not already a member.

set -e

CONF=/etc/tmpfiles.d/waffle-fuse.conf

if [ "$(id -u)" -ne 0 ]; then
    echo "Run me as root: pkexec $0 or sudo $0" >&2
    exit 1
fi

# ── Where does udisks2 mount? ────────────────────────────────────────────────
media_base() {
    for d in /usr/libexec/udisks2/udisksd /usr/lib/udisks2/udisksd /usr/libexec/udisksd /usr/lib/udisksd; do
        [ -x "$d" ] || continue
        if grep -aq '/run/media/%s' "$d"; then echo /run/media; return; fi
        if grep -aq '/media/%s' "$d"; then echo /media; return; fi
    done
    # No udisks2: follow the distribution's convention
    if [ -d /run/media ]; then echo /run/media; else echo /media; fi
}

if [ "${1:-}" = "--uninstall" ]; then
    if [ -f "$CONF" ]; then
        # Remove our ACL entries; the directories stay (udisks uses them)
        sed -n 's|^a+ \([^ ]*\) .* u:\([^:]*\):rwx$|\1 \2|p' "$CONF" | while read -r dir u; do
            [ -d "$dir" ] && setfacl -x "u:$u" "$dir" 2>/dev/null || true
        done
        rm -f "$CONF"
        echo "Removed $CONF"
    else
        echo "Nothing to remove"
    fi
    exit 0
fi

USERS="$*"
[ -z "$USERS" ] && USERS="${SUDO_USER:-}"
# pkexec does not set SUDO_USER but PKEXEC_UID
[ -z "$USERS" ] && [ -n "${PKEXEC_UID:-}" ] && USERS="$(id -nu "$PKEXEC_UID")"
if [ -z "$USERS" ]; then
    echo "No user given: $0 <user>" >&2
    exit 1
fi

for tool in systemd-tmpfiles setfacl; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing $tool (packages: systemd, acl)" >&2
        exit 1
    fi
done

BASE="$(media_base)"
echo "udisks mount directory: $BASE/<user>"

{
    echo "# Installed by waffle-fuse scripts/install-system.sh"
    echo "# $BASE/<user> writable by <user>, so waffle-fuse can mount floppies there like udisks"
    [ "$BASE" = /run/media ] && echo "d /run/media 0755 root root -"
    for u in $USERS; do
        if ! id "$u" >/dev/null 2>&1; then
            echo "Unknown user: $u" >&2
            exit 1
        fi
        echo "d $BASE/$u 0750 root root -"
        echo "a+ $BASE/$u - - - - u:$u:rwx"
    done
} > "$CONF.tmp"
mv "$CONF.tmp" "$CONF"
chmod 644 "$CONF"
echo "Installed $CONF"
systemd-tmpfiles --create "$CONF"

# ── Serial port group ────────────────────────────────────────────────────────
GROUP=""
DEV=""
for dev in /dev/ttyUSB* /dev/ttyACM*; do
    [ -e "$dev" ] && { DEV="$dev"; GROUP="$(stat -c %G "$dev")"; break; }
done
if [ -z "$GROUP" ] || [ "$GROUP" = root ]; then
    for g in dialout uucp; do getent group "$g" >/dev/null && { GROUP="$g"; break; }; done
fi

RELOGIN=0
for u in $USERS; do
    if command -v getfacl >/dev/null 2>&1; then
        getfacl -p "$BASE/$u" 2>/dev/null | grep -E "^(# file|user:$u)" || true
    fi
    # Already able to open the port (group or a session ACL)?  Then leave it.
    if [ -n "$DEV" ] && runuser -u "$u" -- test -r "$DEV" -a -w "$DEV" 2>/dev/null; then
        continue
    fi
    if [ -n "$GROUP" ] && ! id -nG "$u" | tr ' ' '\n' | grep -qx "$GROUP"; then
        usermod -aG "$GROUP" "$u"
        echo "Added $u to group $GROUP (serial port access): log out and in again"
        RELOGIN=1
    fi
done

echo "System setup done."
[ $RELOGIN -eq 1 ] && exit 10   # tells the installer a new login is needed
exit 0
