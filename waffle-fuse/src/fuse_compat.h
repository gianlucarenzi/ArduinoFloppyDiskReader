#pragma once
// fuse_compat.h – Cross-platform FUSE compatibility header
//
// Platform       Library          FUSE API
// ──────────────────────────────────────────────────────
// Linux          libfuse3-dev     FUSE 3.x  (FUSE_USE_VERSION 31)
// macOS          macFUSE          FUSE 2.x  (FUSE_USE_VERSION 26)
// Windows        WinFsp           FUSE 2.x  (FUSE_USE_VERSION 26)
//
// Install instructions:
//   macOS:   brew install macfuse
//   Windows: https://github.com/winfsp/winfsp/releases

#if defined(_WIN32)
    // WinFsp FUSE compatibility layer
    // Build: add -I"C:/Program Files (x86)/WinFsp/inc/fuse" to CXXFLAGS
    #define FUSE_USE_VERSION 26
    #include <fuse.h>

#elif defined(__APPLE__)
    // macFUSE (formerly OSXFuse)
    // Build: add -I/usr/local/include/osxfuse (Intel)
    //         or -I/opt/homebrew/include/osxfuse (Apple Silicon)
    #define FUSE_USE_VERSION 26
    #include <fuse.h>

#else
    // Linux: libfuse3-dev by default; USE_FUSE2 forces libfuse-dev (FUSE 2)
    // (set via make USE_FUSE2=1, which passes -DUSE_FUSE2 to the compiler)
    #ifdef USE_FUSE2
        #define FUSE_USE_VERSION 26
        #include <fuse.h>
    #else
        #define FUSE_USE_VERSION 31
        #include <fuse3/fuse.h>
    #endif

#endif

// ── Build information ────────────────────────────────────────────────────────

#if defined(_WIN32)
#  define WF_PLATFORM_STR "Windows (WinFsp)"
#elif defined(__APPLE__)
#  define WF_PLATFORM_STR "macOS (macFUSE)"
#else
#  define WF_PLATFORM_STR "Linux"
#endif

// Stringify helpers
#define WF_STR_(x) #x
#define WF_STR(x)  WF_STR_(x)

// Compile-time FUSE API version (e.g. "3.1" or "2.9")
#define WF_FUSE_API_STR \
    WF_STR(FUSE_MAJOR_VERSION) "." WF_STR(FUSE_MINOR_VERSION)

#include <string>

// Runtime FUSE library version as human-readable string.
// FUSE 3 encodes fuse_version() as major*100+minor (e.g. 314 → "3.14").
// FUSE 2 encodes it as major*10+minor  (e.g. 29  → "2.9").
inline std::string wf_fuse_runtime_version()
{
    int v = fuse_version();
#if FUSE_USE_VERSION >= 30
    return std::to_string(v / 100) + "." + std::to_string(v % 100);
#else
    return std::to_string(v / 10)  + "." + std::to_string(v % 10);
#endif
}
//
// FUSE 3 added extra parameters to several callbacks and introduced
// fuse_config / fuse_readdir_flags / fuse_fill_dir_flags.
// The macros below let the source compile against both versions.

#if FUSE_USE_VERSION >= 30

    // Trailing fuse_file_info* added to these callbacks in FUSE 3:
#   define WF_GETATTR_EXTRA   , struct fuse_file_info*
#   define WF_TRUNCATE_EXTRA  , struct fuse_file_info*
#   define WF_CHMOD_EXTRA     , struct fuse_file_info*
#   define WF_CHOWN_EXTRA     , struct fuse_file_info*
#   define WF_UTIMENS_EXTRA   , struct fuse_file_info*

    // readdir: extra fuse_readdir_flags param added in FUSE 3:
#   define WF_READDIR_FLAGS   , enum fuse_readdir_flags

    // init: fuse_config* added in FUSE 3:
#   define WF_INIT_SIG(conn)   struct fuse_conn_info* conn, struct fuse_config* cfg
#   define WF_CFG_SET(f, v)    cfg->f = (v)

    // rename: flags param added in FUSE 3:
#   define WF_RENAME_SIG       const char* from, const char* to, unsigned int flags
#   define WF_RENAME_CHECK     if (flags) return -EINVAL;

    // filler() in readdir: extra fuse_fill_dir_flags param in FUSE 3:
#   define WF_FILL(fn, buf, name, st, off) \
        (fn)((buf), (name), (st), (off), static_cast<fuse_fill_dir_flags>(0))

#else  // FUSE 2

#   define WF_GETATTR_EXTRA
#   define WF_TRUNCATE_EXTRA
#   define WF_CHMOD_EXTRA
#   define WF_CHOWN_EXTRA
#   define WF_UTIMENS_EXTRA
#   define WF_READDIR_FLAGS

#   define WF_INIT_SIG(conn)   struct fuse_conn_info* conn
#   define WF_CFG_SET(f, v)    ((void)0)

#   define WF_RENAME_SIG       const char* from, const char* to
#   define WF_RENAME_CHECK

#   define WF_FILL(fn, buf, name, st, off) \
        (fn)((buf), (name), (st), (off))

#endif
