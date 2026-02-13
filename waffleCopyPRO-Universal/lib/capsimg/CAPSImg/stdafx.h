// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// standard C/C++ includes
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <vector>

// Windows/MSVC specific includes
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
// Don't include windows.h here if CAPS_USER is defined (static linking)
// CommonTypes.h will handle it appropriately
#ifdef _MSC_VER
#include <io.h>
#include <direct.h>
#ifndef CAPS_USER
#include <crtdbg.h>
#define _CRTDBG_MAP_ALLOC
#endif
#endif
// Use compatibility dirent.h on Windows
#include <dirent.h>
#else
// Unix/Linux includes
#include <dirent.h>
#endif

// Core definitions required on all platforms
#include "CommonTypes.h"
#include "BaseFile.h"
#include "DiskFile.h"
#include "MemoryFile.h"
#include "CRC.h"
#include "BitBuffer.h"

#if !defined(_WIN32) || defined(CAPSIMG_STANDALONE)
//-- Linux changes
#include <stddef.h>			// offsetof
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>			// localtime
#ifndef MAX_PATH
#define MAX_PATH ( 260 )
#endif
#ifndef __cdecl
#define __cdecl
#endif
#define _lrotl(x,n) (((x) << (n)) | ((x) >> (sizeof(x)*8-(n))))
#define _lrotr(x,n) (((x) >> (n)) | ((x) << (sizeof(x)*8-(n))))
typedef const char *LPCSTR;
typedef const char *LPCTSTR;
//-- Linux changes


#define INTEL
#ifndef MAX_FILENAMELEN
#define MAX_FILENAMELEN (MAX_PATH*2)
#endif

// external definitions
#include "CommonTypes.h"

// Core components
#include "BaseFile.h"
#include "DiskFile.h"
#include "MemoryFile.h"
#include "CRC.h"
#include "BitBuffer.h"

// IPF library public definitions
#include "CapsLibAll.h"

// CODECs
#include "DiskEncoding.h"
#include "CapsDefinitions.h"
#include "CTRawCodec.h"

// file support
#include "CapsFile.h"
#include "DiskImage.h"
#include "CapsLoader.h"
#include "CapsImageStd.h"
#include "CapsImage.h"
#include "StreamImage.h"
#include "StreamCueImage.h"
#include "DiskImageFactory.h"

// Device access
#include "C2Comm.h"

// system
#include "CapsCore.h"
#include "CapsFDCEmulator.h"
#include "CapsFormatMFM.h"


//-- Linux/Unix compatibility macros
#define _access access
#ifndef __MINGW32__
#define _mkdir(x) mkdir(x,0)
#else
#define _mkdir(x) mkdir(x)
#endif
#define d_namlen d_reclen
#ifndef _MSC_VER
#define __assume(cond) do { if (!(cond)) __builtin_unreachable(); } while (0)
#endif
#ifndef min
#define min(x, y) (((x) < (y)) ? (x) : (y))
#endif

typedef struct _SYSTEMTIME {
        WORD wYear;
        WORD wMonth;
        WORD wDayOfWeek;
        WORD wDay;
        WORD wHour;
        WORD wMinute;
        WORD wSecond;
        WORD wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;

static inline void GetLocalTime(LPSYSTEMTIME lpSystemTime) {
    time_t t = time(NULL);
    struct tm lt;
#if defined(_MSC_VER)
    // MSVC uses localtime_s with different parameter order
    localtime_s(&lt, &t);
#elif defined(_WIN32) || defined(__MINGW32__)
    // MinGW uses localtime_s with swapped parameters
    struct tm tmp;
    localtime_s(&tmp, &t);
    lt = tmp;
#else
    // POSIX uses localtime_r
    localtime_r(&t, &lt);
#endif
    lpSystemTime->wYear = lt.tm_year + 1900;
    lpSystemTime->wMonth = lt.tm_mon + 1;
    lpSystemTime->wDayOfWeek = lt.tm_wday;
    lpSystemTime->wDay = lt.tm_mday;
    lpSystemTime->wHour = lt.tm_hour;
    lpSystemTime->wMinute = lt.tm_min;
    lpSystemTime->wSecond = lt.tm_sec;
    lpSystemTime->wMilliseconds = 0;
}
#endif

#ifdef CAPSIMG_STANDALONE
// When building standalone (Makefile) ensure CODEC headers are available
#include "CapsAPI.h"
#include "CapsDefinitions.h"
#include "CTRawCodec.h"
#endif

//-- Linux changes


