#ifndef COMPILERDEFS_H
#define COMPILERDEFS_H

#if !defined(__PRETTY_FUNCTION__) && !defined(__GNUC__)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#ifdef _WIN32
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (P)
#endif
#endif

#endif // COMPILERDEFS_H
