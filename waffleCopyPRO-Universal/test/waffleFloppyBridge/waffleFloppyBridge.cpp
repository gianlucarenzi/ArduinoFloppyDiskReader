//============================================================================
// Name        : waffleFloppyBridge.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include "ArduinoInterface.h"
#include "debug.h"
#include <vector>
#include <algorithm>
#include <assert.h>
#include <sstream>
#include <string>
#include <codecvt>
#include <thread>
#include <fstream>
#include <iostream>
#include <stdint.h>
#ifndef _WIN32
#include <string.h>
#include <cstring>
#endif

static int debuglevel = DBG_NOISY;

#ifdef _WIN32
#include <conio.h>
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

using namespace ArduinoFloppyReader;

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


void internalListSettings(ArduinoFloppyReader::ArduinoInterface& io) {
	for (int a = 0; a < MAX_SETTINGS; a++) {
		bool value = false;;
		switch (a) {
		case 0:io.eeprom_IsAdvancedController(value); break;
		case 1:io.eeprom_IsDrawbridgePlusMode(value); break;
		case 2:io.eeprom_IsDensityDetectDisabled(value); break;
		case 3:io.eeprom_IsSlowSeekMode(value); break;
		case 4:io.eeprom_IsIndexAlignMode(value); break;
		}
		DBG_V("[%s] %ls \t%s\n", value ? "X" : " ", AllSettings[a].name, AllSettings[a].description);
	}
}

void listSettings(const std::wstring& port) {
	ArduinoFloppyReader::ArduinoInterface io;
	DBG_N("Attempting to read settings from device on port: %ls\n\n", port.c_str());

	ArduinoFloppyReader::DiagnosticResponse resp = io.openPort(port);
	if (resp != ArduinoFloppyReader::DiagnosticResponse::drOK) {
		DBG_E("Unable to connect to device: %s\n", io.getLastErrorStr().c_str());
		return;
	}

	ArduinoFloppyReader::FirmwareVersion version = io.getFirwareVersion();
	if ((version.major == 1) && (version.minor < 9)) {
		DBG_E("This feature requires firmware V1.9\n");
		return;
	}

	internalListSettings(io);
}

void programmeSetting(const std::wstring& port, const std::wstring& settingName, const bool settingValue) {
	ArduinoFloppyReader::ArduinoInterface io;
	DBG_N("Attempting to set settings to device on port: %ls\n\n", port.c_str());
	ArduinoFloppyReader::DiagnosticResponse resp = io.openPort(port);
	if (resp != ArduinoFloppyReader::DiagnosticResponse::drOK) {
		DBG_E("Unable to connect to device: %s\n", io.getLastErrorStr().c_str());
		return;
	}

	ArduinoFloppyReader::FirmwareVersion version = io.getFirwareVersion();
	if ((version.major == 1) && (version.minor < 9)) {
		DBG_E("This feature requires firmware V1.9\n");
		return;
	}

	// See which one it is
	for (int a = 0; a < MAX_SETTINGS; a++) {
		std::wstring s = AllSettings[a].name;
		if (iequals(s,  settingName)) {

			switch (a) {
			case 0:io.eeprom_SetAdvancedController(settingValue); break;
			case 1:io.eeprom_SetDrawbridgePlusMode(settingValue); break;
			case 2:io.eeprom_SetDensityDetectDisabled(settingValue); break;
			case 3:io.eeprom_SetSlowSeekMode(settingValue); break;
			case 4:io.eeprom_SetIndexAlignMode(settingValue); break;
			}

			DBG_N("Settng Set.  Current settings are now:\n\n");
			internalListSettings(io);
			return;
		}
	}
	DBG_E("Setting %ls was not found.\n\n", settingName.c_str());
}


/**
// Read an ADF file and write it to disk
void adf2Disk(const std::wstring& filename, bool verify) {
	printf("\nWrite disk from ADF mode\n\n");
	if (!verify) printf("WARNING: It is STRONGLY recommended to write with verify support turned on.\r\n\r\n");

	ADFResult result = writer.ADFToDisk(filename,verify,true, [](const int32_t currentTrack, const DiskSurface currentSide, bool isVerifyError) ->WriteResponse {
		if (isVerifyError) {
			char input;
			do {
				printf("\rDisk write verify error on track %i, %s side. [R]etry, [S]kip, [A]bort?                                   ", currentTrack, (currentSide == DiskSurface::dsUpper) ? "Upper" : "Lower");
#ifdef _WIN32
				input = toupper(_getch());
#else
				input = toupper(_getChar());
#endif	
			} while ((input != 'R') && (input != 'S') && (input != 'A'));
			
			switch (input) {
			case 'R': return WriteResponse::wrRetry;
			case 'I': return WriteResponse::wrSkipBadChecksums;
			case 'A': return WriteResponse::wrAbort;
			}
		}
		printf("\rWriting Track %i, %s side     ", currentTrack, (currentSide == DiskSurface::dsUpper) ? "Upper" : "Lower");
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

 **/


