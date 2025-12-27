# Stage 1: Build environment
FROM ubuntu:20.04 AS builder

# Docker containers run as root by default, which is suitable for this build process.
# Set environment variables for non-interactive installation
ENV DEBIAN_FRONTEND=noninteractive

# Update apt and install build tools and Qt 5.12
RUN apt update && apt upgrade -y && \
    apt install -y build-essential git cmake libgl1-mesa-dev qt5-default qttools5-dev-tools libfuse2 curl xz-utils && \
    rm -rf /var/lib/apt/lists/*

# Download and extract linuxdeployqt
RUN curl -L https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage -o /tmp/linuxdeployqt.AppImage && \
    chmod a+x /tmp/linuxdeployqt.AppImage && \
    /tmp/linuxdeployqt.AppImage --appimage-extract && \
    mv squashfs-root /usr/local/linuxdeployqt_extracted && \
    rm /tmp/linuxdeployqt.AppImage

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
# Run linuxdeployqt on the executable, letting it create the AppDir
RUN /usr/local/linuxdeployqt_extracted/AppRun /app/waffleCopyPRO-Universal/waffleCopyPRO-Universal

# Find the created AppDir (e.g., waffleCopyPRO-Universal.AppDir)
# and copy fonts and WaffleUI into it
RUN APPLDIR=$(find . -maxdepth 1 -type d -name "*.AppDir" | head -n 1) && \
    if [ -z "$APPLDIR" ]; then echo "AppDir not found!"; exit 1; fi && \
    cp -r ./fonts "$APPLDIR/" && \
    cp -r ./WaffleUI "$APPLDIR/"

# Convert AppDir to AppImage using appimagetool
RUN APPLDIR=$(find . -maxdepth 1 -type d -name "*.AppDir" | head -n 1) && \
    if [ -z "$APPLDIR" ]; then echo "AppDir not found!"; exit 1; fi && \
    /usr/local/appimagetool_extracted/AppRun "$APPLDIR"

RUN mkdir -p /app/release
RUN mv *.AppImage /app/release/
