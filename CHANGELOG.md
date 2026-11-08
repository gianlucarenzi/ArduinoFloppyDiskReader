# ChangeLog - waffleCopyPRO-Universal

## [v2.10.1] - 2026-11-08

### Fixed
- **Track grid alignment on high-DPI displays**: Fixed a critical bug in `prepareTracksPosition()` that caused track indicator boxes (green/red squares) to be drawn outside the grid on high-density screens (4K, 2K, Retina).
  - **Issue**: Coordinate scaling was performed on already-composed values, causing cumulative rounding errors with fractional scale factors (1.25, 1.5, 1.56, etc.)
  - **Root cause**: On macOS Retina and 4K displays on Windows/Linux, the scale factor calculation resulted in non-integer values, leading to progressive misalignment across tracks.
  - **Solution**: Refactored to scale each coordinate component individually before summation, ensuring pixel-perfect grid alignment on all DPI settings.
  - **Impact**: Fixes visual rendering on:
    - macOS Retina displays (both Intel & Apple Silicon)
    - Windows 4K/ultrawide/high-DPI setups
    - Linux systems with HiDPI scaling
  - **Commit**: 6f44acf

### Changed
- Synchronized `cellSize` calculation in `prepareTracksPosition()` with `createTrackGrid()` to prevent future desynchronization
- Improved code consistency by scaling all layout components uniformly

---

## [v2.10.0] - 2026-10-12

### Added
- Qt6 support with proper high-DPI scaling compatibility
- Unified high-DPI behavior between Qt5 and Qt6

### Fixed
- Qt6 scaling issues by disabling automatic high-DPI scaling to match Qt5 behavior

---

## Previous Releases
See commit history for changes prior to v2.10.0.
