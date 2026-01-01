# Stage 1: Build environment
FROM ubuntu:20.04 AS builder

# Docker containers run as root by default, which is suitable for this build process.
# Set environment variables for non-interactive installation
ENV DEBIAN_FRONTEND=noninteractive

# Update apt and install build tools and Qt 5.12
RUN apt clean && apt update && apt upgrade -y
RUN apt-cache search portaudio
RUN apt install -y build-essential git cmake libgl1-mesa-dev qt5-default qttools5-dev-tools libqt5serialport5-dev libfuse2 curl xz-utils libopenmpt-dev portaudio19-dev libsdl2-dev libpulse-dev && \
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
RUN cp waffleCopyPRO-Universal AppDir/usr/share/waffleCopyPRO-Universal/

# Copy fonts and WaffleUI into the AppDir
RUN cp -r ./fonts AppDir/usr/share/waffleCopyPRO-Universal/
RUN cp -r ./WaffleUI AppDir/usr/share/waffleCopyPRO-Universal/

# Manually collect dependencies using ldd
# This is a simplified version and might need iteration for all dependencies
RUN echo "Invalidating cache for library collection" && \
    executable="AppDir/usr/share/waffleCopyPRO-Universal/waffleCopyPRO-Universal" && \
    echo "ldd output for $executable:" && \
    ldd_output=$(ldd "$executable") && \
    echo "$ldd_output" && \
    libs=$(echo "$ldd_output" | awk '{print $1, $3}' | sed 's/ =>//' | grep -v "linux-vdso" | grep -v "ld-linux" | sort -u | xargs -n1 realpath 2>/dev/null) && \
    echo "Libraries to copy: $libs" && \
    for lib in $libs; do \
        if [ -f "$lib" ]; then \
            mkdir -p AppDir/$(dirname "$lib") && \
            cp "$lib" AppDir/$(dirname "$lib")/ || true; \
        fi \
    done && \
    echo "Contents of AppDir/usr/lib after copy:" && \
    ls -l AppDir/usr/lib

# Create a basic .desktop file for the AppImage
RUN echo "[Desktop Entry]\nName=WaffleCopyPRO-Universal\nExec=waffleCopyPRO-Universal\nIcon=waffleCopyPRO-Universal-icon\nType=Application\nCategories=Utility;\n" > AppDir/waffleCopyPRO-Universal.desktop

# Create the AppRun script
RUN echo '#!/bin/bash\nHERE="$(dirname "$(readlink -f "${0}")")"\nexport LD_LIBRARY_PATH="$HERE"/usr/lib:"$HERE"/lib64:"$LD_LIBRARY_PATH"\nexport QT_QPA_PLATFORM_PLUGIN_PATH="$HERE"/usr/lib/qt5/plugins\nexport QT_QPA_FONTDIR="$HERE"/usr/share/waffleCopyPRO-Universal/fonts\ncd "$HERE"/usr/share/waffleCopyPRO-Universal/\nexec "./waffleCopyPRO-Universal" "$@"' > AppDir/AppRun && \
    chmod +x AppDir/AppRun

# Copy the dynamic linker
RUN mkdir -p AppDir/lib64 && \
    cp /lib64/ld-linux-x86-64.so.2 AppDir/lib64/

# Copy the existing icon for the .desktop file and .DirIcon
RUN cp ./WaffleUI/waffleCopyPRO-icon.png AppDir/waffleCopyPRO-Universal-icon.png
RUN cp ./WaffleUI/waffleCopyPRO-icon.png AppDir/.DirIcon

# Create directories for Qt plugins
RUN mkdir -p AppDir/usr/lib/qt5/plugins/platforms && \
    mkdir -p AppDir/usr/lib/qt5/plugins/imageformats

# Copy essential Qt plugins
RUN cp /usr/lib/x86_64-linux-gnu/qt5/plugins/platforms/libqxcb.so AppDir/usr/lib/qt5/plugins/platforms/ && \
    cp /usr/lib/x86_64-linux-gnu/qt5/plugins/imageformats/libqico.so AppDir/usr/lib/qt5/plugins/imageformats/ && \
    cp /usr/lib/x86_64-linux-gnu/qt5/plugins/imageformats/libqjpeg.so AppDir/usr/lib/qt5/plugins/imageformats/

RUN ls -l AppDir # Debug: List files in AppDir before appimagetool

# Create directories for Qt plugins
RUN mkdir -p AppDir/usr/lib/qt5/plugins/platforms && \
    mkdir -p AppDir/usr/lib/qt5/plugins/imageformats

# Debug: List system Qt plugin directories to identify plugins
RUN ls -l /usr/lib/x86_64-linux-gnu/qt5/plugins/platforms/
RUN ls -l /usr/lib/x86_64-linux-gnu/qt5/plugins/imageformats/
# Convert AppDir to AppImage using appimagetool
RUN /usr/local/appimagetool_extracted/AppRun AppDir

