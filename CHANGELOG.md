# ChangeLog - waffleCopyPRO-Universal

## [v2.10.2] - 2026-11-08

### Fixed
- **Track boxes drawn over the grid lines on high-DPI screens**: the v2.10.1 change made the misalignment worse. Scaling each layout component separately (`col * int(cellSize * scale)`) accumulated rounding errors, so on fractional scale factors (e.g. 1.12, 1.37, 1.45, 1.74 — typical of a 2560x1440 macOS display) the green/red track boxes overlapped the grid lines.
  - `prepareTracksPosition()` now computes each box from the same rounded grid-line coordinates used by `createTrackGrid()` and centers it inside the cell interior.
  - Verified numerically for every scale factor from 1.00 to 2.99, and with real screenshots of a simulated read at scales 0.75, 1.37, 1.45, 1.48 (3440x1440), 2.66 (3840x2160) and 3.54 (5120x2880): no box touches the grid, max 1px centering difference.
- **Tiny fonts after changing language**: `changeLanguage()` reset the application font to `9pt`, discarding the high-DPI scaling (on macOS 1pt = 1px, so text dropped to 9px). For Greek/Russian/Ukrainian/Japanese/Chinese the font always ended up at 8pt (also at startup for el/ru/uk), and the diagnostic font could shrink to 2px. Fonts are now always sized in pixels and scaled with the UI scale factor.
- **Unscaled fonts when the Topaz font fails to load**: the scale factor is now computed and the fallback system font is scaled too.

### Changed
- **macOS scale factor**: the size tier is now chosen from the full logical screen size instead of the area left free by the menu bar and the Dock, so a Mac no longer falls into a smaller tier than the same screen on Windows/Linux (e.g. 2560x1440 with the Dock: 1.37 → 1.48; 1920x1080: 1.05 → 1.15).
- The window is now guaranteed to fit the available screen area on every platform.
- **Menu bar and menus** scale their item padding (items no longer touch each other at 4K).
- **Check boxes** scale their indicator size and spacing together with the text.
- The scale factor auto-detection is now a single shared function (`MainWindow::calculateScaleFactor()`) used by both `main.cpp` and `MainWindow`.
- Removed a duplicated `QDir::setCurrent()` call in the macOS startup code.

---

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
