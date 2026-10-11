# Waffle-Fuse AppImage Build

This directory contains the configuration and scripts to build a self-contained AppImage for waffle-fuse that runs on Ubuntu 20.04 and later.

## What is an AppImage?

An AppImage is a portable Linux application format that bundles an application with its dependencies into a single executable file. This means you can run waffle-fuse on any Linux distribution without installing packages or worrying about missing dependencies.

## Building the AppImage

### Automatic (GitHub Actions - Recommended)

The AppImage is automatically built when:
1. A tag matching `waffle-fuse-v*` is pushed (e.g., `waffle-fuse-v1.0.0`)
2. Changes are pushed to the master branch (creates development AppImage)
3. Manually triggered via GitHub Actions workflow

The built AppImage is available in:
- **Release downloads** (for tagged versions)
- **Actions artifacts** (for development builds)

### Manual Build

To build the AppImage locally:

```bash
cd waffle-fuse
./.AppImage/build-appimage.sh
```

Requirements:
- Ubuntu 20.04 or later (or Debian-based distribution)
- Build tools: `build-essential pkg-config libfuse3-dev fuse3`
- Python 3 with GTK support: `python3 python3-gi gir1.2-gtk-3.0`

The AppImage will be created in `dist/Waffle-Fuse-<version>-x86_64.AppImage`

## Using the AppImage

### Launch the Installer

Simply run the AppImage - it automatically starts the graphical installer:

```bash
chmod +x Waffle-Fuse-*.AppImage
./Waffle-Fuse-*.AppImage
```

This executes the waffle-installer with all dependencies bundled, without requiring any system-wide installation.

### What the Installer Does

The installer (launched by the AppImage) will:

1. **Check dependencies** – verify all required packages are installed
2. **Build waffle-fuse** – compile the driver from source if needed
3. **Install for the user** – create menu launchers and icons, start the tray daemon
4. **Install system components** – optionally ask for admin privileges to:
   - Configure mount directories (`/media/$USER` or `/run/media/$USER`)
   - Grant serial port access (add user to `dialout`/`uucp` group)

### Advanced: Run AppImage without Installer

If you want to run the AppImage without the installer (e.g., to just extract files):

```bash
# Mount the AppImage as a loop device
./Waffle-Fuse-*.AppImage --appimage-mount

# Or extract it
./Waffle-Fuse-*.AppImage --appimage-extract

# Access the binaries and scripts in:
# ./squashfs-root/opt/waffle-fuse/
```

## Files in this Directory

- **AppRun** – Wrapper script that runs the installer when AppImage is launched
- **waffle-installer.desktop** – Desktop entry for the installer
- **build-appimage.sh** – Script that builds the AppImage locally

## AppImage Contents

The AppImage includes:
- `waffle-fuse` binary (compiled for x86_64)
- All Python scripts: installer, daemon, image opener
- All desktop files and icons
- Required system libraries for Ubuntu 20.04+
- All necessary dependencies

## Compatibility

- **Ubuntu 20.04 LTS and later**
- **Debian 11+** (and other Debian-based distributions)
- **Other Linux distributions** (may have dependency issues; please report)

## Troubleshooting

### "Transport endpoint is not connected" error

If you see this error when mounting the AppImage, extract it first:

```bash
./Waffle-Fuse-*.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

### Installer crashes on startup

The AppImage bundles Python and GTK, but your system may have incompatible versions. Try:

```bash
# Run in text mode
./Waffle-Fuse-*.AppImage --text
```

### Permission denied when running AppImage

Make sure it's executable:

```bash
chmod +x Waffle-Fuse-*.AppImage
```

## GitHub Actions Workflow

The workflow is defined in `.github/workflows/waffle-appimage.yml` and:

1. Triggers on tags matching `waffle-fuse-v*`
2. Builds on Ubuntu 20.04 in a clean environment
3. Uploads to GitHub Releases (for tags)
4. Saves as workflow artifact (all builds)

### To create a release:

```bash
git tag waffle-fuse-v1.0.0
git push origin waffle-fuse-v1.0.0
```

The GitHub Actions workflow will automatically:
1. Build the AppImage
2. Create a GitHub Release
3. Upload the AppImage as a release asset
