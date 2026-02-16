# WaffleCopyPRO-Universal

WaffleCopyPRO-Universal is a powerful, cross-platform tool for reading and writing floppy disks, with a special focus on Amiga disk formats. It allows users to manage and preserve their floppy disk collections using modern computers and a variety of hardware interfaces like the RetroGiovedì Waffle or the Arduino DrawBridge.

This project provides a complete, user-friendly solution for retro computing enthusiasts to bridge the gap between vintage floppies and modern hardware.

## Key Features

*   **Truly Universal and Cross-Platform**: Natively runs on **Windows**, **macOS** (Intel & Apple Silicon), and **Linux**, providing a consistent experience everywhere.
*   **Extensive Hardware Compatibility**: Works with a wide range of floppy disk controllers, including:
    *   **RetroGiovedì Waffle**
    *   **Arduino DrawBridge** (and its variants)
    *   Other Arduino-based and FTDI solutions.
*   **Rich Localization**: The interface is fully translated into **16 languages** to feel native to users across the globe. Supported languages include: *English, Italian, German, Spanish, French, Portuguese, Polish, Greek, Hungarian, Russian, Ukrainian, Czech, Romanian, Serbian/Croatian, Japanese, and Simplified Chinese*.
*   **Retro Look & Feel**: The UI is designed with an "old school" aesthetic that pays homage to the golden age of computing. Featuring the classic **Amiga Topaz font** and demoscene-inspired elements like a sine-scroller, it brings a touch of nostalgia to your workflow.
*   **Adaptive UI for All Screens**: Whether you're using a small laptop or a large desktop monitor, the interface is designed to be clear and functional, ensuring a great user experience on any screen size.
*   **Advanced Disk Format Support**: Reliably handles Amiga Disk Files (`.ADF`) and includes statically-linked support for the Interchangeable Preservation Format (`.IPF`) via the CAPS library, eliminating the need for external plugins or dependencies.
*   **Open Source**: The project is open source, encouraging community contributions and customization.

## Screenshots

Here is a glimpse of WaffleCopyPRO-Universal in action.

**Startup Screen**
*The main window, showing the classic Amiga-inspired theme.*
![WaffleCopyPRO-Universal Startup Screen](WaffleUI/waffle-startup.png)

**Disk Operation in Progress**
*Shows the interface while reading or writing a disk.*
![WaffleCopyPRO-Universal Working Screen](WaffleUI/waffle-working.png)

***To better showcase the application's features, I would appreciate it if you could provide the following screenshots:***

1.  ***A screenshot of the language selection menu (`File -> Language`).***
2.  ***A view of the sine-scroller text in action (it appears during operations).***
3.  ***A screenshot of the `Diagnostics` tab.***

## Installation

The project is built with Qt 5.15.2. Detailed, platform-specific instructions can be found in the original `README.md`. A summary is provided below:

*   **Linux**: Install `qt5-default`, `libqt5serialport5-dev`, `libmikmod-dev`, `libftdi1-dev`, then run `qmake && make`.
*   **macOS**: Install dependencies like `libmikmod` via Homebrew, then use `qmake && make` to build the `.app` bundle.
*   **Windows**: Use Visual Studio 2019 and the Qt 5.15.2 MSVC kit. Dependencies like `libmikmod` can be installed via `vcpkg`. Build using `qmake` and `msbuild`.

## Usage Guide

1.  **Connect Hardware**: Plug in your Waffle, DrawBridge, or other floppy controller.
2.  **Launch the App**: Start `waffleCopyPRO-Universal`.
3.  **Select Serial Port**: Choose the correct COM/serial port for your device from the dropdown menu.
4.  **Read a Disk**: Insert a floppy, select a location to save the image file, and click "Start".
5.  **Write a Disk**: Insert a floppy, select an `.ADF` or `.IPF` image file, and click "Start".
6.  **Change Language**: Go to `File -> Language`, select your preferred language, and restart the application.

---
*This README was updated to better highlight the project's unique features. The original, more detailed document is preserved in the project history.*