#define MFM_MASK    0x55555555L		
#define AMIGA_WORD_SYNC  0x4489							 // Disk SYNC code for the Amiga start of sector
#define SECTOR_BYTES	512								 // Number of bytes in a decoded sector
#define NUM_SECTORS_PER_TRACK_DD 11						 // Number of sectors per track
#define NUM_SECTORS_PER_TRACK_HD 22						  // Same but for HD disks
#define RAW_SECTOR_SIZE (8+56+SECTOR_BYTES+SECTOR_BYTES)      // Size of a sector, *Including* the sector sync word longs
#define ADF_TRACK_SIZE_DD (SECTOR_BYTES*NUM_SECTORS_PER_TRACK_DD)   // Bytes required for a single track dd
#define ADF_TRACK_SIZE_HD (SECTOR_BYTES*NUM_SECTORS_PER_TRACK_HD)   // Bytes required for a single track hd


const char* TEST_BYTE_SEQUENCE = "amiga.robsmithdev.co.uk";

// Buffer to hold raw data for just a single sector
typedef unsigned char RawEncodedSector[RAW_SECTOR_SIZE];
typedef unsigned char RawDecodedSector[SECTOR_BYTES];
typedef RawDecodedSector RawDecodedTrackDD[NUM_SECTORS_PER_TRACK_DD];
typedef RawDecodedSector RawDecodedTrackHD[NUM_SECTORS_PER_TRACK_HD];
typedef unsigned char RawMFMData[SECTOR_BYTES + SECTOR_BYTES];

// When workbench formats a disk, it write 13630 bytes of mfm data to the disk.  So we're going to write this amount, and then we dont need an erase first
typedef struct alignas(1) {
	unsigned char filler1[1654];  // Padding at the start of the track.  This will be set to 0xaa
	// Raw sector data
	RawEncodedSector sectors[NUM_SECTORS_PER_TRACK_DD];   // 11968 bytes
	// Blank "Filler" gap content. (this may get overwritten by the sectors a little)
	unsigned char filler2[8];
} FullDiskTrackDD;

typedef struct alignas(1) {
	unsigned char filler1[1654];  // Padding at the start of the track.  This will be set to 0xaa
	// Raw sector data
	RawEncodedSector sectors[NUM_SECTORS_PER_TRACK_HD];   // 11968*2 bytes
	// Blank "Filler" gap content. (this may get overwritten by the sectors a little)
	unsigned char filler2[8];
} FullDiskTrackHD;

// Structure to hold data while we decode it
typedef struct alignas(8)  {
	unsigned char trackFormat;        // This will be 0xFF for Amiga
	unsigned char trackNumber;        // Current track number (this is actually (tracknumber*2) + side
	unsigned char sectorNumber;       // The sector we just read (0 to 11)
	unsigned char sectorsRemaining;   // How many more sectors remain until the gap (0 to 10)

	uint32_t sectorLabel[4];     // OS Recovery Data, we ignore this

	uint32_t headerChecksum;	  // Read from the header, header checksum
	uint32_t dataChecksum;		  // Read from the header, data checksum

	uint32_t headerChecksumCalculated;   // The header checksum we calculate
	uint32_t dataChecksumCalculated;     // The data checksum we calculate

	RawDecodedSector data;          // decoded sector data

	RawMFMData rawSector;   // raw track data, for analysis of invalid sectors
} DecodedSector;

// To hold a list of valid and checksum failed sectors
struct DecodedTrack {
	// A list of valid sectors where the checksums are OK
	std::vector<DecodedSector> validSectors;
	// A list of sectors found with invalid checksums.  These are used if ignore errors is triggered
	// We keep copies of each one so we can perform a statistical analysis to see if we can get a working one based on which bits are mostly set the same
	std::vector<DecodedSector> invalidSectors[NUM_SECTORS_PER_TRACK_HD];
};


// MFM decoding algorithm
// *input;	MFM coded data buffer (size == 2*data_size) 
// *output;	decoded data buffer (size == data_size) 
// Returns the checksum calculated over the data
uint32_t decodeMFMdata(const uint32_t* input, uint32_t* output, const unsigned int data_size) {
	uint32_t odd_bits, even_bits;
	uint32_t chksum = 0L;
	unsigned int count;

	// the decoding is made here long by long : with data_size/4 iterations 
	for (count = 0; count < data_size / 4; count++) {
		odd_bits = *input;					// longs with odd bits 
		even_bits = *(uint32_t*)(((unsigned char*)input) + data_size);   // longs with even bits - located 'data_size' bytes after the odd bits

		chksum ^= odd_bits;              // XOR Checksum
		chksum ^= even_bits;

		*output = ((even_bits & MFM_MASK) | ((odd_bits & MFM_MASK) << 1));
		input++;      /* next 'odd' long and 'even bits' long  */
		output++;     /* next location of the future decoded long */
	}
	return chksum & MFM_MASK;
}

