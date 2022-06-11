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
#include <stdint.h>
#include "waffleconfig.h"
#ifdef _WIN32
#include <conio.h>
#else
#include <stdio.h>
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

// declaration of file pointers
static FILE *trackFile = NULL;
static FILE *sideFile = NULL;
static FILE *badFile = NULL;

using namespace ArduinoFloppyReader;

ADFWriter writer;

static void prepareUIFiles(void)
{
    int zero = 0;
    trackFile = fopen(TRACKFILE, "w+b");
    sideFile  = fopen(SIDEFILE, "w+b");
    badFile   = fopen(STATUSFILE, "w+b");
    fprintf(trackFile, "%d", zero);
    fprintf(sideFile, "%d", zero);
    fprintf(badFile, "%d", zero);
    fflush(trackFile);
    fflush(sideFile);
    fflush(badFile);
}

static void removeUIFiles(void)
{
    if (trackFile)
        fclose(trackFile);
    if (sideFile)
        fclose(sideFile);
    if (badFile)
        fclose(badFile);
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
                    input = 'S';
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
			case 'I': return WriteResponse::wrSkipBadChecksums;
			case 'A': return WriteResponse::wrAbort;
			}
		}
#ifdef __USE_GUI__
        if (trackFile) {
            //fprintf(stdout, "%s trackFile %d\n", __PRETTY_FUNCTION__, currentTrack);
            fseek(trackFile, 0, SEEK_SET);
            fprintf(trackFile, "%d", currentTrack);
            fflush(trackFile);
        } else {
            fprintf(stderr, "%s trackFile NOT Valid\n", __PRETTY_FUNCTION__);
        }
        if (sideFile) {
            //fprintf(stdout, "%s SideFile %s\n", __PRETTY_FUNCTION__,
            //        (currentSide == DiskSurface::dsUpper) ? "UPPER" : "LOWER");
            fseek(sideFile, 0, SEEK_SET);
            fprintf(sideFile, "%d", (currentSide == DiskSurface::dsUpper) ? 1 : 0);
            fflush(sideFile);
        } else {
            fprintf(stderr, "%s SideFile NOT Valid\n", __PRETTY_FUNCTION__);
        }
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
                    input = 'I';
#else
    #ifdef _WIN32
                    input = toupper(_getch());
    #else
                    input = toupper(_getChar());
    #endif
#endif

			} while ((input != 'R') && (input != 'I') && (input != 'A'));
			switch (input) {
			case 'R': return WriteResponse::wrRetry;
			case 'I': return WriteResponse::wrSkipBadChecksums;
			case 'A': return WriteResponse::wrAbort;
			}
		}

#ifdef __USE_GUI__
        if (trackFile) {
            //fprintf(stdout, "%s TrackFile %d\n", __PRETTY_FUNCTION__, currentTrack);
            fseek(trackFile, 0, SEEK_SET);
            fprintf(trackFile, "%d", currentTrack);
            fflush(trackFile);
        } else {
            fprintf(stderr, "%s TrackFile NOT Valid\n", __PRETTY_FUNCTION__);
        }
        if (sideFile) {
            //fprintf(stdout, "%s SideFile %s\n", __PRETTY_FUNCTION__,
            //        (currentSide == DiskSurface::dsUpper) ? "UPPER" : "LOWER");
            fseek(sideFile, 0, SEEK_SET);
            fprintf(sideFile, "%d", (currentSide == DiskSurface::dsUpper) ? 1 : 0);
            fflush(sideFile);
        } else {
            fprintf(stderr, "%s SideFile NOT Valid\n", __PRETTY_FUNCTION__);
        }
        if (badFile) {
            //fprintf(stdout, "%s BadFile %d\n", __PRETTY_FUNCTION__, badSectorsFound);
            fseek(badFile, 0, SEEK_SET);
            fprintf(badFile, "%d", badSectorsFound);
            fflush(badFile);
        } else {
            fprintf(stderr, "%s BadFile NOT Valid\n", __PRETTY_FUNCTION__);
        }
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

int wmain(QStringList list)
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

    if (list.contains("DIAGNOSTIC")) {
            runDiagnostics(port);
    } else {
        if (!writer.openDevice(port)) {
			printf("\rError opening COM port: %s  ", writer.getLastError().c_str());
		}
		else {
            prepareUIFiles();
            if (writeMode) adf2Disk(filename.c_str(), verify, preComp, eraseBeforeWrite); else disk2ADF(filename.c_str(), numTracks);
			writer.closeDevice();
            removeUIFiles();
		}
	}
	printf("\r\n\r\n");
    return 0;
}

