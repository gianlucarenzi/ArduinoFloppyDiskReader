#include "debugmsg.h"
#include <QString>
// Simple caps plug-in support on Win32, should fit most users
//
// You may want to link with the libray instead, if your product fully complies
// with the license.
// If using the library directly without the plug-in, define CAPS_USER, and include CapsLib.h
// functions are the same with the plug-in, but their name start with "CAPS", not "Caps"
// CAPSInit does not have any parameters, CapsInit gets the library name
//
// Comment out stdafx.h if your project does not use the MSVC precompiled headers
//
// www.caps-project.org
//
// Modified for basic cross-platform support

#include "Comtype.h"
#include "CapsAPI.h"
#include "CapsPlug.h"
#ifdef CAPS_USER
extern "C" {
    SDWORD CAPSInit();
    SDWORD CAPSExit();
    SDWORD CAPSAddImage();
    SDWORD CAPSRemImage(SDWORD id);
    SDWORD CAPSLockImage(SDWORD id, PCHAR name);
    SDWORD CAPSLockImageMemory(SDWORD id, PUBYTE buffer, UDWORD length, UDWORD flag);
    SDWORD CAPSUnlockImage(SDWORD id);
    SDWORD CAPSLoadImage(SDWORD id, UDWORD flag);
    SDWORD CAPSGetImageInfo(PCAPSIMAGEINFO pi, SDWORD id);
    SDWORD CAPSLockTrack(PCAPSTRACKINFO pi, SDWORD id, UDWORD cylinder, UDWORD head, UDWORD flag);
    SDWORD CAPSUnlockTrack(SDWORD id, UDWORD cylinder, UDWORD head);
    SDWORD CAPSUnlockAllTracks(SDWORD id);
    PCHAR CAPSGetPlatformName(UDWORD pid);
    SDWORD CAPSGetInfo(void* si, SDWORD id, UDWORD cylinder, UDWORD head, UDWORD inftype, UDWORD infid);
}
#endif


#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "windows.h"

#define CALLING_CONVENTION WINAPI
#define GETFUNC GetProcAddress

HMODULE capi = NULL;


#else
#include <dlfcn.h>

#define CALLING_CONVENTION
#define GETFUNC dlsym

void* capi = NULL;
#endif



CapsProc cpr[]= {
	"CAPSInit", NULL,
	"CAPSExit", NULL,
	"CAPSAddImage", NULL,
	"CAPSRemImage", NULL,
	"CAPSLockImage", NULL,
	"CAPSUnlockImage", NULL,
	"CAPSLoadImage", NULL,
	"CAPSGetImageInfo", NULL,
	"CAPSLockTrack", NULL,
	"CAPSUnlockTrack", NULL,
	"CAPSUnlockAllTracks", NULL,
	"CAPSGetPlatformName", NULL,
	"CAPSLockImageMemory", NULL,
	"CAPSGetInfo", NULL,
	NULL, NULL
};