// MFM encoding algorithm part 1 - this just writes the actual data bits in the right places
// *input;	RAW data buffer (size == data_size) 
// *output;	MFM encoded buffer (size == data_size*2) 
// Returns the checksum calculated over the data
uint32_t encodeMFMdataPart1(const uint32_t* input, uint32_t* output, const unsigned int data_size) {
	uint32_t chksum = 0L;
	unsigned int count;

	uint32_t* outputOdd = output;
	uint32_t* outputEven = (uint32_t*)(((unsigned char*)output) + data_size);

	// Encode over two passes.  First split out the odd and even data, then encode the MFM values, the /4 is because we're working in longs, not bytes
	for (count = 0; count < data_size / 4; count++) {
		*outputEven = *input & MFM_MASK;
		*outputOdd = ((*input)>>1) & MFM_MASK;
		outputEven++;
		outputOdd++;
		input++;
	}
	
	// Checksum calculator
	// Encode over two passes.  First split out the odd and even data, then encode the MFM values, the /4 is because we're working in longs, not bytes
	for (count = 0; count < (data_size / 4) * 2; count++) {
		chksum ^= *output;
		output++;
	}

	return chksum & MFM_MASK;
}

// Copys the data from inTrack into outTrack but fixes the bit/byte alignment so its aligned on the start of a byte 
void alignSectorToByte(const unsigned char* inTrack, const int dataLength, int byteStart, int bitStart, RawEncodedSector& outSector) {
	unsigned char byteOut = 0;
	unsigned int byteOutPosition = 0;

	// Bit counter output
	unsigned int counter = 0;

	// The position supplied is the last bit of the track sync.  
	bitStart--;   // goto the next bit
	if (bitStart < 0) {
		// Could do a MEMCPY here, but for now just let the code below run
		bitStart = 7;
		byteStart++;
	}
	byteStart -= 8;   // wind back 8 bytes

	// This is mis-aligned.  So we need to shift the data into byte boundarys
	for (;;) {
		for (int bitCounter = bitStart; bitCounter >= 0; bitCounter--) {
			byteOut <<= 1;
			if (inTrack[byteStart % dataLength] & (1 << bitCounter)) byteOut |= 1;

			if (++counter >= 8) {
				outSector[byteOutPosition] = byteOut;
				byteOutPosition++;
				if (byteOutPosition >= RAW_SECTOR_SIZE) return;
				counter = 0;
			}
		}

		// Move along and reset
		byteStart++;
		bitStart = 7;
	}
}

// Attempt to repair the MFM data.  Returns TRUE if errors are detected
bool repairMFMData(unsigned char* data, const unsigned int dataLength) {
	bool errors = false;
	// Only certain bit-patterns are allowed.  So if we come across an invalid one we will try to repair it.  
	// You cannot have two '1's together, and a max of three '0' in a row
	// Allowed combinations:  (note the SYNC WORDS and TRACK START are designed to break these rules, but we shouldn't encounter them)
	// 
	//	00010
	//	00100
	//	00101
	//	01000
	//	01001
	//	01010
	//	10001
	//	10010
	//	10100
	//	10101
	//
	unsigned char testByte = 0;
	int counter = 0;
	for (unsigned int position = 0; position < dataLength; position++) {
		// Fixed: This was the wrong way around
		for (int bitIndex = 7; bitIndex >= 0; bitIndex--) {
			testByte <<= 1;   // shift off one bit to make room for the new bit
			if (*data & (1 << bitIndex)) {
				// Make sure two '1's dont come in together as this is not allowed! This filters out a lot of BAD combinations
				if ((testByte & 0x2) != 0x2) {
					testByte |= 1;
				} 
				else {
					// We detected two '1's in a row, which isnt allowed.  From reading this most likely means this was a weak bit, so we change it to zero.
					errors = true;
				}
			} 

			// We're only interested in the last so many bits, and only when we have received that many
			if (++counter > 4) {
				switch (testByte & 0x1F) {
					// These are the only possible invalid combinations left
				case 0x00:
				case 0x01:
				case 0x10:
					// No idea how to repair these	
					errors = true;
					break;
				}
			}
		}
		data++;
	}

	return errors;
	 
}

// Looks at the history for this sector number and creates a new sector where the bits are set to whatever occurs more.  We then do a checksum and if it succeeds we use it
bool attemptFixSector(const DecodedTrack& decodedTrack, DecodedSector& outputSector) {
	int sectorNumber = outputSector.sectorNumber;

	if (decodedTrack.invalidSectors[sectorNumber].size() < 2) return false;

	typedef struct {
		int zeros = 0;
		int ones = 0;
	} SectorCounter[8];

	SectorCounter sectorSum[SECTOR_BYTES + SECTOR_BYTES];

	memset(sectorSum, 0, sizeof(sectorSum));

	// Calculate the number of '1's and '0's in each block
	for (const DecodedSector& sec : decodedTrack.invalidSectors[sectorNumber]) 
		for (int byteNumber = 0; byteNumber < SECTOR_BYTES + SECTOR_BYTES; byteNumber++) 
			for (int bit = 0; bit <= 7; bit++) 
				if (sec.rawSector[byteNumber] & (1 << bit))
					sectorSum[byteNumber][bit].ones++; else sectorSum[byteNumber][bit].zeros++;

	// Now create a sector based on this data
	memset(outputSector.rawSector, 0, sizeof(outputSector.rawSector));
	for (int byteNumber = 0; byteNumber < SECTOR_BYTES + SECTOR_BYTES; byteNumber++)
		for (int bit = 0; bit <= 7; bit++)
			if (sectorSum[byteNumber][bit].ones >= sectorSum[byteNumber][bit].zeros)
				outputSector.rawSector[byteNumber] |= (1 << bit);

	return true;
}

