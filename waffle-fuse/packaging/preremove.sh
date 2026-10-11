#!/bin/sh
# Package pre-remove (deb, rpm, Arch): runs as root.
# No "exit": on Arch this body is pasted into a function of the .INSTALL file.
#   deb: $1 = remove | purge | upgrade | failed-upgrade | deconfigure
#   rpm: $1 = 0 (erase) | 1 (upgrade)
#   Arch: pre_remove only runs on removal, $1 = version

case "${1:-}" in
    upgrade|failed-upgrade|deconfigure|1)
        ;;      # upgrade: keep the system setup
    *)
        /opt/waffle-fuse/scripts/install-system.sh --uninstall || true
        ;;
esac
