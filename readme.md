# AVR Powered Amiga Floppy Disk Reader and Writer
Created by Robert Smith @RobSmithDev
...with interfaces for Amiga, ATARI ST and DOS/PC Disk formats 
POSIX compatibility by Gianluca Renzi <icjtqr@gmail.com> <gianlucarenzi@eurek.it>

# What is it?
This project uses an Arduino to interface with a floppy disk drive and 
communicate with a PC in order to recover the data from any formatted 
disks. 

The drive can be either a 8", 5 1/4" or 3 1/2" standard floppy drive.
It was tested on a 3 1/2" standard PC floppy drive. Others (like the 
5 1/4" standard PC floppy drive) my also work without modifications.

This configration can read SD, DD and HD floppy disk formats 
(AMIGA, ATARI ST, PC DOS, COMMODORE C64) 
and maybe more. 

The Arduino firmware allows to read the raw data from each track of the
floppy. Decoding of the sector data is done on the PC. Usually a floppy image
file is created (ADF for AMIGA, .img for ATARI ST and PC/DOS).

It also allows you to write a backed up ADF file back onto a floppy disk!
The portable build is on the way! Now it works on Linux.

# ArduinoFloppyReader
This Visual Studio 2019 project contains two applications, a command line, 
and a Windows dialog based application allow reading and writing of Amiga 
formatted DD floppy disks.

# Scripts for linux
The above application apparently works under WINE, however,
Github user "kollokollo" made some scripts for reading other formats on Linux 
too as follows:
	The ATARI ST and DOS/PC floppy formats can be decoded whith these scripts.
	9,10,11 or 18 Sectors per track. Up to 82 tracks, DD (ca. 800 kBytes) or 
	HD (1.4 MBytes). The images usually contain a FAT12 file system which can be 
	directly mounted by linuy without any additional driver.   
	For more information see 
		https://github.com/kollokollo/ArduinoFloppyDiskReader/tree/new/for_linux
		They need the X11-Basic interpreter from http://x11-basic.sourceforge.net/

# Commodore 1581 Disks
	To read commodore 1581 disks, check out the project at: 
		https://github.com/hpingel/pyAccess1581

# FloppyDriverController.sketch
This is the Ardunio source code/sketch for all Floppy formats.
* Detect disk density (SD/DD or HD)
* Motor ON/OFF
* Seek to Track 0
* Seek to any track (up to 82 - be careful, this can damage some drives!)
* read write protection status
* Read index pulse
* read raw track data (its, RAW, so FM, MFM; SD, DD or HD)
* write track data (unbuffered, DD, untested HD)

# AVR Firmware
In my project I am using the AVR based hardware (not Arduino!) so jump to [https://github.com/gianlucarenzi/usbamigafloppy] where I ported the code and the hardware stuff.

# Help and Instructions
TODO

# Whats changed?
V1.0 Initial release

# Licence
This entire project is available under the GNU General Public License v3 licence.  See licence.txt for more details.
