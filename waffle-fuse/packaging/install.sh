#!/bin/sh
# install.sh – install waffle-fuse on any Linux distribution (x86_64, glibc >= 2.31)
#
#   ./install.sh               install to /opt/waffle-fuse (asks for the administrator password)
#   ./install.sh --uninstall   remove it (also: /opt/waffle-fuse/install.sh --uninstall)
#
# For distributions without a .deb, .rpm or Arch package. After installing,
# this directory can be deleted. Each user then starts "Waffle Installer"
# from the menu (or runs waffle-installer) for the launchers and tray daemon.

set -e

PREFIX=/opt/waffle-fuse
BIN_DIR=/usr/local/bin
APP_DIR=/usr/local/share/applications
ICON_DIR=/usr/local/share/icons/hicolor/scalable/apps
HERE="$(cd "$(dirname "$0")" && pwd)"

if [ "$(id -u)" -ne 0 ]; then
    echo "The administrator password is needed to install waffle-fuse."
    if command -v sudo >/dev/null 2>&1; then
        exec sudo sh "$HERE/install.sh" "$@"
    elif command -v pkexec >/dev/null 2>&1; then
        exec pkexec sh "$HERE/install.sh" "$@"
    fi
    echo "Neither sudo nor pkexec found: run me as root" >&2
    exit 1
fi

if [ -e /usr/bin/waffle-fuse ]; then
    echo "waffle-fuse is installed by the package manager: use it to update or remove it" >&2
    exit 1
fi

refresh_desktop() {
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q "$APP_DIR" || true
    command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t /usr/local/share/icons/hicolor || true
}

if [ "${1:-}" = "--uninstall" ]; then
    if [ -x "$PREFIX/scripts/install-system.sh" ]; then
        "$PREFIX/scripts/install-system.sh" --uninstall || true
    fi
    rm -f "$BIN_DIR/waffle-fuse" "$BIN_DIR/waffle-installer" \
          "$APP_DIR/waffle-installer.desktop" "$ICON_DIR/waffle-fuse.svg"
    rm -rf "$PREFIX"
    refresh_desktop
    echo "waffle-fuse removed. Launchers and autostart in the users' home directories:"
    echo "run 'waffle-installer' and choose Uninstall before removing, or delete them by hand."
    exit 0
fi

if [ ! -x "$HERE/waffle-fuse/waffle-fuse" ]; then
    echo "waffle-fuse/ not found next to $0" >&2
    exit 1
fi

# Runtime requirements the binary and scripts cannot bring along
for tool in fusermount3 python3 setfacl; do
    command -v "$tool" >/dev/null 2>&1 || echo "Warning: $tool not found (packages: fuse3, python3, acl)" >&2
done

rm -rf "$PREFIX.new"
cp -R "$HERE/waffle-fuse" "$PREFIX.new"
chown -R root:root "$PREFIX.new"
chmod -R u=rwX,go=rX "$PREFIX.new"
install -m 755 "$HERE/install.sh" "$PREFIX.new/install.sh"      # to uninstall later
chmod 755 "$PREFIX.new/waffle-fuse" "$PREFIX.new"/scripts/*.py "$PREFIX.new"/scripts/*.sh
rm -rf "$PREFIX"
mv "$PREFIX.new" "$PREFIX"

mkdir -p "$BIN_DIR" "$APP_DIR" "$ICON_DIR"
ln -sf "$PREFIX/waffle-fuse" "$BIN_DIR/waffle-fuse"
ln -sf "$PREFIX/scripts/waffle-installer.py" "$BIN_DIR/waffle-installer"
install -m 644 "$HERE/waffle-installer.desktop" "$APP_DIR/waffle-installer.desktop"
install -m 644 "$PREFIX/icons/amiga-floppy.svg" "$ICON_DIR/waffle-fuse.svg"
refresh_desktop
echo "Installed $PREFIX"

# System setup for the user who ran sudo/pkexec (mount directory, serial port group)
WAFFLE_USER="${SUDO_USER:-}"
if [ -z "$WAFFLE_USER" ] && [ -n "${PKEXEC_UID:-}" ]; then
    WAFFLE_USER="$(id -nu "$PKEXEC_UID" 2>/dev/null || true)"
fi
if [ -n "$WAFFLE_USER" ] && [ "$WAFFLE_USER" != root ]; then
    "$PREFIX/scripts/install-system.sh" "$WAFFLE_USER" && rc=0 || rc=$?
    [ "$rc" -eq 10 ] && echo "$WAFFLE_USER was added to the serial port group: log out and in again"
    [ "$rc" -ne 0 ] && [ "$rc" -ne 10 ] && echo "System setup failed: run 'waffle-installer' to retry" >&2
fi

echo ""
echo "Done: this directory can now be deleted."
echo "Each user: start \"Waffle Installer\" from the menu (or run waffle-installer)."
echo "To remove waffle-fuse: $PREFIX/install.sh --uninstall"
