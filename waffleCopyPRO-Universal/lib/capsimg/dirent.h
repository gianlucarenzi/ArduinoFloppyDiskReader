#ifndef _WIN32_DIRENT_H
#define _WIN32_DIRENT_H

/* Minimal dirent.h compatibility for MSVC using _findfirst/_findnext
   Provides: DIR, struct dirent { char d_name[]; }, opendir, readdir, closedir
   License: public-domain style (small compatibility shim)
*/

#ifdef _WIN32

#include <io.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAX_PATH
#define WIN32_DIRENT_MAX_PATH 260
#else
#define WIN32_DIRENT_MAX_PATH MAX_PATH
#endif

struct dirent {
    char d_name[WIN32_DIRENT_MAX_PATH];
};

typedef struct {
    intptr_t handle;
    struct _finddata_t info;
    int first;
    char pattern[WIN32_DIRENT_MAX_PATH];
    struct dirent result;
} DIR;

static inline DIR* opendir(const char* name) {
    if (!name) return NULL;
    DIR* d = (DIR*)malloc(sizeof(DIR));
    if (!d) return NULL;
    memset(d, 0, sizeof(DIR));
    /* build search pattern "<name>\\*" */
    size_t len = strlen(name);
    if (len + 3 > WIN32_DIRENT_MAX_PATH) { free(d); return NULL; }
    strcpy(d->pattern, name);
    /* remove trailing slash/backslash if present */
    if (len > 0 && (d->pattern[len-1] == '/' || d->pattern[len-1] == '\\')) {
        d->pattern[len-1] = '\\0';
    }
    strcat(d->pattern, "\\\\*");
    d->handle = _findfirst(d->pattern, &d->info);
    if (d->handle == -1) { free(d); return NULL; }
    d->first = 1;
    strncpy(d->result.d_name, d->info.name, WIN32_DIRENT_MAX_PATH-1);
    d->result.d_name[WIN32_DIRENT_MAX_PATH-1] = '\\0';
    return d;
}

static inline struct dirent* readdir(DIR* d) {
    if (!d) return NULL;
    if (d->first) { d->first = 0; return &d->result; }
    if (_findnext(d->handle, &d->info) != 0) return NULL;
    strncpy(d->result.d_name, d->info.name, WIN32_DIRENT_MAX_PATH-1);
    d->result.d_name[WIN32_DIRENT_MAX_PATH-1] = '\\0';
    return &d->result;
}

static inline int closedir(DIR* d) {
    if (!d) return -1;
    int r = 0;
    if (d->handle != -1) r = _findclose(d->handle);
    free(d);
    return r;
}

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* _WIN32_DIRENT_H */
