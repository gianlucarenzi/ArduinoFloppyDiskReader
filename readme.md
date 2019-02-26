# AVR Powered Amiga Floppy Disk Reader and Writer
Created by Robert Smith @RobSmithDev
POSIX compatibility by Gianluca Renzi <icjtqr@gmail.com> <gianlucarenzi@eurek.it>

# What is it?
The original project uses an Arduino to interface with a floppy disk drive and communicate with a PC in order to recover the data from Amiga formatted AmigaDOS disks.
It also allows you to write a backed up ADF file back onto a floppy disk!
The portable build is on the way! Now it works on Linux.

# ArduinoFloppyReader
This Visual Studio 2017 project contains two applications, a command line, and a Windows dialog based application
It will be replaced by the Qt version as soon as the POSIX compatibility layer proves to be usable.


# FloppyDriverController.sketch (not used in this project)
This is the Arduino source code/sketch

# AVR Firmware
In my project I am using the AVR based hardware (not Arduino!) so jump to [https://github.com/gianlucarenzi/usbamigafloppy] where I ported the code and the hardware stuff.

# Help and Instructions
TODO

# Whats changed?
V1.0 Initial release

# Licence
This entire project is available under the GNU General Public License v3 licence.  See licence.txt for more details.
