#!/bin/bash
# Build script for waffle-fuse AppImage on Ubuntu 20.04+
# This script packages waffle-fuse with all dependencies into a self-contained AppImage

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-appimage"
APPDIR="${BUILD_DIR}/waffle-fuse.AppDir"
OUTPUT_DIR="${SCRIPT_DIR}/dist"

echo "=== waffle-fuse AppImage Builder ==="
echo "Build directory: $BUILD_DIR"

# Clean previous builds
rm -rf "$BUILD_DIR" "$OUTPUT_DIR"
mkdir -p "$BUILD_DIR" "$OUTPUT_DIR" "$APPDIR/opt/waffle-fuse"

cd "$SCRIPT_DIR"

# 1. Build waffle-fuse
echo ""
echo "--- Building waffle-fuse ---"
make clean
make

# 2. Create AppDir structure
echo ""
echo "--- Creating AppDir structure ---"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib/x86_64-linux-gnu"
mkdir -p "$APPDIR/opt/waffle-fuse/scripts"
mkdir -p "$APPDIR/opt/waffle-fuse/icons"

# 3. Copy waffle-fuse binary
echo "Copying waffle-fuse binary..."
cp waffle-fuse "$APPDIR/opt/waffle-fuse/"
chmod +x "$APPDIR/opt/waffle-fuse/waffle-fuse"

# 4. Copy scripts and installer
echo "Copying scripts and installer..."
cp scripts/waffle-installer.py "$APPDIR/opt/waffle-fuse/scripts/"
cp scripts/waffle-diskflashback.py "$APPDIR/opt/waffle-fuse/scripts/"
cp scripts/install-system.sh "$APPDIR/opt/waffle-fuse/scripts/"
cp scripts/install-desktop.sh "$APPDIR/opt/waffle-fuse/scripts/"
cp scripts/waffle-mount.sh "$APPDIR/opt/waffle-fuse/scripts/"
cp scripts/waffle-open-image.py "$APPDIR/opt/waffle-fuse/scripts/"
chmod +x "$APPDIR/opt/waffle-fuse/scripts/"*.py "$APPDIR/opt/waffle-fuse/scripts/"*.sh

# 5. Copy icons and desktop files
echo "Copying icons and desktop files..."
cp icons/amiga-floppy.svg "$APPDIR/opt/waffle-fuse/icons/"
cp icons/dos-floppy.svg "$APPDIR/opt/waffle-fuse/icons/"
cp waffle-installer.desktop "$APPDIR/opt/waffle-fuse/"
cp waffle-diskflashback.desktop "$APPDIR/opt/waffle-fuse/"

# 6. Copy wrapper AppRun and desktop entry
echo "Copying AppImage configuration..."
cp .AppImage/AppRun "$APPDIR/"
cp .AppImage/waffle-installer.desktop "$APPDIR/waffle-installer.desktop"
chmod +x "$APPDIR/AppRun"

# 7. Create a symlink for the installer in usr/bin for easier access
ln -s ../opt/waffle-fuse/scripts/waffle-installer.py "$APPDIR/usr/bin/waffle-installer"

# 8. Copy required system libraries (linuxdeploy will handle most, but we need some runtime libs)
echo "Copying runtime libraries..."
LIBS_TO_COPY=(
    "libfuse3.so.3"
    "libpthread.so.0"
    "libc.so.6"
    "libm.so.6"
    "libdl.so.2"
    "libgcc_s.so.1"
    "libstdc++.so.6"
)

for lib in "${LIBS_TO_COPY[@]}"; do
    LIB_PATH=$(ldconfig -p | grep -E "/$lib" | head -1 | awk '{print $NF}')
    if [ -n "$LIB_PATH" ] && [ -f "$LIB_PATH" ]; then
        echo "Copying $lib..."
        cp "$LIB_PATH" "$APPDIR/usr/lib/x86_64-linux-gnu/" || true
    fi
done

# 9. Create AppImage using appimagetool
echo ""
echo "--- Creating AppImage ---"

if ! command -v appimagetool &> /dev/null; then
    echo "appimagetool not found. Downloading..."
    wget -q https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage -O /tmp/appimagetool
    chmod +x /tmp/appimagetool
    APPIMAGETOOL=/tmp/appimagetool
else
    APPIMAGETOOL=$(which appimagetool)
fi

# Get version from git if available
VERSION=$(git describe --tags --always 2>/dev/null || echo "latest")
VERSION=${VERSION#v}  # Remove 'v' prefix if present

OUTPUT_FILE="$OUTPUT_DIR/Waffle-Fuse-${VERSION}-x86_64.AppImage"

"$APPIMAGETOOL" -n "$APPDIR" "$OUTPUT_FILE"
chmod +x "$OUTPUT_FILE"

echo ""
echo "=== AppImage created successfully ==="
echo "Output: $OUTPUT_FILE"
echo ""
echo "To use it:"
echo "  chmod +x $OUTPUT_FILE"
echo "  ./$OUTPUT_FILE"
