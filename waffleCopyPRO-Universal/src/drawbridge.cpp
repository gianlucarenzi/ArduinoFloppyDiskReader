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
#include <iostream>
#include <chrono>
#include <thread>
#include <socketserver.h>

static SOCKET sockfd = INVALID_SOCKET;

static struct sockaddr_in servaddr;

static char shmCommand[MAXBUFF];

static void ClearWinSock(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

static void setupSocketClient(void)
{
    //int rval;
    //int counter = 0;
    //fprintf(stdout, "%s Called\n", __FUNCTION__);
    if (sockfd == INVALID_SOCKET)
    {
#ifdef _WIN32
        WSADATA wsaData;
        int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0)
        {
            qDebug() << __PRETTY_FUNCTION__ << "Error at WSAStartup";
            return;
        }
#endif
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == INVALID_SOCKET)
        {
            fprintf(stderr, "%s socket creation failed %d\n", __FUNCTION__, errno);
            perror("OPEN SOCKET");
            ClearWinSock();
            return;
        }
        // CleanUp servaddr struct
        memset(&servaddr, 0, sizeof(servaddr));

        // assign IP, PORT
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        servaddr.sin_port = htons(SOCKET_PORT);

        if (connect(sockfd, (sockaddr *) &servaddr, sizeof(servaddr)) != 0)
        {
            sockfd = INVALID_SOCKET;
            fprintf(stderr, "%s connection with the server failed %d\n", __FUNCTION__, errno);
            perror("CONNECT SOCKET");
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            ClearWinSock();
            return;
        }
    }

    if (sockfd != INVALID_SOCKET)
    {
        int m_zero = 0;
        sprintf( shmCommand, "TRACK:%03d", m_zero );
        send(sockfd, shmCommand, strlen( shmCommand ), 0);
        // Do not hog the CPU for an housekeeping busy-loop
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        sprintf( shmCommand, "SIDE:%02d", m_zero );
        send(sockfd, shmCommand, strlen( shmCommand ), 0);
        // Do not hog the CPU for an housekeeping busy-loop
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        sprintf( shmCommand, "STATUS:%02d", m_zero );
        send(sockfd, shmCommand, strlen( shmCommand ), 0);
        // Do not hog the CPU for an housekeeping busy-loop
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        sprintf( shmCommand, "ERROR:%01d", m_zero );
        send(sockfd, shmCommand, strlen( shmCommand ), 0);
        // Do not hog the CPU for an housekeeping busy-loop
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    //fprintf(stdout, "%s Exit: sockfd %d\n", __FUNCTION__, sockfd);
}


using namespace ArduinoFloppyReader;

// Error Handling. This must be done here, and the correct value will be written into the fError file...
ArduinoFloppyReader::DiagnosticResponse lastResponse = DiagnosticResponse::drOK;

ADFWriter writer;


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
        // Do not hog the CPU for an housekeeping busy-loop
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    userInputDone = false; // Valid for the next time
    return userInput;
}