// start caps image support
SDWORD CapsInit()
{
	DebugMsg::print(__func__, QString("ENTER"));
	if (capi)
		return imgeOk;

#ifdef CAPS_USER
	// when linking capsimg sources directly, map function pointers to the library functions
	cpr[0].proc = (void*)CAPSInit;
	cpr[1].proc = (void*)CAPSExit;
	cpr[2].proc = (void*)CAPSAddImage;
	cpr[3].proc = (void*)CAPSRemImage;
	cpr[4].proc = (void*)CAPSLockImage;
	cpr[5].proc = (void*)CAPSUnlockImage;
	cpr[6].proc = (void*)CAPSLoadImage;
	cpr[7].proc = (void*)CAPSGetImageInfo;
	cpr[8].proc = (void*)CAPSLockTrack;
	cpr[9].proc = (void*)CAPSUnlockTrack;
	cpr[10].proc = (void*)CAPSUnlockAllTracks;
	cpr[11].proc = (void*)CAPSGetPlatformName;
	cpr[12].proc = (void*)CAPSLockImageMemory;
	cpr[13].proc = (void*)CAPSGetInfo;
	// mark capi to indicate library available
	capi = (void*)1;
	SDWORD res = cpr[0].proc ? CAPSHOOKN(cpr[0].proc)() : imgeUnsupported;
	return res;
#else
#ifdef _WIN32
	capi = LoadLibrary(L"CAPSImg.dll");
#else
#ifdef __APPLE__
	capi = dlopen("@rpath/CAPSImage.framework/Versions/Current/CAPSImage", RTLD_NOW);
	if (!capi) capi = dlopen("CAPSImage.framework/Versions/Current/CAPSImage", RTLD_NOW);
	if (!capi) capi = dlopen("CAPSImage.framework/CAPSImage", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.dylib", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.5.dylib", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.5.1.dylib", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.4.dylib", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.4.2.dylib", RTLD_NOW);
#endif
	capi = dlopen("libcapsimage.so.5", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.so.5", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.so.5.1", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.so.4", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.so.4.2", RTLD_NOW);
	if (!capi) capi = dlopen("libcapsimage.so", RTLD_NOW);
#endif
	if (!capi)
		return imgeUnsupported;

	for (int prc=0; cpr[prc].name; prc++)
		cpr[prc].proc= GETFUNC(capi, cpr[prc].name);

	SDWORD res=cpr[0].proc ? CAPSHOOKN(cpr[0].proc)() : imgeUnsupported;

	return res;
#endif
}

// stop caps image support
SDWORD CapsExit()
{
	SDWORD res=cpr[1].proc ? CAPSHOOKN(cpr[1].proc)() : imgeUnsupported;

#ifndef CAPS_USER
	if (capi)
{
#ifdef _WIN32
		FreeLibrary(capi);
#else
		dlclose(capi);
#endif
		capi=NULL;
	}
#else
	// if CAPS_USER, do not attempt to free dynamic lib handle
	capi = NULL;
#endif

	for (int prc=0; cpr[prc].name; prc++)
		cpr[prc].proc=NULL;

	return res;
}

// add image container
SDWORD CapsAddImage()
{
	DebugMsg::print(__func__, QString("ENTER"));
	SDWORD res=cpr[2].proc ? CAPSHOOKN(cpr[2].proc)() : -1;

	return res;
}

// delete image container
SDWORD CapsRemImage(SDWORD id)
{
	DebugMsg::print(__func__, QString("ENTER"));
#ifdef CAPS_USER
	SDWORD res = CAPSRemImage(id);
#else
	SDWORD res=cpr[3].proc ? CAPSHOOKN(cpr[3].proc)(id) : -1;
#endif

	return res;
}

// lock image
SDWORD CapsLockImage(SDWORD id, PCHAR name)
{
	DebugMsg::print(__func__, QString("ENTER"));
	SDWORD res=cpr[4].proc ? CAPSHOOKN(cpr[4].proc)(id, name) : imgeUnsupported;

	return res;
}

// unlock image
SDWORD CapsUnlockImage(SDWORD id)
{
	DebugMsg::print(__func__, QString("ENTER"));
#ifdef CAPS_USER
	SDWORD res = CAPSUnlockImage(id);
#else
	SDWORD res=cpr[5].proc ? CAPSHOOKN(cpr[5].proc)(id) : imgeUnsupported;
#endif

	return res;
}

// load and decode complete image
SDWORD CapsLoadImage(SDWORD id, UDWORD flag)
{
	DebugMsg::print(__func__, QString("ENTER"));
#ifdef CAPS_USER
	SDWORD res = CAPSLoadImage(id, flag);
#else
	SDWORD res=cpr[6].proc ? CAPSHOOKN(cpr[6].proc)(id, flag) : imgeUnsupported;
#endif

	return res;
}

// get image information
SDWORD CapsGetImageInfo(PCAPSIMAGEINFO pi, SDWORD id)
{
	DebugMsg::print(__func__, QString("ENTER"));
#ifdef CAPS_USER
	SDWORD res = CAPSGetImageInfo(pi, id);
#else
	SDWORD res=cpr[7].proc ? CAPSHOOKN(cpr[7].proc)(pi, id) : imgeUnsupported;
#endif

	return res;
}

SDWORD CapsGetInfo(void* si, SDWORD id, UDWORD cylinder, UDWORD head, UDWORD inftype, UDWORD infid)
{
	DebugMsg::print(__func__, QString("ENTER"));
	SDWORD res = cpr[13].proc ? CAPSHOOKN(cpr[13].proc)(si, id, cylinder, head, inftype, infid) : imgeUnsupported;

	return res;
}

// load and decode track, or return with the cache
SDWORD CapsLockTrack(PCAPSTRACKINFO pi, SDWORD id, UDWORD cylinder, UDWORD head, UDWORD flag)
{
	DebugMsg::print(__func__, QString("ENTER"));
#ifdef CAPS_USER
	SDWORD res = CAPSLockTrack(pi, id, cylinder, head, flag);
#else
	SDWORD res=cpr[8].proc ? CAPSHOOKN(cpr[8].proc)(pi, id, cylinder, head, flag) : imgeUnsupported;
#endif

	return res;
}

// remove track from cache
SDWORD CapsUnlockTrack(SDWORD id, UDWORD cylinder, UDWORD head)
{
	DebugMsg::print(__func__, QString("ENTER"));
	SDWORD res=cpr[9].proc ? CAPSHOOKN(cpr[9].proc)(id, cylinder, head) : imgeUnsupported;

	return res;
}

// remove all tracks from cache
SDWORD CapsUnlockAllTracks(SDWORD id)
{
	DebugMsg::print(__func__, QString("ENTER"));
	SDWORD res=cpr[10].proc ? CAPSHOOKN(cpr[10].proc)(id) : imgeUnsupported;

	return res;
}

// get platform name
PCHAR CapsGetPlatformName(UDWORD pid)
{
	DebugMsg::print(__func__, QString("ENTER"));
	PCHAR res=cpr[11].proc ? CAPSHOOKS(cpr[11].proc)(pid) : NULL;

	return res;
}

// lock memory mapped image
SDWORD CapsLockImageMemory(SDWORD id, PUBYTE buffer, UDWORD length, UDWORD flag)
{
	DebugMsg::print(__func__, QString("ENTER"));
	SDWORD res=cpr[12].proc ? CAPSHOOKN(cpr[12].proc)(id, buffer, length, flag) : imgeUnsupported;

	return res;
}