// Extract and convert the sector.  This may be a duplicate so we may reject it.  Returns TRUE if it was valid, or false if not
bool decodeSector(const RawEncodedSector& rawSector, const unsigned int trackNumber, bool isHD, const DiskSurface surface, DecodedTrack& decodedTrack, bool ignoreHeaderChecksum, int& lastSectorNumber) {
	DecodedSector sector;

	lastSectorNumber = -1;
	memcpy(sector.rawSector, rawSector, sizeof(RawMFMData));

	// Easier to operate on
	unsigned char* sectorData = (unsigned char*)rawSector;
 
	// Read the first 4 bytes (8).  This  is the track header data	
	sector.headerChecksumCalculated = decodeMFMdata((uint32_t*)(sectorData + 8), (uint32_t*)&sector, 4);
	// Decode the label data and update the checksum
	sector.headerChecksumCalculated ^= decodeMFMdata((uint32_t*)(sectorData + 16), (uint32_t*)&sector.sectorLabel[0], 16);
	// Get the checksum for the header
	decodeMFMdata((uint32_t*)(sectorData + 48), (uint32_t*)&sector.headerChecksum, 4);  // (computed on mfm longs, longs between offsets 8 and 44 == 2 * (1 + 4) longs)
	// If the header checksum fails we just cant trust anything we received, so we just drop it
	if ((sector.headerChecksum != sector.headerChecksumCalculated) && (!ignoreHeaderChecksum)) {
		return false;
	}

	// Check if the header contains valid fields
	if (sector.trackFormat != 0xFF) 
		return false;  // not valid
	if (sector.sectorNumber > (isHD ? 21 : 10))
		return false;
	if (sector.trackNumber > 162) 
		return false;   // 81 tracks * 2 for both sides
	if (sector.sectorsRemaining > (isHD ? 22 : 11))
		return false;  // this isnt possible either
	if (sector.sectorsRemaining < 1)
		return false;  // or this

	// And is it from the track we expected?
	const unsigned char targetTrackNumber = (trackNumber << 1) | ((surface == DiskSurface::dsUpper) ? 1 : 0);

	if (sector.trackNumber != targetTrackNumber) return false; // this'd be weird

	// Get the checksum for the data
	decodeMFMdata((uint32_t*)(sectorData + 56), (uint32_t*)&sector.dataChecksum, 4);
	

	// Lets see if we already have this one
	const int searchSector = sector.sectorNumber;
	auto index = std::find_if(decodedTrack.validSectors.begin(), decodedTrack.validSectors.end(), [searchSector](const DecodedSector& sector) -> bool {
		return (sector.sectorNumber == searchSector);
	});

	// We already have it as a GOOD VALID sector, so skip, we don't need it.
	if (index != decodedTrack.validSectors.end()) return true;

	// Decode the data and receive it's checksum
	sector.dataChecksumCalculated = decodeMFMdata((uint32_t*)(sectorData + 64), (uint32_t*)&sector.data[0], SECTOR_BYTES); // (from 64 to 1088 == 2*512 bytes)

	lastSectorNumber = sector.sectorNumber;

	// Is the data valid?
	if (sector.dataChecksum != sector.dataChecksumCalculated) {
		// Keep a copy
		decodedTrack.invalidSectors[sector.sectorNumber].push_back(sector);
		return false;
	}
	else {
		// Its a good sector, and we dont have it yet
		decodedTrack.validSectors.push_back(sector);

		// Lets delete it from invalid sectors list
		decodedTrack.invalidSectors[sector.sectorNumber].clear();


		return true;
	}
}

