#ifndef _WIN32_UNISTD_H
#define _WIN32_UNISTD_H

/* Minimal unistd.h compatibility for MSVC/Windows.
   On non-Windows compilers, include the system unistd.h to avoid breaking Unix builds.
*/

#if !defined(_WIN32)
  /* Prefer include_next when available to avoid including this file recursively. */
  #if defined(__has_include_next)
    #if __has_include_next(<unistd.h>)
      #include_next <unistd.h>
    #else
      #include <unistd.h>
    #endif
  #else
    #include <unistd.h>
  #endif
#else

#include <windows.h>
#include <io.h>
#include <direct.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int ssize_t;

static inline unsigned int sleep(unsigned int seconds) {
    Sleep((DWORD)(seconds * 1000));
    return 0;
}

static inline int usleep(unsigned int usec) {
    /* Sleep granularity is milliseconds; approximate */
    Sleep((DWORD)((usec + 999) / 1000));
    return 0;
}

#define getcwd _getcwd
#define chdir _chdir
#define access _access

#ifndef F_OK
#define F_OK 0
#endif
#ifndef R_OK
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef X_OK
#define X_OK 1
#endif

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* _WIN32_UNISTD_H */
