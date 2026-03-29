#!/bin/sh
# install-desktop.sh – install waffle-fuse desktop launchers
#
# Copies icons to ~/.local/share/icons and .desktop files to
# ~/.local/share/applications, substituting the correct icon path.
# Run once after building waffle-fuse.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WAFFLE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

ICON_DST="$HOME/.local/share/icons/waffle-fuse"
APP_DST="$HOME/.local/share/applications"

AUTOSTART_DST="$HOME/.config/autostart"
mkdir -p "$ICON_DST" "$APP_DST" "$AUTOSTART_DST"

# Install icons (launcher + tray)
for icon in amiga-floppy.svg dos-floppy.svg \
            tray-idle.svg tray-mounting.svg tray-mounted.svg; do
    cp "$WAFFLE_DIR/icons/$icon" "$ICON_DST/"
done
echo "Icons installed to $ICON_DST"

# Install per-format launcher .desktop files
for desktop in "$WAFFLE_DIR"/waffle-amiga-dd.desktop \
               "$WAFFLE_DIR"/waffle-amiga-hd.desktop \
               "$WAFFLE_DIR"/waffle-dos-dd.desktop \
               "$WAFFLE_DIR"/waffle-dos-hd.desktop; do
    [ -f "$desktop" ] || continue
    name="$(basename "$desktop")"
    dst="$APP_DST/$name"
    case "$name" in
        waffle-amiga-dd*) ARGS="amiga dd" ;;
        waffle-amiga-hd*) ARGS="amiga hd" ;;
        waffle-dos-dd*)   ARGS="dos dd"   ;;
        waffle-dos-hd*)   ARGS="dos hd"   ;;
        *)                ARGS=""         ;;
    esac
    sed \
        -e "s|ICON_PATH_PLACEHOLDER|$ICON_DST|g" \
        -e "s|Exec=.*|Exec=$WAFFLE_DIR/scripts/waffle-mount.sh $ARGS|g" \
        "$desktop" > "$dst"
    echo "Installed $dst"
done

# Install DiskFlashback daemon launcher
DAEMON_DST="$APP_DST/waffle-diskflashback.desktop"
chmod +x "$WAFFLE_DIR/scripts/waffle-diskflashback.py"
sed \
    -e "s|ICON_PATH_PLACEHOLDER|$ICON_DST|g" \
    -e "s|WAFFLE_DIR_PLACEHOLDER|$WAFFLE_DIR|g" \
    "$WAFFLE_DIR/waffle-diskflashback.desktop" > "$DAEMON_DST"
echo "Installed $DAEMON_DST"

# Autostart entry – start DiskFlashback at login
cp "$DAEMON_DST" "$AUTOSTART_DST/waffle-diskflashback.desktop"
echo "Autostart entry installed to $AUTOSTART_DST/waffle-diskflashback.desktop"

# Refresh desktop database
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APP_DST" 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$HOME/.local/share/icons" 2>/dev/null || true
fi

echo ""
echo "Done. Launchers are in the application menu under 'System'."
echo "  - Per-format launchers: waffle-amiga-dd/hd, waffle-dos-dd/hd"
echo "  - Auto-detect daemon:   Waffle DiskFlashback (also set to autostart)"
echo ""
echo "To place launchers on the Desktop:"
echo "  cp $APP_DST/waffle-*.desktop ~/Desktop/"
echo "  chmod +x ~/Desktop/waffle-*.desktop"
echo ""
echo "To start the daemon now:"
echo "  $WAFFLE_DIR/scripts/waffle-diskflashback.py &"