// Encode a sector into the correct format for disk
void encodeSector(const unsigned int trackNumber, const DiskSurface surface, bool isHD, const unsigned int sectorNumber, const RawDecodedSector& input, RawEncodedSector& encodedSector, unsigned char& lastByte) {
	// Sector Start
	encodedSector[0] = (lastByte & 1) ? 0x2A : 0xAA;
	encodedSector[1] = 0xAA;
	encodedSector[2] = 0xAA;
	encodedSector[3] = 0xAA;
	// Sector Sync
	encodedSector[4] = 0x44;
	encodedSector[5] = 0x89;
	encodedSector[6] = 0x44;
	encodedSector[7] = 0x89;

	// MFM Encoded header
	DecodedSector header;
	memset(&header, 0, sizeof(header));

	header.trackFormat = 0xFF;
	header.trackNumber = (trackNumber << 1) | ((surface == DiskSurface::dsUpper) ? 1 : 0);
	header.sectorNumber = sectorNumber; 
	header.sectorsRemaining = (isHD ? NUM_SECTORS_PER_TRACK_HD : NUM_SECTORS_PER_TRACK_DD) - sectorNumber;  //1..11
	
	
	header.headerChecksumCalculated = encodeMFMdataPart1((const uint32_t*)&header, (uint32_t*)&encodedSector[8], 4);
	// Then theres the 16 bytes of the volume label that isnt used anyway
	header.headerChecksumCalculated ^= encodeMFMdataPart1((const uint32_t*)&header.sectorLabel, (uint32_t*)&encodedSector[16], 16);
	// Thats 40 bytes written as everything doubles (8+4+4+16+16). - Encode the header checksum
	encodeMFMdataPart1((const uint32_t*)&header.headerChecksumCalculated, (uint32_t*)&encodedSector[48], 4);
	// And move on to the data section.  Next should be the checksum, but we cant encode that until we actually know its value!
	header.dataChecksumCalculated = encodeMFMdataPart1((const uint32_t*)&input, (uint32_t*)&encodedSector[64], SECTOR_BYTES);
	// And add the checksum
	encodeMFMdataPart1( (const uint32_t*)&header.dataChecksumCalculated, (uint32_t*)&encodedSector[56], 4);

	// Now fill in the MFM clock bits
	bool lastBit = encodedSector[7] & (1 << 0);
	bool thisBit = lastBit;

	// Clock bits are bits 7, 5, 3 and 1
	// Data is 6, 4, 2, 0
	for (int count = 8; count < RAW_SECTOR_SIZE; count++) {
		for (int bit = 7; bit >= 1; bit -= 2) {
			lastBit = thisBit;			
			thisBit = encodedSector[count] & (1 << (bit-1));
	
			if (!(lastBit || thisBit)) {
				// Encode a 1!
				encodedSector[count] |= (1 << bit);
			}
		}
	}

	lastByte = encodedSector[RAW_SECTOR_SIZE - 1];
}

// Find sectors within raw data read from the drive by searching bit-by-bit for the SYNC bytes
void findSectors(const unsigned char* track, bool isHD, unsigned int trackNumber, DiskSurface side, unsigned short trackSync, DecodedTrack& decodedTrack, bool ignoreHeaderChecksum)
{
	// Work out what we need to search for which is syncsync
	const uint32_t search = (trackSync | (((uint32_t)trackSync) << 16));

	// Prepare our test buffer
	uint32_t decoded = 0;

	// Keep runnign until we run out of data
	unsigned int byteIndex = 0;

	int nextTrackBitCount = 0;

	const unsigned int dataLength = isHD ? RAW_TRACKDATA_LENGTH_HD : RAW_TRACKDATA_LENGTH_DD;
	const int maxSectors = isHD ? NUM_SECTORS_PER_TRACK_HD : NUM_SECTORS_PER_TRACK_DD;
	DBG_N("Called\n");

	// run the entire track length
	while (byteIndex < dataLength) {

		// Check each bit, the "decoded" variable slowly slides left providing a 32-bit wide "window" into the bitstream
		for (int bitIndex = 7; bitIndex >= 0; bitIndex--) {
			decoded <<= 1;   // shift off one bit to make room for the new bit

			if (track[byteIndex] & (1 << bitIndex)) decoded |= 1;

			// Have we matched the sync words correctly
			++nextTrackBitCount;
			int lastSectorNumber = -1;
			if (decoded == search) {
				RawEncodedSector alignedSector;
				
				// We extract ALL of the track data from this BIT to byte align it properly, then pass it onto the code to read the sector (from the start of the sync code)
				alignSectorToByte(track, dataLength, byteIndex, bitIndex, alignedSector);

				// Now see if there's a valid sector there.  We now only skip the sector if its valid, incase rogue data gets in there
				if (decodeSector(alignedSector, trackNumber, isHD, side, decodedTrack, ignoreHeaderChecksum, lastSectorNumber)) {
					// We know the size of this buffer, so we can skip by exactly this amount
					byteIndex += RAW_SECTOR_SIZE - 8; // skip this many bytes as we know this is part of the track! minus 8 for the SYNC
					if (byteIndex >= dataLength) break;
					// We know that 8 bytes from here should be another track. - we allow 1 bit either way for slippage, but this allows an extra check incase the SYNC pattern is damaged
					nextTrackBitCount = 0;
				}
				else {

					// Decode failed.  Lets try a "homemade" one
					DecodedSector newTrack;
					if ((lastSectorNumber >= 0) && (lastSectorNumber < maxSectors)) {
						newTrack.sectorNumber = lastSectorNumber;
						if (attemptFixSector(decodedTrack, newTrack)) {
							memcpy(newTrack.rawSector, alignedSector, sizeof(newTrack.rawSector));
							// See if our makeshift data will decode or not
							if (decodeSector(alignedSector, trackNumber, isHD, side, decodedTrack, ignoreHeaderChecksum, lastSectorNumber)) {
								// We know the size of this buffer, so we can skip by exactly this amount
								byteIndex += RAW_SECTOR_SIZE - 8; // skip this many bytes as we know this is part of the track! minus 8 for the SYNC
								if (byteIndex >= dataLength) break;
							}
						}
					}
					if (decoded == search) nextTrackBitCount = 0;
				}
			}
		}
		byteIndex++;
	}
	DBG_N("Exit\n");
}

