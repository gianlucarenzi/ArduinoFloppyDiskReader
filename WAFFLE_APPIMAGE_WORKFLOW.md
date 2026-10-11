# GitHub Actions Workflow for waffle-fuse AppImage

## Overview

The GitHub Actions workflow (`.github/workflows/waffle-fuse-appimage.yml`) automatically builds 
and distributes waffle-fuse as an AppImage for Ubuntu 20.04+.

## What it does

1. **Compiles waffle-fuse** on Ubuntu 20.04 (the oldest supported LTS)
2. **Creates an AppImage** – a self-contained, portable executable
3. **Bundles dependencies** – no need to install packages on target systems
4. **Uploads artifacts** – to GitHub Releases (for tagged versions) and workflow artifacts
5. **Launches installer** – when the AppImage is run, it automatically starts the graphical installer

## Triggering the workflow

### Automatic (recommended)

The workflow runs automatically on:

#### 1. **Tag pushes** (creates a release)
```bash
git tag waffle-fuse-v1.0.0
git push origin waffle-fuse-v1.0.0
```

This:
- Builds the AppImage on Ubuntu 20.04
- Creates a GitHub Release
- Uploads `Waffle-Fuse-1.0.0-x86_64.AppImage` as a release asset
- Release is automatically published with generated release notes

#### 2. **Master branch pushes** (development builds)
```bash
git push origin master
```

Changes to `waffle-fuse/**` or `.github/workflows/waffle-fuse-appimage.yml` trigger an automatic build.
The AppImage is available as a workflow artifact (download from Actions tab).

#### 3. **Pull requests** (testing)
Any pull request that modifies `waffle-fuse/**` triggers a build to verify compatibility.

### Manual trigger

Run the workflow manually from the GitHub Actions tab:

1. Go to Actions → "Build waffle-fuse AppImage"
2. Click "Run workflow"
3. Select branch (usually `master`)
4. Click "Run workflow" button
5. Wait for completion and download the AppImage from artifacts

## Accessing the AppImage

### For releases (production)

```bash
# Download from GitHub Releases
https://github.com/gianlucarenzi/ArduinoFloppyDiskReader/releases

# Find "Waffle-Fuse-*.AppImage" under the release assets
chmod +x Waffle-Fuse-*.AppImage
./Waffle-Fuse-*.AppImage
```

### For development builds

1. Go to: https://github.com/gianlucarenzi/ArduinoFloppyDiskReader/actions/workflows/waffle-fuse-appimage.yml
2. Click the latest successful workflow run
3. Scroll down to "Artifacts"
4. Download "waffle-fuse-appimage-ubuntu2004"
5. Extract and run:
   ```bash
   unzip waffle-fuse-appimage-ubuntu2004.zip
   chmod +x Waffle-Fuse-*.AppImage
   ./Waffle-Fuse-*.AppImage
   ```

## Workflow configuration

The workflow is configured in `.github/workflows/waffle-fuse-appimage.yml`:

```yaml
on:
  push:
    tags:
      - 'waffle-fuse-v*'          # Trigger on version tags
    branches:
      - master                     # Trigger on master pushes
    paths:
      - 'waffle-fuse/**'          # Only if waffle-fuse changes
  pull_request:
    branches:
      - master
  workflow_dispatch:              # Manual trigger
```

## Release process

To create a new release:

```bash
# 1. Ensure all changes are committed
git status  # should be clean

# 2. Create and push a tag
git tag waffle-fuse-v1.0.0
git push origin waffle-fuse-v1.0.0

# 3. Wait for GitHub Actions to complete
# (takes ~3-5 minutes)

# 4. Check GitHub Releases
# https://github.com/gianlucarenzi/ArduinoFloppyDiskReader/releases

# 5. AppImage is available as a release asset
```

## Build environment

The workflow builds on:
- **Ubuntu 20.04 LTS** (oldest supported, ensures maximum compatibility)
- **Python 3**, **libfuse3**, GTK 3 with all dependencies pre-installed
- **appimagetool** – downloaded automatically to create the AppImage

## AppImage compatibility

The AppImage works on:
- ✅ Ubuntu 20.04 LTS, 22.04 LTS, 24.04 LTS
- ✅ Debian 11, 12 (testing)
- ✅ Arch Linux (likely, but not tested)
- ✅ Fedora (likely, but not tested)
- ✅ Other Debian-based distributions

If you test it on other distributions, please report results.

## Troubleshooting

### Workflow fails to build

Check the workflow logs at:
https://github.com/gianlucarenzi/ArduinoFloppyDiskReader/actions/workflows/waffle-fuse-appimage.yml

Common issues:
- **Dependency not installed**: The workflow installs all needed packages; check the build log
- **appimagetool download fails**: Network issue; retry manually via "Run workflow"
- **Permission denied**: The script sets correct permissions; may be an artifact download issue

### AppImage won't run

```bash
# Make it executable
chmod +x Waffle-Fuse-*.AppImage

# Try extracting instead
./Waffle-Fuse-*.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

## See also

- [.AppImage/README.md](.AppImage/README.md) – AppImage building and usage
- [README.md](README.md) – Main waffle-fuse documentation
- [AppImage documentation](https://docs.appimage.org/)