static void update_error_file(int err)
{
    sprintf(shmCommand, "ERROR:%01d", err);
    if (sockfd != INVALID_SOCKET) send(sockfd, shmCommand, strlen( shmCommand ), 0);
    // Do not hog the CPU for an housekeeping busy-loop
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

static void update_gui_writing(int32_t currentTrack, DiskSurface currentSide)
{
    sprintf(shmCommand, "TRACK:%03d", currentTrack);
    if (sockfd != INVALID_SOCKET) send(sockfd, shmCommand, strlen( shmCommand ), 0);
    // Do not hog the CPU for an housekeeping busy-loop
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    sprintf(shmCommand, "SIDE:%02d", (currentSide == DiskSurface::dsUpper) ? 1 : 0);
    if (sockfd != INVALID_SOCKET) send(sockfd, shmCommand, strlen( shmCommand ), 0);
    // Do not hog the CPU for an housekeeping busy-loop
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

static void update_gui(int32_t currentTrack, DiskSurface currentSide, int32_t badSectorsFound)
{
    update_gui_writing(currentTrack, currentSide);
    sprintf(shmCommand, "STATUS:%02d", badSectorsFound);
    if (sockfd != INVALID_SOCKET) send(sockfd, shmCommand, strlen( shmCommand ), 0);
    // Do not hog the CPU for an housekeeping busy-loop
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

// Settings type
struct SettingName {
    const wchar_t* name;
    const char* description;
};

#define MAX_SETTINGS 5

// All Settings
const SettingName AllSettings[MAX_SETTINGS] = {
    {L"DISKCHANGE","Force DiskChange Detection Support (used if pin 12 is not connected to GND)"},
    {L"PLUS","Set DrawBridge Plus Mode (when Pin 4 and 8 are swapped for improved accuracy)"},
    {L"ALLDD","Force All Disks to be Detected as Double Density (faster if you don't need HD support)"},
    {L"SLOW","Use Slower Disk Seeking (for slow head-moving floppy drives - rare)"},
    {L"INDEX","Always Index-Align Writing to Disks (not recommended unless you know what you are doing)"}
};

// Stolen from https://stackoverflow.com/questions/11635/case-insensitive-string-comparison-in-c
// A wide-string case insensative compare of strings
bool iequals(const std::wstring& a, const std::wstring& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](wchar_t a, wchar_t b) {
        return tolower(a) == tolower(b);
    });
}
bool iequals(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char a, char b) {
        return tolower(a) == tolower(b);
    });
}