// Merges any invalid sectors into the valid ones as a last resort
void mergeInvalidSectors(DecodedTrack& track, bool isHD) {
	const int maxSectors = isHD ? NUM_SECTORS_PER_TRACK_HD : NUM_SECTORS_PER_TRACK_DD;

	for (unsigned char sector = 0; sector < maxSectors; sector++) {
		if (track.invalidSectors[sector].size()) {
			// Lets try to make the best sector we can
			DecodedSector sec = track.invalidSectors[sector][0];
			// Repair maybe!?
			attemptFixSector(track, sec);

			track.validSectors.push_back(sec);
		}
		track.invalidSectors[sector].clear();
	}
}

static int disk2ADF(ArduinoFloppyReader::ArduinoInterface &bridge, const bool inHDMode, const unsigned int numTracks, const std::wstring& port, const std::wstring& filename)
{
	const wchar_t* extension = wcsrchr(filename.c_str(), L'.');
	bool isADF = true;

	if (extension)
	{
		extension++;
		isADF = !iequals(extension, L"SCP");
	}

	if (isADF)
	{
		DBG_N("\nCreate ADF from disk mode\n\n");
	}
	else
	{
		DBG_N("\nCreate SCP file from disk mode\n\n");
	}

	if (numTracks > 82)
	{
		DBG_E("NumTracks MUST BE 82 at max!\n");
		return -1;
	}


//	for (;;)
	{
		DBG_I("LastError %s\n", bridge.getLastErrorStr());
		if (bridge.isOpen())
		{
			DBG_I("Port Ready\n");
			FirmwareVersion fw = bridge.getFirwareVersion();
			DBG_I("FW Ver: %d.%d FullControlMod: %s FLG1: %d - FLG2: %d - BUILD: %d\n",
				fw.major, fw.minor, fw.fullControlMod == 0 ? "NO" : "YES", fw.deviceFlags1,
				fw.deviceFlags2, fw.buildNumber);
			bool doDiskInDrive = false;
			if (fw.major >= 1)
			{
				if (fw.minor > 8)
				{
					doDiskInDrive = true;
					DBG_I("Disk present feature\n");
				}
				else
				{
					DBG_I("Disk present feature NOT PRESENT\n");
				}
			}
			else
			{
				DBG_E("Firmware too old\n");
				return -1;
			}

			if (doDiskInDrive)
			{
				if (bridge.isDiskInDrive())
				{
					DBG_I("Disk in drive\n");
				}
				else
				{
					DBG_E("Disk NOT in drive\n");
					return -2;
				}
			}

			if (bridge.setDiskCapacity(inHDMode) != DiagnosticResponse::drOK)
			{
				DBG_E("Cannot setDiskCapacity\n");
				//return -2;
			}
			else
			{
				DBG_N("SetDiskCapacity %s\n", inHDMode == true ? "HD Mode" : "DD Mode");
			}

			// To hold a raw track
			RawTrackDataHD data;
			DecodedTrack track;
			bool includesBadSectors = false;

			const unsigned int readSize = inHDMode ? sizeof(ArduinoFloppyReader::RawTrackDataHD) : sizeof(ArduinoFloppyReader::RawTrackDataDD);
			const unsigned int maxSectorsPerTrack = inHDMode ? NUM_SECTORS_PER_TRACK_HD : NUM_SECTORS_PER_TRACK_DD;

			DBG_I("READ DISK SIZE: %d and maxSectorPerTrack %d\n", readSize, maxSectorsPerTrack);
			for (int currentTrack = 0; currentTrack < numTracks; currentTrack++)
			{
				// The sequence is like in the Rob Smith library
				if (bridge.selectTrack(currentTrack) != DiagnosticResponse::drOK)
				{
					DBG_E("Cannot select track: %d\n", currentTrack);
					return -2;
				}
				else
				{
					DBG_N("Track %d selected\n", currentTrack);
				}
				usleep(100 * 1000L);
				// Now select the side
				for (unsigned int surfaceIndex = 0; surfaceIndex < 2; surfaceIndex++)
				{
					// Surface 
					const DiskSurface surface = (surfaceIndex == 1) ? DiskSurface::dsUpper : DiskSurface::dsLower;

					if (bridge.selectSurface(surface) != DiagnosticResponse::drOK)
					{
						DBG_E("Cannot select surface %s for track: %d\n",
							(surfaceIndex == 1) ? "DiskSurface::dsUpper" : "DiskSurface::dsLower",
							currentTrack);
						return -3;
					}
					else
					{
						DBG_N("Surface selected %s \n", (surfaceIndex == 1) ? "DiskSurface::dsUpper" : "DiskSurface::dsLower");
					}
					usleep(100 * 1000L);

					// Reset the sectors list
					DBG_N("Cleaning invalid sectors\n");
					for (unsigned int sector=0; sector< maxSectorsPerTrack; sector++)
						track.invalidSectors[sector].clear();

					DBG_N("Cleaning valid sectors\n");
					track.validSectors.clear();
					// Extract phase code

					int failureTotal = 0;
					bool ignoreChecksums = false;

					DBG_N("Searching for sectors...\n");
					// Repeat until we have all 11 sectors
					while (track.validSectors.size() < maxSectorsPerTrack)
					{
						DBG_N("Track.validSectors.size() %ld -- maxSectorsPerTrack\n",
							track.validSectors.size(), maxSectorsPerTrack);
						DBG_N("readingCurrentTrack()...\n");
						if (bridge.readCurrentTrack(data, readSize, false) == DiagnosticResponse::drOK)
						{
							findSectors(data, inHDMode, currentTrack, surface, AMIGA_WORD_SYNC, track, ignoreChecksums);
							failureTotal++;
						}
						else
						{
							DBG_E("ADFResult ADFRDRIVE ERROR on reading Track\n");
							return -4;
						}

						// If the user wants to skip invalid sectors and save them
						if (ignoreChecksums)
						{
							for (unsigned int sector = 0; sector < maxSectorsPerTrack; sector++)
							{
								if (track.invalidSectors[sector].size())
								{
									includesBadSectors = true;
									break;
								}
							}
							mergeInvalidSectors(track, inHDMode);
						}
						usleep(100 * 1000L);
					}
					// Sort the sectors in order
					std::sort(track.validSectors.begin(), track.validSectors.end(), [](const DecodedSector & a, const DecodedSector & b) -> bool {
						return a.sectorNumber < b.sectorNumber;
					});

					// Now write all of them to disk
					for (unsigned int sector = 0; sector < maxSectorsPerTrack; sector++)
					{
						try
						{
							//hADFFile.write((const char*)track.validSectors[sector].data, 512);
						}
						catch (...)
						{
							//hADFFile.close();
							DBG_E("Error on writing ADF file\n");
							return -5;
						}
					}
				}
			}
			DBG_I("DISK READ OK\n");
			return 0;
		}
		else
		{
			DBG_I("Port not Open\n");
			return -1;
		}
	}
	return -1;
}


