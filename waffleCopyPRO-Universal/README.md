# WaffleCopyPRO-Universal

WaffleCopyPRO-Universal is a powerful, cross-platform floppy disk imaging and writing tool primarily designed for Amiga systems. It allows users to read and write Amiga floppy disk images (ADF, IPF, etc.) using standard PC floppy drives and custom hardware interfaces like the Waffle or DrawBridge. This project aims to provide a comprehensive and user-friendly solution for Amiga enthusiasts to preserve and utilize their floppy disk collections across various operating systems.

## Features

*   **Universal Compatibility**: Works seamlessly with WAFFLE, or other various floppy drive interfaces, including DrawBridge.
*   **Multi-platform Support**: Natively supported on Linux, macOS (x86_64 and Apple Silicon), and Windows.
*   **Intuitive GUI**: A user-friendly graphical interface built with Qt, allowing for easy operation.
*   **Open Source**: Fully open-source, enabling community contributions, customizations, and extensions.
*   **Advanced Imaging**: Supports various Amiga disk formats for accurate preservation and restoration.

## Screenshots

Here are a couple of screenshots demonstrating WaffleCopyPRO-Universal in action:

### Startup Screen

![WaffleCopyPRO-Universal Startup Screen](WaffleUI/waffle-startup.png)

### Working Screen

![WaffleCopyPRO-Universal Working Screen](WaffleUI/waffle-working.png)

## Multi-platform Support

WaffleCopyPRO-Universal is built with cross-platform compatibility in mind, ensuring a consistent experience across different operating systems:

*   **Linux**: Fully supported, leveraging standard serial port communication.
*   **macOS**: Compatible with both Intel (x86_64) and Apple Silicon architectures, utilizing macOS-specific serial port handling.
*   **Windows**: Comprehensive support for Windows operating systems, including FTDI and standard COM port enumeration.

## How It Works: A Glimpse into the Code

The core functionality of WaffleCopyPRO-Universal involves low-level serial communication to interact with floppy drive controllers. Below is a simplified snippet from `lib/SerialIO.cpp` demonstrating how the application opens a serial port, handling platform-specific differences:

```cpp
// Open a port by name
SerialIO::Response SerialIO::openPort(const std::wstring& portName) {
	closePort();

#ifdef FTDI_D2XX_AVAILABLE
	// ... FTDI specific code ...
#endif

#ifdef _WIN32
	std::wstring path = L"\\.\\" + portName;
	m_portHandle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0);
	if (m_portHandle == INVALID_HANDLE_VALUE) {
		switch (GetLastError()) {
		case ERROR_FILE_NOT_FOUND:  return Response::rNotFound;
		case ERROR_ACCESS_DENIED:   return Response::rInUse;
		default: return Response::rUnknownError;
		}
	}
	updateTimeouts();
	return Response::rOK;
#else
	std::string apath;
	quickw2a(portName, apath);
#ifdef __APPLE__
	m_portHandle = open(apath.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
#else
	m_portHandle = open(apath.c_str(), O_RDWR | O_NOCTTY);
#endif
	if (m_portHandle == -1) {
		switch (errno) {
		case ENOENT: return Response::rNotFound;
		case EBUSY: return Response::rInUse;
		default: return Response::rUnknownError;
		}
	}

	// ... Linux/macOS specific code for locking and configuring port ...

	updateTimeouts();
	return Response::rOK;
#endif

	return Response::rNotImplemented;
}
```
This snippet illustrates the use of preprocessor directives (`#ifdef`, `#else`) to adapt the code for different operating systems (Windows, macOS, Linux) and specific hardware interfaces (FTDI). This modular approach ensures broad compatibility and efficient hardware interaction.

## Open Source & Customization

WaffleCopyPRO-Universal is an open-source project, licensed under the Mozilla Public License Version 2.0 and GNU General Public License, version 2 or later. This means:

*   **Full Transparency**: The entire codebase is available for review, understanding, and modification.
*   **Community Driven**: We encourage contributions from the community to improve features, fix bugs, and extend functionality.
*   **GUI Customization**: The graphical user interface, built with Qt, can be customized to your liking. Feel free to modify `mainwindow.ui` and related C++ files (`mainwindow.cpp`, `mainwindow.h`) to tailor the look and feel.
*   **Code Extensibility**: Developers are welcome to dive into the core logic, add support for new hardware, implement new disk formats, or optimize existing routines.

Join us in making WaffleCopyPRO-Universal the ultimate tool for Amiga floppy disk management!
