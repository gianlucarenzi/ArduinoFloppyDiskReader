# Stage 1: Build environment
FROM ubuntu:20.04 AS builder

# Docker containers run as root by default, which is suitable for this build process.
# Set environment variables for non-interactive installation
ENV DEBIAN_FRONTEND=noninteractive

# Update apt and install build tools and Qt 5.12
RUN apt update && apt upgrade -y && \
    apt install -y build-essential git cmake libgl1-mesa-dev qt5-default qttools5-dev-tools libfuse2 curl xz-utils imagemagick && \
    rm -rf /var/lib/apt/lists/*

# Download and extract appimagetool
RUN curl -L https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage -o /tmp/appimagetool.AppImage && \
    chmod a+x /tmp/appimagetool.AppImage && \
    /tmp/appimagetool.AppImage --appimage-extract && \
    mv squashfs-root /usr/local/appimagetool_extracted && \
    rm /tmp/appimagetool.AppImage

# Copy the project source
COPY . /app
WORKDIR /app/waffleCopyPRO-Universal

# Build the project
RUN qmake waffleCopyPRO-Universal.pro
RUN make clean
RUN make -j$(nproc)

# Deploy and Package into AppImage
# The executable name will be waffleCopyPRO-Universal
# Assuming it's in the current directory after make

# Deploy and Package into AppImage
# Deploy and Package into AppImage
# Create the AppDir structure
RUN mkdir -p AppDir/usr/bin
RUN mkdir -p AppDir/usr/lib
RUN mkdir -p AppDir/usr/share/waffleCopyPRO-Universal

# Copy the executable into the AppDir
RUN cp waffleCopyPRO-Universal AppDir/usr/bin/

# Copy fonts and WaffleUI into the AppDir
RUN cp -r ./fonts AppDir/usr/share/waffleCopyPRO-Universal/
RUN cp -r ./WaffleUI AppDir/usr/share/waffleCopyPRO-Universal/

# Manually collect dependencies using ldd
# This is a simplified version and might need iteration for all dependencies
RUN executable="AppDir/usr/bin/waffleCopyPRO-Universal" && \
    libs=$(ldd "$executable" | grep "=>" | awk '{print $3}' | xargs -I {} find /usr/lib /lib -name "$(basename {})" 2>/dev/null | sort -u) && \
    for lib in $libs; do cp "$lib" AppDir/usr/lib/; done

# Create a basic .desktop file for the AppImage
RUN echo "[Desktop Entry]\n\
Name=WaffleCopyPRO-Universal\n\
Exec=waffleCopyPRO-Universal\n\
Icon=waffleCopyPRO-Universal-icon\n\
Type=Application\n\
Categories=Utility;\n\
" > AppDir/waffleCopyPRO-Universal.desktop

# Create circular icon with transparent background
RUN convert ./WaffleUI/waffleCopyPRO.png -alpha set -virtual-pixel transparent -channel A -morphology erode disk:5 -trim +repage -background none -gravity center -extent 256x256 ./waffleCopyPRO-icon.png

# Copy the processed icon for the .desktop file and .DirIcon
RUN cp ./waffleCopyPRO-icon.png AppDir/waffleCopyPRO-Universal-icon.png
RUN cp ./waffleCopyPRO-icon.png AppDir/.DirIcon

# Convert AppDir to AppImage using appimagetool
RUN /usr/local/appimagetool_extracted/AppRun AppDir

RUN mkdir -p /app/release
RUN mv *.AppImage /app/release/
