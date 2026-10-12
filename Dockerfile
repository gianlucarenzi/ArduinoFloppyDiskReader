# Stage 1: Build environment
# Debian 11: Qt 6.4.2 from bullseye-backports, and a glibc (2.31) old enough
# for the AppImage to run on most current distributions
FROM debian:bullseye AS builder

# Set environment variables for non-interactive installation
ENV DEBIAN_FRONTEND=noninteractive

# Debian 11 (and its backports) has moved to archive.debian.org: use it only,
# deb.debian.org still lists packages that are no longer there.
# Update apt and install build tools and Qt6 from bullseye-backports;
# qt6-qpa-plugins has the xcb platform plugin that linuxdeploy-plugin-qt puts
# into the AppImage
RUN rm -f /etc/apt/sources.list.d/* && \
    printf '%s\n' \
        'deb http://archive.debian.org/debian bullseye main' \
        'deb http://archive.debian.org/debian-security bullseye-security main' \
        'deb http://archive.debian.org/debian bullseye-backports main' > /etc/apt/sources.list && \
    apt-get update && \
    apt-get install -y build-essential git cmake patchelf libmikmod-dev curl xz-utils file && \
    apt-get install -y -t bullseye-backports qmake6 qt6-base-dev qt6-base-dev-tools \
        qt6-serialport-dev qt6-qpa-plugins

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
RUN qmake6 waffleCopyPRO-Universal.pro
RUN make clean
RUN make -j$(nproc)

# Create a basic .desktop file for the AppImage
RUN echo "[Desktop Entry]\nName=WaffleCopyPRO-Universal\nExec=waffleCopyPRO-Universal\nIcon=waffleCopyPRO-Universal\nType=Application\nCategories=Utility;\n" > waffleCopyPRO-Universal.desktop
RUN cp ./WaffleUI/waffleCopyPRO-icon.png ./waffleCopyPRO-Universal.png

# Deploy and Package into AppImage (linuxdeploy-plugin-qt finds Qt6 through QMAKE)
ENV QMAKE=/usr/bin/qmake6
RUN linuxdeploy --appdir AppDir --executable ./waffleCopyPRO-Universal --desktop-file ./waffleCopyPRO-Universal.desktop --icon-file ./waffleCopyPRO-Universal.png --plugin qt

# Manually copy resources
RUN cp -r ./fonts AppDir/usr/bin/
RUN cp -r ./WaffleUI AppDir/usr/bin/
RUN cp -r ./translations AppDir/usr/bin/

# Create custom AppRun
# (linuxdeploy makes AppRun a symlink to the executable: remove it, or the
# script below would overwrite the executable itself)
RUN rm -f AppDir/AppRun && \
    echo '#!/bin/bash' > AppDir/AppRun && \
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
