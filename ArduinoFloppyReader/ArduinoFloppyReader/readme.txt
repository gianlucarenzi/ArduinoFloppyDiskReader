ArduinoReaderWriter
https://amiga.robsmithdev.co.uk

This is a console application

It should build with Visual Studio 2019

It will also compile on some distros of Linux.

It has been tested on Raspberry Pi OS (Raspbian - Debian-based) and can be compiled using the provided makefile.

The CAPS/IPF decoder sources are now vendored with the project under `../lib/capsimg`,
so DrawBridge can reliably read and write/copy both **IPF** and **SuperCard Pro
(`.SCP`)** images without requiring a separate CAPS library installation on Linux,
macOS or Windows.

The vendored CAPS sources were also adjusted to build cleanly with recent compilers
such as gcc 14.2.0.
