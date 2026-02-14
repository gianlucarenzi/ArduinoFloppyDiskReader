# Stage 1: Build environment
FROM ubuntu:20.04 AS builder

# Set environment variables for non-interactive installation
ENV DEBIAN_FRONTEND=noninteractive

# Update apt and install build tools and Qt
RUN apt-get update && \
    apt-get install -y build-essential git cmake qt5-default qttools5-dev-tools libqt5serialport5-dev patchelf libmikmod-dev curl xz-utils

# Download and extract appimagetool
RUN curl -L https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage -o /tmp/appimagetool.AppImage && \
    chmod +x /tmp/appimagetool.AppImage && \
    /tmp/appimagetool.AppImage --appimage-extract && \
    mv squashfs-root /usr/local/appimagetool_extracted && \
    rm /tmp/appimagetool.AppImage

# Download and extract linuxdeploy and plugins
RUN curl -L https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage -o /tmp/linuxdeploy.AppImage && \
    chmod +x /tmp/linuxdeploy.AppImage && \
    cd /tmp && ./linuxdeploy.AppImage --appimage-extract && \
    cp /tmp/squashfs-root/usr/bin/linuxdeploy /usr/local/bin/linuxdeploy && \
    rm -rf /tmp/squashfs-root && \
    rm /tmp/linuxdeploy.AppImage

RUN curl -L https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage -o /tmp/linuxdeploy-plugin-qt.AppImage && \
    chmod +x /tmp/linuxdeploy-plugin-qt.AppImage && \
    cd /tmp && ./linuxdeploy-plugin-qt.AppImage --appimage-extract && \
    cp /tmp/squashfs-root/usr/bin/linuxdeploy-plugin-qt /usr/local/bin/linuxdeploy-plugin-qt && \
    rm -rf /tmp/squashfs-root && \
    rm /tmp/linuxdeploy-plugin-qt.AppImage

# Copy the project source
COPY . /app
WORKDIR /app/waffleCopyPRO-Universal

# Build the project
RUN qmake waffleCopyPRO-Universal.pro
RUN make clean
RUN make -j$(nproc)

# Create a basic .desktop file for the AppImage
RUN echo "[Desktop Entry]\nName=WaffleCopyPRO-Universal\nExec=waffleCopyPRO-Universal\nIcon=waffleCopyPRO-Universal\nType=Application\nCategories=Utility;\n" > waffleCopyPRO-Universal.desktop
RUN cp ./WaffleUI/waffleCopyPRO-icon.png ./waffleCopyPRO-Universal.png

# Deploy and Package into AppImage
RUN linuxdeploy --appdir AppDir --executable ./waffleCopyPRO-Universal --desktop-file ./waffleCopyPRO-Universal.desktop --icon-file ./waffleCopyPRO-Universal.png --plugin qt

# Manually copy resources
RUN cp -r ./fonts AppDir/usr/bin/
RUN cp -r ./WaffleUI AppDir/usr/bin/
RUN cp -r ./translations AppDir/usr/bin/

# Create custom AppRun
RUN echo '#!/bin/bash' > AppDir/AppRun && \
    echo 'HERE="$(dirname "$(readlink -f "${0}")")"' >> AppDir/AppRun && \
    echo 'cd "$HERE/usr/bin" || exit 1' >> AppDir/AppRun && \
    echo 'exec "./waffleCopyPRO-Universal" "$@"' >> AppDir/AppRun && \
    chmod +x AppDir/AppRun

# Bundle CAPS libraries from repo into the AppImage and set rpath for the executable
RUN mkdir -p AppDir/usr/lib && \
    cp -v /app/lib/capsapi/*.so* AppDir/usr/lib/ || true && \
    if [ -f AppDir/usr/bin/waffleCopyPRO-Universal ]; then \
      patchelf --set-rpath '$ORIGIN/../lib' AppDir/usr/bin/waffleCopyPRO-Universal || true; \
    fi

# Convert AppDir to AppImage using appimagetool
RUN /usr/local/appimagetool_extracted/AppRun AppDir
