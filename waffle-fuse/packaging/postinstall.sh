#!/bin/sh
# Package post-install (deb, rpm, Arch): runs as root.
# No "exit": on Arch this body is pasted into a function of the .INSTALL file.

# System setup (mount directory, serial port group) for the user who ran the
# package manager through sudo or pkexec; otherwise waffle-installer does it.
WAFFLE_USER="${SUDO_USER:-}"
if [ -z "$WAFFLE_USER" ] && [ -n "${PKEXEC_UID:-}" ]; then
    WAFFLE_USER="$(id -nu "$PKEXEC_UID" 2>/dev/null || true)"
fi
if [ -n "$WAFFLE_USER" ] && [ "$WAFFLE_USER" != root ]; then
    /opt/waffle-fuse/scripts/install-system.sh "$WAFFLE_USER" && rc=0 || rc=$?
    if [ "$rc" -eq 10 ]; then
        echo "waffle-fuse: $WAFFLE_USER was added to the serial port group: log out and in again"
    elif [ "$rc" -ne 0 ]; then
        echo "waffle-fuse: system setup failed, run 'waffle-installer' to retry" >&2
    fi
fi

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q -t /usr/share/icons/hicolor || true
fi

echo "waffle-fuse installed. Each user: start \"Waffle Installer\" from the menu"
echo "(or run waffle-installer) to add the launchers and the tray daemon."