// Read an ADF file and write it to disk
void adf2Disk(const std::wstring& filename, bool verify, bool preComp, bool eraseFirst, bool hdMode) {
    bool isSCP = true;
    bool isIPF = false;
    bool writeFromIndex = true;

    const wchar_t* extension = wcsrchr(filename.c_str(), L'.');
    if (extension) {
        extension++;
        isSCP = iequals(extension, L"SCP");
        isIPF = iequals(extension, L"IPF");
    }

    if (isIPF) printf("\nWrite disk from IPF mode\n\n"); else
    if (isSCP) printf("\nWrite disk from SCP mode\n\n"); else {
        printf("\nWrite disk from ADF mode\n\n");
        if (!verify) printf("WARNING: It is STRONGLY recommended to write with verify support turned on.\r\n\r\n");
    }

    // Detect disk speed
    const ArduinoFloppyReader::FirmwareVersion v = writer.getFirwareVersion();

    if (((v.major == 1) && (v.minor >= 9)) || (v.major > 1)) {
        if ((!isSCP) && (!isIPF))
            if (writer.GuessDiskDensity(hdMode) != ArduinoFloppyReader::ADFResult::adfrComplete) {
                lastResponse = DiagnosticResponse::drMediaTypeMismatch;
                printf("unable to work out the density of the disk inserted or disk not inserted\n");
                return;
            }
    }

    ADFResult result;
    if (isIPF) {
        result = writer.IPFToDisk(filename, eraseFirst, [](const int32_t currentTrack, const DiskSurface currentSide, bool isVerifyError, const CallbackOperation operation) ->WriteResponse {
            if (isVerifyError) {
                char input;
                do {
                    printf("\rDisk write verify error on track %i, %s side. [R]etry, [S]kip, [A]bort?                                              ", currentTrack, (currentSide == DiskSurface::dsUpper) ? "Upper" : "Lower");
#ifdef __USE_GUI__
                    update_error_file(1); // Set error for ask user
                    // This is not an error! I need to zero the badSectorCount (status file)
                    update_gui( currentTrack, currentSide, 1 ); // Set status flag for that track
                    fprintf(stderr, "\r\n %s WRITE ERROR NOW WAIT USER INPUT\r\n", __PRETTY_FUNCTION__);
                    fflush(stderr);
                    input = wait_user_input(); // This locks the program flow as the toupper() function
                    update_error_file(0); // Clear error for next
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
            update_gui(currentTrack, currentSide, 0);
#else
            printf("\nWriting Track %i, %s side     ", currentTrack, (currentSide == DiskSurface::dsUpper) ? "Upper" : "Lower");
#endif
#ifndef _WIN32
            fflush(stdout);
#endif
            return WriteResponse::wrContinue;
        });
    }
    else
    if (isSCP) {
        result = writer.SCPToDisk(filename, eraseFirst, [](const int currentTrack, const DiskSurface currentSide, bool isVerifyError, const CallbackOperation operation) ->WriteResponse {
            if (isVerifyError) {
                char input;
                do {
                    printf("\rDisk write verify error on track %i, %s side. [R]etry, [S]kip, [A]bort?                                   ", currentTrack, (currentSide == DiskSurface::dsUpper) ? "Upper" : "Lower");
#ifdef __USE_GUI__
                    update_error_file(1); // Set error for ask user
                    // This is not an error! I need to zero the badSectorCount (status file)
                    update_gui( currentTrack, currentSide, 1 ); // Set status flag for that track
                    fprintf(stderr, "\r\n %s WRITE ERROR NOW WAIT USER INPUT\r\n", __PRETTY_FUNCTION__);
                    fflush(stderr);
                    input = wait_user_input(); // This locks the program flow as the toupper() function
                    update_error_file(0); // Clear error for next
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
            update_gui(currentTrack, currentSide, 0);
#else
            printf("\rWriting Track %i, %s side     ", currentTrack, (currentSide == DiskSurface::dsUpper) ? "Upper" : "Lower");
#endif
#ifndef _WIN32
            fflush(stdout);
#endif
            return WriteResponse::wrContinue;
            });
    } else {
        result = writer.ADFToDisk(filename, hdMode, verify, preComp, eraseFirst, writeFromIndex, [](const int32_t currentTrack, const DiskSurface currentSide, bool isVerifyError, const CallbackOperation operation) ->WriteResponse {
            if (isVerifyError) {
                char input;
                do {
    #ifdef __USE_GUI__
                    update_error_file(1); // Set error for ask user
                    // This is not an error! I need to zero the badSectorCount (status file)
                    update_gui( currentTrack, currentSide, 1 ); // Set status flag for that track
                    fprintf(stderr, "\r\n %s WRITE ERROR NOW WAIT USER INPUT\r\n", __PRETTY_FUNCTION__);
                    fflush(stderr);
                    input = wait_user_input(); // This locks the program flow as the toupper() function
                    update_error_file(0); // Clear error for next
    #else
                    printf("\rDisk write verify error on track %i, %s side. [R]etry, [S]kip, [A]bort?                                   ", currentTrack, (currentSide == DiskSurface::dsUpper) ? "Upper" : "Lower");
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
            update_gui(currentTrack, currentSide, 0);
    #else
            printf("\rWriting Track %i, %s side     ", currentTrack, (currentSide == DiskSurface::dsUpper) ? "Upper" : "Lower");
    #endif
    #ifndef _WIN32
            fflush(stdout);
    #endif
            return WriteResponse::wrContinue;
        });
    }

	switch (result) {
    case ADFResult::adfrBadSCPFile:				    printf("\nBad, invalid or unsupported SCP file                                               "); lastResponse = DiagnosticResponse::drError; break;
    case ADFResult::adfrComplete:					printf("\rADF file written to disk                                                           "); lastResponse = DiagnosticResponse::drOK; break;
    case ADFResult::adfrExtendedADFNotSupported:	printf("\nExtended ADF files are not currently supported.                                    "); lastResponse = DiagnosticResponse::drMediaTypeMismatch; break;
    case ADFResult::adfrMediaSizeMismatch:			if (isIPF)
                                                        printf("\nIPF writing is only supported for DD disks and images                          "); else
                                                    if (isSCP)
                                                        printf("\nSCP writing is only supported for DD disks and images                             "); else
                                                    if (hdMode)
                                                        printf("\nDisk in drive was detected as HD, but a DD ADF file supplied.                      "); else
                                                        printf("\nDisk in drive was detected as DD, but an HD ADF file supplied.                     ");
                                                    lastResponse = DiagnosticResponse::drMediaTypeMismatch;
                                                    break;
    case ADFResult::adfrFirmwareTooOld:             printf("\nCannot write this file, you need to upgrade the firmware first.                    "); lastResponse = DiagnosticResponse::drOldFirmware; break;
    case ADFResult::adfrCompletedWithErrors:		printf("\rADF file written to disk but there were errors during verification                 "); lastResponse = DiagnosticResponse::drOK; break;
    case ADFResult::adfrAborted:					printf("\rWriting ADF file to disk                                                           "); lastResponse = DiagnosticResponse::drOK; break;
    case ADFResult::adfrFileError:					printf("\rError opening ADF file.                                                            "); lastResponse = DiagnosticResponse::drError; break;
    case ADFResult::adfrIPFLibraryNotAvailable:		printf("\nIPF CAPSImg from Software Preservation Society Library Missing                     "); lastResponse = DiagnosticResponse::drErrorMalformedVersion; break;
    case ADFResult::adfrDriveError:					printf("\rError communicating with the Arduino interface.                                    ");
                                                    printf("\n%s                                                  ", writer.getLastError().c_str()); lastResponse = DiagnosticResponse::drError; break;
    case ADFResult::adfrDiskWriteProtected:			printf("\rError, disk is write protected!                                                    "); lastResponse = DiagnosticResponse::drWriteProtected; break;
    default:										printf("\rAn unknown error occured                                                           "); lastResponse = DiagnosticResponse::drError; break;
	}
    writer.closeDevice();
}


// Read a disk and save it to ADF or SCP files
void disk2ADF(const std::wstring& filename, int numTracks, bool hdMode) {
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
            lastResponse = DiagnosticResponse::drOldFirmware;
			return;
		}
		else {
			// improved disk timings in 1.8, so make them aware
			printf("Rob strongly recommends updating the firmware on your Arduino to at least V1.8.\n");
			printf("That version is even better at reading old disks.\n");
		}
	}

//    bool hdMode = false;

    auto callback = [isADF, hdMode](const int32_t currentTrack, const DiskSurface currentSide, const int32_t retryCounter, const int32_t sectorsFound, const int32_t badSectorsFound, const int totalSectors, const CallbackOperation operation) ->WriteResponse {
		if (retryCounter > 20) {
			char input;
			do {
#ifdef __USE_GUI__
                    // Now we have to inform the GUI Thread we have an error, and we need to decide how to proceed
                    update_error_file(1); // set error flag
                    fprintf(stderr, "\r\n %s READ ERROR NOW WAIT USER INPUT \r\n", __PRETTY_FUNCTION__);
                    fflush(stderr);
                    input = wait_user_input(); // This locks the program flow as getch()
                    update_error_file(0); // clear error flag for next one
#else
                    printf("\rDisk has checksum errors/missing data.  [R]etry, [I]gnore, [A]bort?                                      ");
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
        update_gui( currentTrack, currentSide, badSectorsFound );
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
    case ADFResult::adfrComplete:					printf("\rFile created successfully.                                                     "); lastResponse = DiagnosticResponse::drOK; break;
    case ADFResult::adfrAborted:					printf("\rFile aborted.                                                                  "); lastResponse = DiagnosticResponse::drOK; break;
    case ADFResult::adfrFileError:					printf("\rError creating file.                                                           "); lastResponse = DiagnosticResponse::drError; break;
    case ADFResult::adfrFileIOError:				printf("\rError writing to file.                                                         "); lastResponse = DiagnosticResponse::drError; break;
    case ADFResult::adfrFirmwareTooOld:			    printf("\rThis requires firmware V1.8 or newer.                                          "); lastResponse = DiagnosticResponse::drOldFirmware; break;
    case ADFResult::adfrCompletedWithErrors:		printf("\rFile created with partial success.                                             "); lastResponse = DiagnosticResponse::drOK; break;
	case ADFResult::adfrDriveError:					printf("\rError communicating with the Arduino interface.                                ");
                                                    printf("\n%s                                                  ", writer.getLastError().c_str()); lastResponse = DiagnosticResponse::drError; break;
    default: 										printf("\rAn unknown error occured.                                                      "); lastResponse = DiagnosticResponse::drError; break;
	}
    writer.closeDevice();
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

int wmain(QStringList list)
{
    bool writeMode = list.contains("WRITE");
    bool verify = list.contains("VERIFY");
    bool preComp = list.contains("PRECOMP");
    bool eraseBeforeWrite = list.contains("ERASEBEFOREWRITE");
    bool num82Tracks = list.contains("TRACKSNUM82");
    int numTracks = 80;
    bool isOpened = false;
    bool isHDMode = list.contains("HD");

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
    qDebug() << "HD" << isHDMode;

    userInputDone = false; // It will be changed by the user on errors

    if (list.contains("DIAGNOSTIC")) {
            runDiagnostics(port);
    } else {
        if (!writer.openDevice(port)) {
			printf("\rError opening COM port: %s  ", writer.getLastError().c_str());
		}
		else {
            setupSocketClient();
            isOpened = true;
            if (writeMode) {
                adf2Disk(filename.c_str(), verify, preComp, eraseBeforeWrite, isHDMode);
            } else {
                disk2ADF(filename.c_str(), numTracks, isHDMode);
                lastResponse = writer.getLastErrorCode();
            }
		}
	}
    if (isOpened) writer.closeDevice();

    int globalError;
    switch (lastResponse)
    {
    case DiagnosticResponse::drOK: globalError = 0; break;
        // Responses from openPort
    case DiagnosticResponse::drPortInUse: globalError = 1; break;
    case DiagnosticResponse::drPortNotFound: globalError = 2; break;
    case DiagnosticResponse::drPortError: globalError = 3; break;
    case DiagnosticResponse::drAccessDenied: globalError = 4; break;
    case DiagnosticResponse::drComportConfigError: globalError = 5; break;
    case DiagnosticResponse::drBaudRateNotSupported: globalError = 6; break;
    case DiagnosticResponse::drErrorReadingVersion: globalError = 7; break;
    case DiagnosticResponse::drErrorMalformedVersion: globalError = 8; break;
    case DiagnosticResponse::drOldFirmware: globalError = 9; break;
        // Responses from commands
    case DiagnosticResponse::drSendFailed: globalError = 10; break;
    case DiagnosticResponse::drSendParameterFailed: globalError = 11; break;
    case DiagnosticResponse::drReadResponseFailed: globalError = 12; break;
    case DiagnosticResponse::drWriteTimeout: globalError = 13; break;
    case DiagnosticResponse::drSerialOverrun: globalError = 14; break;
    case DiagnosticResponse::drFramingError: globalError = 15; break;
    case DiagnosticResponse::drError: globalError = 16; break;

        // Response from selectTrack
    case DiagnosticResponse::drTrackRangeError: globalError = 17; break;
    case DiagnosticResponse::drSelectTrackError: globalError = 18; break;
    case DiagnosticResponse::drWriteProtected: globalError = 19; break;
    case DiagnosticResponse::drStatusError: globalError = 20; break;
    case DiagnosticResponse::drSendDataFailed: globalError = 21; break;
    case DiagnosticResponse::drTrackWriteResponseError: globalError = 22; break;

        // Returned if there is no disk in the drive
    case DiagnosticResponse::drNoDiskInDrive: globalError = 23; break;

    case DiagnosticResponse::drDiagnosticNotAvailable: globalError = 24; break;
    case DiagnosticResponse::drUSBSerialBad: globalError = 25; break;
    case DiagnosticResponse::drCTSFailure: globalError = 26; break;
    case DiagnosticResponse::drRewindFailure: globalError = 27; break;
    case DiagnosticResponse::drMediaTypeMismatch: globalError = 28; break;
    default: qDebug() << "UNKONWN ERROR. PLEASE DEBUG"; globalError = 99; break;
    }

    qDebug() << "GLOBAL ERROR: " << globalError << "IS OPENED" << isOpened;
    if (!isOpened) {
        globalError = 3;
    }
    if (sockfd != INVALID_SOCKET)
    {
#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif
        sockfd = INVALID_SOCKET;
        ClearWinSock();
    }
    printf("\r\n\r\n");
    return globalError;
}

