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

mkdir -p "$ICON_DST" "$APP_DST"

# Install icons
cp "$WAFFLE_DIR/icons/amiga-floppy.svg" "$ICON_DST/"
cp "$WAFFLE_DIR/icons/dos-floppy.svg"   "$ICON_DST/"

echo "Icons installed to $ICON_DST"

# Install .desktop files with correct paths substituted
for desktop in "$WAFFLE_DIR"/waffle-*.desktop; do
    name="$(basename "$desktop")"
    sed \
        -e "s|ICON_PATH_PLACEHOLDER|$ICON_DST|g" \
        -e "s|Exec=sh -c 'SCRIPT_DIR=\$(dirname \$(readlink -f %k)); \"\$SCRIPT_DIR/scripts/waffle-mount.sh\"|Exec=$WAFFLE_DIR/scripts/waffle-mount.sh|g" \
        "$desktop" > "$APP_DST/$name"
    # Fix Exec line properly: replace the whole sh -c trick with direct path
    sed -i "s|Exec=$WAFFLE_DIR/scripts/waffle-mount.sh|Exec=$WAFFLE_DIR/scripts/waffle-mount.sh|g" "$APP_DST/$name"
    echo "Installed $APP_DST/$name"
done

# Simpler approach: regenerate Exec lines directly
for desktop in "$WAFFLE_DIR"/waffle-*.desktop; do
    name="$(basename "$desktop")"
    dst="$APP_DST/$name"

    # Determine args from filename
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

# Refresh desktop database
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APP_DST" 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$HOME/.local/share/icons" 2>/dev/null || true
fi

echo ""
echo "Done. You can now find the launchers in your application menu"
echo "under 'System' or search for 'waffle' / 'Amiga' / 'DOS'."
echo ""
echo "To also place them on the desktop:"
echo "  cp $APP_DST/waffle-*.desktop ~/Desktop/"
echo "  chmod +x ~/Desktop/waffle-*.desktop"
