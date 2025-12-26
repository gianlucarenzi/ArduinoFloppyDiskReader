#ifndef COMPILERDEFS_H
#define COMPILERDEFS_H

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#if !defined(__PRETTY_FUNCTION__) && !defined(__GNUC__)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#endif // COMPILERDEFS_H