static int runDiagnostics(ArduinoFloppyReader::ArduinoInterface& bridge, const std::wstring& port)
{
	DBG_N("Called\n");

	ArduinoFloppyReader::DiagnosticResponse r = bridge.openPort(port);
	if (r != DiagnosticResponse::drOK)
	{
		DBG_E("\rError opening COM port %ls\n", port.c_str());
		return -1;
	}
	else
	{
		DBG_N("Open OK\n");
		FirmwareVersion fw = bridge.getFirwareVersion();

		DBG_I("FW Ver: %d.%d FullControlMod: %s FLG1: %d - FLG2: %d - BUILD: %d\n",
		fw.major, fw.minor, fw.fullControlMod == 0 ? "NO" : "YES", fw.deviceFlags1,
		fw.deviceFlags2, fw.buildNumber);

		bool doDiskInDrive = false;

		if (fw.major >= 1)
		{
			if (fw.minor > 8)
			{
				doDiskInDrive = true;
				DBG_I("Disk present feature\n");
			}
			else
			{
				DBG_I("Disk present feature NOT PRESENT\n");
			}
		}
		else
		{
			DBG_E("Firmware too old\n");
			return -1;
		}

		// Step 2: This has been setup with CTS stuff disabled, but the port opened.  So lets validate CTS *is* working as we expect it to
		r = bridge.testCTS();
		if (r != DiagnosticResponse::drOK)
		{
			DBG_E("Please check the following PINS on Waffle: A2\n");
			return -1;
		}
		else
		{
			DBG_I("CTS pin is working (A2)\n");
		}
		// Step 3: CTS is working correctly, so re-open the port in normal mode
		bridge.closePort();
		r = bridge.openPort(port);
		if (r != DiagnosticResponse::drOK)
		{
			DBG_E("\rError opening (2) COM port %ls\n", port.c_str());
			return -1;
		}
		else
		{
			DBG_N("REOPENING the port OK\n");
		}
		DBG_I("Transfer Speed TEST\n");
		if (bridge.testTransferSpeed() != DiagnosticResponse::drOK)
		{
			DBG_E("The USB->Serial adapter in use is not suitable.\n");
			DBG_E("Arduino UNO: The on-board adapter is not able to sustain large data transfers\n");
			DBG_E("Arduino Pro Mini: The USB-Serial board is not able to sustain large data transfers\n");
			DBG_E("Arduino Nano: The USB-Serial on this board is not fast enough to sustain large data transfers\n");
			DBG_E("If you are using any of the devices with a CH340 converter then swap it for an FTDI one.\n");
			DBG_E("If you still have problems after switching, connect the Arduino using a shorter cable\n");
			DBG_E("and if possible directly (ie: not via a USB hub)\n");
			DBG_E("Diagnostics failed.\n");
			return -1;
		}
		else
		{
			DBG_I("Read speed test passed. USB to serial converter is functioning correctly!\n");
		}
		DBG_I("Testing write-protect signal\n");
		DBG_I("Inserted disk is not write protected. If it is, then check Waffle Pin A0. Please insert a write protected AmigaDOS disk in drive\n");
		for (;;)
		{
			if (bridge.checkIfDiskIsWriteProtected(true) == DiagnosticResponse::drWriteProtected)
				break;
			usleep(1000L * 1500);
		}
		
		int testFail = 0;
		// Functions to test
		DBG_I("Enabling the drive (please listen and watch the drive)\n");
//		r = bridge.enableReading(true, false);
//		if (r != DiagnosticResponse::drOK)
//		{
//			DBG_E("Error: %s\n", bridge.getLastErrorStr());
//			testFail++;
//		}
		DBG_I("Asking Waffle to find Track 0\n");
//		r = bridge.findTrack0();
		r = bridge.selectTrack(10);
		if (r != DiagnosticResponse::drRewindFailure)
		{
			DBG_E("Unable to find track 0\n");
			DBG_E("Please check pins 6,8 and 7\n");
			return -1;
		}
		DBG_I("Track 0 was found. Asking the Waffle to find track 70\n");
		r = bridge.selectTrack(70);
		if (r != DiagnosticResponse::drOK)
		{
			DBG_E("Head is not moving. Maybe 5V USB issue?\n");
			DBG_E("Or check out pins 6 and 7\n");
			return -1;
		}
		if (fw.fullControlMod)
		{
			bridge.selectTrack(3);
			std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
			DBG_I("Testing Disk Change pin.*** Please remove disk from drive *** (you have 30 seconds)\n");
			while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < 30000)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
				if (bridge.checkForDisk(true) == DiagnosticResponse::drNoDiskInDrive)
					break;
			}

			if (bridge.isDiskInDrive())
			{
				DBG_E("Disk change is NOT working correctly, Please check the following pins on the Waffle: 10, 11\n");
				return -1;
			}

			start = std::chrono::steady_clock::now();
			DBG_I("*** Please re-insert disk into drive *** (you have 30 seconds)\n");
			while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < 30000)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(2000));
				bridge.selectTrack(2);
				if (bridge.isDiskInDrive())
					break;
				std::this_thread::sleep_for(std::chrono::milliseconds(2000));
				bridge.selectTrack(3);
				if (bridge.isDiskInDrive())
					break;
			}

			if (bridge.isDiskInDrive())
			{
				DBG_I("Disk change is working correctly\n");
			}
			else
			{
				DBG_E("Disk change is NOT working correctly. Please check the following pins on the Waffle: 10, 11\n");
				return -1;
			}
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	ArduinoFloppyReader::ArduinoInterface bridge;

	DBG_I("Detected Serial Devices:\r\n");
	std::vector<std::wstring> portList;
	ArduinoFloppyReader::ArduinoInterface::enumeratePorts(portList);
	for (const std::wstring& port : portList) {
		DBG_I("    %ls\n", port.c_str());
	}

