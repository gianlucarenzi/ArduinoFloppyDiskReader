/* ArduinoFloppyReader (and writer)
*
* Copyright (C) 2017-2021 Robert Smith (@RobSmithDev)
* https://amiga.robsmithdev.co.uk
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Library General Public
* License as published by the Free Software Foundation; either
* version 3 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Library General Public License for more details.
*
* You should have received a copy of the GNU Library General Public
* License along with this program; if not see http://www.gnu.org/licenses/
*/

////////////////////////////////////////////////////////////////////////////////////////////
// Example console application for reading and writing floppy disks to and from ADF files //
////////////////////////////////////////////////////////////////////////////////////////////

#include "ADFWriter.h"
#include "ArduinoInterface.h"
#include <stdio.h>
#include <stdint.h>
#include "waffleconfig.h"
#include "compilerdefs.h"
#ifdef _WIN32
#include <conio.h>
#include <io.h>
#else
#include <termios.h>

#ifndef _wcsupr
#include <wctype.h>
void _wcsupr(wchar_t* str) {
	while (*str) {
		*str = towupper(*str);
		str++;
	}
}
#endif
static struct termios old, current;

/* Initialize new terminal i/o settings */
void initTermios(int echo) 
{
  tcgetattr(0, &old); /* grab old terminal i/o settings */
  current = old; /* make new settings same as old settings */
  current.c_lflag &= ~ICANON; /* disable buffered i/o */
  if (echo) {
      current.c_lflag |= ECHO; /* set echo mode */
  } else {
      current.c_lflag &= ~ECHO; /* set no echo mode */
  }
  tcsetattr(0, TCSANOW, &current); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
void resetTermios(void) 
{
  tcsetattr(0, TCSANOW, &old);
}

char _getChar() 
{
  char ch;
  initTermios(0);
  ch = getchar();
  resetTermios();
  return ch;
}

std::wstring atw(const std::string& str) {
	std::wstring ws(str.size(), L' '); 
	ws.resize(::mbstowcs((wchar_t*)ws.data(), str.c_str(), str.size())); 
	return ws;
}
#endif


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <string.h>

// declaration of file pointers
static int fd_trackFile = -1;
static int fd_sideFile = -1;
static int fd_statusFile = -1;
static int fd_errorFile = -1;

using namespace ArduinoFloppyReader;

ADFWriter writer;

static void prepareUIFiles(const char *tFile, const char *sFile, const char *bFile, const char *eFile)
{
    char zero[1] = { '0' };
    if (tFile == NULL)
    {
        fprintf(stderr, "tFile is NULL\n");
        exit(1);
    }
    if (sFile == NULL)
    {
        fprintf(stderr, "sFile is NULL\n");
        exit(1);
    }
    if (bFile == NULL)
    {
        fprintf(stderr, "bFile is NULL\n");
        exit(1);
    }
    if (eFile == NULL)
    {
        fprintf(stderr, "eFile is NULL\n");
        exit(1);
    }
    //fprintf(stdout, "prepareUIFiles TrackFile: %s\n", tFile);
    //fprintf(stdout, "prepareUIFiles SideFile: %s\n", sFile);
    //fprintf(stdout, "prepareUIFiles StatusFile: %s\n", bFile);
    //fprintf(stdout, "prepareUIFiles ErrorFile: %s\n", eFile);
    fflush(stdout);
    fd_trackFile  = open(tFile, O_RDWR);
    fd_sideFile   = open(sFile, O_RDWR);
    fd_statusFile = open(bFile, O_RDWR);
    fd_errorFile  = open(eFile, O_RDWR);
    write(fd_trackFile, zero, 1);
    write(fd_sideFile, zero, 1);
    write(fd_statusFile, zero, 1);
    write(fd_errorFile, zero, 1);
}

static void removeUIFiles(void)
{
    if (fd_trackFile)
        close(fd_trackFile);
    if (fd_sideFile)
        close(fd_sideFile);
    if (fd_statusFile)
        close(fd_statusFile);
    if (fd_errorFile)
        close(fd_errorFile);
    fd_trackFile = -1;
    fd_sideFile = -1;
    fd_statusFile = -1;
    fd_errorFile = -1;
}

static char userInput = 'Z';
static bool userInputDone = false;
static char wait_user_input(void);

/* Global function. It will be called by GUI */
void set_user_input(char uinput);

void set_user_input(char uinput)
{
    userInput = uinput;
    userInputDone = true;
}

char wait_user_input(void)
{
    for (;;)
    {
        if (userInputDone != false)
            break;
        usleep(50 * 1000L);
    }
    userInputDone = false; // Valid for the next time
    return userInput;
}

static void update_error_file(int err)
{
    if (fd_errorFile > 0) {
        //fprintf(stdout, "ADF2Disk ErrorFile %s\n", err);
        lseek(fd_errorFile, 0L, SEEK_SET);
        char errS[2];
        sprintf(errS, "%01d", err);
        write(fd_errorFile, errS, 1);
    } else {
        fprintf(stderr, "ADF2Disk errorFile NOT Valid\n");
    }
}

static void update_gui_writing(int32_t currentTrack, DiskSurface currentSide)
{
    if (fd_trackFile > 0) {
        //fprintf(stdout, "ADF2Disk trackFile %d -- %d\n", fd_trackFile, currentTrack);
        lseek(fd_trackFile, 0L, SEEK_SET);
        char tr[4];
        sprintf(tr, "%03d", currentTrack);
        write(fd_trackFile, tr, 3);
    } else {
        fprintf(stderr, "ADF2Disk trackFile NOT Valid\n");
    }
    if (fd_sideFile > 0) {
        //fprintf(stdout, "ADF2Disk SideFile %s\n",
        //        (currentSide == DiskSurface::dsUpper) ? "UPPER" : "LOWER");
        lseek(fd_sideFile, 0L, SEEK_SET);
        char side[3];
        sprintf(side, "%02d", (currentSide == DiskSurface::dsUpper) ? 1 : 0);
        write(fd_sideFile, side, 2);
    } else {
        fprintf(stderr, "%s SideFile NOT Valid\n", __PRETTY_FUNCTION__);
    }
}

static void update_gui_reading(int32_t currentTrack, DiskSurface currentSide, int32_t badSectorsFound)
{
    update_gui_writing(currentTrack, currentSide);
    if (fd_statusFile) {
        //fprintf(stdout, "Disk2ADF statusFile %d\n", badSectorsFound);
        lseek(fd_statusFile, 0L, SEEK_SET);
        char status[3];
        sprintf(status, "%02d", badSectorsFound);
        write(fd_statusFile, status, 2);
    } else {
        fprintf(stderr, "Disk2ADF statusFile NOT Valid\n");
    }
}

// Read an ADF file and write it to disk
void adf2Disk(const std::wstring& filename, bool verify, bool preComp, bool eraseFirst) {
    bool hdMode = false;
    bool isSCP = true;
    bool isIPF = false;
    bool writeFromIndex = true;

    printf("\nWrite disk from ADF mode\n\n");
    if (isIPF) printf("\nWrite disk from IPF mode\n\n"); else
    if (isSCP) printf("\nWrite disk from SCP mode\n\n"); else {
        printf("\nWrite disk from ADF mode\n\n");
        if (!verify) printf("WARNING: It is STRONGLY recommended to write with verify support turned on.\r\n\r\n");
    }

    //ADFResult ADFToDisk(const std::wstring& inputFile, const bool inHDMode, bool verify, bool usePrecompMode, bool eraseFirst, bool writeFromIndex, std::function < WriteResponse(const int currentTrack, const DiskSurface currentSide, const bool isVerifyError, const CallbackOperation operation) > callback);
    // To be sure erase first of writing with NO PRECOMP
    ADFResult result = writer.ADFToDisk(filename, hdMode, verify, preComp, eraseFirst, writeFromIndex, [](const int32_t currentTrack, const DiskSurface currentSide, bool isVerifyError, const CallbackOperation operation) ->WriteResponse {
		if (isVerifyError) {
			char input;
			do {
				printf("\rDisk write verify error on track %i, %s side. [R]etry, [S]kip, [A]bort?                                   ", currentTrack, (currentSide == DiskSurface::dsUpper) ? "Upper" : "Lower");
#ifdef __USE_GUI__
                    update_error_file(1); fprintf(stderr, "\r\n %s WRITE ERROR NOW WAIT USER INPUT\r\n", __PRETTY_FUNCTION__); fflush(stderr); input = wait_user_input();
#else
    #ifdef _WIN32
                    input = toupper(_getch());
    #else
                    input = toupper(_getChar());
    #endif
#endif
            } while ((input != 'R') && (input != 'S') && (input != 'A'));
			
			switch (input) {
			case 'R': return WriteResponse::wrRetry;
            case 'S': return WriteResponse::wrSkipBadChecksums;
			case 'A': return WriteResponse::wrAbort;
			}
		}
#ifdef __USE_GUI__
        update_gui_writing(currentTrack, currentSide);
#else
        printf("\rWriting Track %i, %s side     ", currentTrack, (currentSide == DiskSurface::dsUpper) ? "Upper" : "Lower");
#endif
#ifndef _WIN32
		fflush(stdout);		
#endif		
		return WriteResponse::wrContinue;
	});

	switch (result) {
	case ADFResult::adfrComplete:					printf("\rADF file written to disk                                                           "); break;
	case ADFResult::adfrCompletedWithErrors:		printf("\rADF file written to disk but there were errors during verification                 "); break;
	case ADFResult::adfrAborted:					printf("\rWriting ADF file to disk                                                           "); break;
	case ADFResult::adfrFileError:					printf("\rError opening ADF file.                                                            "); break;
	case ADFResult::adfrDriveError:					printf("\rError communicating with the Arduino interface.                                    "); 
													printf("\n%s                                                  ", writer.getLastError().c_str()); break;
	case ADFResult::adfrDiskWriteProtected:			printf("\rError, disk is write protected!                                                    "); break;
	default:										printf("\rAn unknown error occured                                                           "); break;
	}
}

// Stolen from https://stackoverflow.com/questions/11635/case-insensitive-string-comparison-in-c
// A wide-string case insensative compare of strings
bool iequals(const std::wstring& a, const std::wstring& b) {
	return std::equal(a.begin(), a.end(),b.begin(), b.end(),[](wchar_t a, wchar_t b) {
			return tolower(a) == tolower(b);
	});
}
bool iequals(const std::string& a, const std::string& b) {
	return std::equal(a.begin(), a.end(),b.begin(), b.end(),[](char a, char b) {
			return tolower(a) == tolower(b);
	});
}

// Read a disk and save it to ADF or SCP files
void disk2ADF(const std::wstring& filename, int numTracks) {
	const wchar_t* extension = wcsrchr(filename.c_str(), L'.');
	bool isADF = true;

	if (extension) {
		extension++;
		isADF = !iequals(extension, L"SCP");
	}

	if (isADF) printf("\nCreate ADF from disk mode\n\n"); else printf("\nCreate SCP file from disk mode\n\n");

	// Get the current firmware version.  Only valid if openDevice is successful
	const ArduinoFloppyReader::FirmwareVersion v = writer.getFirwareVersion();
	if ((v.major == 1) && (v.minor < 8)) {
		if (!isADF) {
			printf("This requires firmware V1.8 or newer.\n");
			return;
		}
		else {
			// improved disk timings in 1.8, so make them aware
			printf("Rob strongly recommends updating the firmware on your Arduino to at least V1.8.\n");
			printf("That version is even better at reading old disks.\n");
		}
	}

    bool hdMode = false;

    auto callback = [isADF](const int32_t currentTrack, const DiskSurface currentSide, const int32_t retryCounter, const int32_t sectorsFound, const int32_t badSectorsFound, const int totalSectors, const CallbackOperation operation) ->WriteResponse {
		if (retryCounter > 20) {
			char input;
			do {
                // Disk has checksum errors/missing data. Skip Bad checksum always
#ifdef __USE_GUI__
                    // Now we have to inform the GUI Thread we have an error, and we need to decide how to proceed
                    update_error_file(1); fprintf(stderr, "\r\n %s READ ERROR NOW WAIT USER INPUT \r\n", __PRETTY_FUNCTION__); fflush(stderr); input = wait_user_input();
#else
    #ifdef _WIN32
                    input = toupper(_getch());
    #else
                    input = toupper(_getChar());
    #endif
#endif

            } while ((input != 'R') && (input != 'S') && (input != 'A'));
			switch (input) {
			case 'R': return WriteResponse::wrRetry;
            case 'S': return WriteResponse::wrSkipBadChecksums;
			case 'A': return WriteResponse::wrAbort;
			}
		}

#ifdef __USE_GUI__
        update_gui_reading( currentTrack, currentSide, badSectorsFound );
#else
        if (isADF) {
            printf("\rReading Track %i, %s side (retry: %i) - Got %i/11 sectors (%i bad found)   ", currentTrack, (currentSide == DiskSurface::dsUpper) ? "Upper" : "Lower", retryCounter, sectorsFound, badSectorsFound);
        }
        else {
            printf("\rReading Track %i, %s side   ", currentTrack, (currentSide == DiskSurface::dsUpper) ? "Upper" : "Lower");
        }
#endif

#ifndef _WIN32
		fflush(stdout);		
#endif		
		return WriteResponse::wrContinue;
	};

	ADFResult result;
	
    if (numTracks > 83)
        numTracks = 83;

    if (isADF) result = writer.DiskToADF(filename, hdMode, numTracks, callback); else result = writer.DiskToSCP(filename, hdMode, numTracks, 3, callback);

	switch (result) {
	case ADFResult::adfrComplete:					printf("\rFile created successfully.                                                     "); break;
	case ADFResult::adfrAborted:					printf("\rFile aborted.                                                                  "); break;
	case ADFResult::adfrFileError:					printf("\rError creating file.                                                           "); break;
	case ADFResult::adfrFileIOError:				printf("\rError writing to file.                                                         "); break;
	case ADFResult::adfrFirmwareTooOld:			    printf("\rThis requires firmware V1.8 or newer.                                          "); break;
	case ADFResult::adfrCompletedWithErrors:		printf("\rFile created with partial success.                                             "); break;
	case ADFResult::adfrDriveError:					printf("\rError communicating with the Arduino interface.                                ");
													printf("\n%s                                                  ", writer.getLastError().c_str()); break;
	default: 										printf("\rAn unknown error occured.                                                      "); break;
	}
}

// Run the diagnostics module
void runDiagnostics(const std::wstring& port) {
	printf("\rRunning diagnostics on COM port: %ls\n",port.c_str());

	writer.runDiagnostics(port, [](bool isError, const std::string message)->void {
		if (isError)
			printf("DIAGNOSTICS FAILED: %s\n",message.c_str());
		else 
			printf("%s\n", message.c_str());
	}, [](bool isQuestion, const std::string question)->bool {
		if (isQuestion) 
			printf("%s [Y/N]: ", question.c_str());
		else 
			printf("%s [Enter/ESC]: ", question.c_str());

		char c;
		do {
#ifdef _WIN32
			c = toupper(_getch());
#else
			c = toupper(_getChar());
#endif	
		} while ((c != 'Y') && (c != 'N') && (c != '\n') && (c != '\r') && (c != '\x1B'));
		printf("%c\n", c);

		return (c == 'Y') || (c == '\n') || (c == '\r') || (c == '\x1B');
	});

	writer.closeDevice();
}

#include <QStringList>
#include <QDebug>
#include <QTemporaryFile>
#include <QFile>
#include <qtdrawbridge.h>

int wmain(QStringList list, QString track, QString side, QString status, QString error)
{
    bool writeMode = list.contains("WRITE");
    bool verify = list.contains("VERIFY");
    bool preComp = list.contains("PRECOMP");
    bool eraseBeforeWrite = list.contains("ERASEBEFOREWRITE");
    bool num82Tracks = list.contains("TRACKSNUM82");
    int numTracks = 80;

    if (num82Tracks)
        numTracks = 82;

    std::wstring port = list.at(0).toStdWString();
    std::wstring filename = list.at(1).toStdWString();;

    qDebug() << "port" << QString::fromStdWString(port);
    qDebug() << "filename" << QString::fromStdWString(filename);

    qDebug() << "writeMode" << writeMode;
    qDebug() << "verify" << verify;
    qDebug() << "preComp" << preComp;
    qDebug() << "eraseBeforeWrite" << eraseBeforeWrite;
    qDebug() << "TRACKS" << numTracks;

    userInputDone = false; // It will be changed by the user on errors

    if (list.contains("DIAGNOSTIC")) {
            runDiagnostics(port);
    } else {
        if (!writer.openDevice(port)) {
			printf("\rError opening COM port: %s  ", writer.getLastError().c_str());
		}
		else {
            QByteArray tr = track.toLocal8Bit();
            char *fTrack = tr.data();
            QByteArray sd = side.toLocal8Bit();
            char *fSide = sd.data();
            QByteArray st = status.toLocal8Bit();
            char *fStatus = st.data();
            QByteArray er = error.toLocal8Bit();
            char *fError = er.data();
            //qDebug() << "TrackFile: " << fTrack << "SideFile: " << fSide << "StatusFile: " << fStatus << "ErrorFile: " << fError;
            prepareUIFiles(fTrack, fSide, fStatus, fError);
            if (writeMode) adf2Disk(filename.c_str(), verify, preComp, eraseBeforeWrite); else disk2ADF(filename.c_str(), numTracks);
			writer.closeDevice();
            removeUIFiles();
		}
	}
	printf("\r\n\r\n");
    return 0;
}

