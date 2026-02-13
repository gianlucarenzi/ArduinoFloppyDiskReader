#ifndef COMPILERDEFS_H
#define COMPILERDEFS_H



#ifdef _WIN32
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (P)
#endif
#endif

#ifndef _WIN32
#define __cdecl
#endif

#endif // COMPILERDEFS_H