#ifdef _WIN32
	std::wstring port = argv[1];
	int i = _wtoi(argv[1]);
	if (i) port = L"COM" + std::to_wstring(i); else port = argv[1];
#else
	std::wstring port = atw(argv[1]);
#endif

#ifdef _WIN32
	bool eepromMode = (argc > 2) && (iequals(argv[2], L"SETTINGS"));
#else
	bool eepromMode = (argc > 2) && (iequals(argv[2], "SETTINGS"));
#endif

	if (eepromMode) {

		if (argc >= 4) {
#ifdef _WIN32
			bool settingValue = iequals(argv[4], L"1");
			std::wstring settingName = argv[3];
			std::wstring port = argv[1];
			int i = _wtoi(argv[1]);
			if (i) port = L"COM" + std::to_wstring(i); else port = argv[1];
#else
			bool settingValue = iequals(argv[4], "1");
			std::wstring settingName = atw(argv[3]);
			std::wstring port = atw(argv[1]);
#endif
			programmeSetting(port, settingName, settingValue);
			return 0;
		}

		listSettings(port);
		return 0;
	}

#ifdef _WIN32
	bool writeMode = (argc > 3) && (iequals(argv[3], L"WRITE"));
	bool verify = (argc > 4) && (iequals(argv[4], L"VERIFY"));
	if (argc >= 2) {
		std::wstring port = argv[1];
		std::wstring filename = argv[2];
#else
	bool writeMode = (argc > 3) && (iequals(argv[3], "WRITE"));
	bool verify = (argc > 4) && (iequals(argv[4], "VERIFY"));
	if (argc >= 2) {
		std::wstring filename = atw(argv[2]);
#endif
		if (iequals(filename, L"DIAGNOSTIC"))
		{
			DBG_N("RUN DIAGNOSTIC on port: %ls\n", port.c_str());
			runDiagnostics(bridge, port);
		}
		else
		{
			ArduinoFloppyReader::DiagnosticResponse rval = bridge.openPort(port);
			if (rval != DiagnosticResponse::drOK)
			{
				DBG_E("\rError opening COM port %ls\n", port.c_str());
			}
			else
			{
				DBG_N("Open OK\n");
				if (writeMode)
				{
					DBG_N("adf2Disk PORT: %ls WRITE: %ls VERIFY: %s\n", port.c_str(), filename.c_str(), verify ? "VERIFY" : "NO VERIFY");
				}
				else
				{
					DBG_N("disk2ADF PORT: %ls READ: %ls\n", port.c_str(), filename.c_str());
					bool inHDmode = false;
					disk2ADF(bridge, inHDmode, 80, port, filename);
				}
			}
		}
	}
	DBG_N("Exiting Program\n");
	return EXIT_SUCCESS;
}
