#pragma once
// waffle-fuse: Amiga OFS/FFS FUSE filesystem driver (read + write)

#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <string>
#include "disk_cache.h"

struct fuse_operations affs_get_operations();

// Returns true if the disk looks like an Amiga ADF (OFS or FFS).
bool affs_detect(IDiskImage* disk);

// Create and initialise an AffsFs instance; returns opaque pointer for use as
// FUSE private_data (and with the helpers below). Returns nullptr on failure.
void* affs_create(IDiskImage* disk);

// Free the AffsFs instance created by affs_create().
void  affs_destroy(void* ctx);

// Query helpers (valid only when ctx != nullptr).
bool        affs_is_ffs(void* ctx);       // false => OFS
bool        affs_is_readonly(void* ctx);  // write-protected, unvalidated bitmap or DirCache
std::string affs_dostype(void* ctx);      // e.g. "DOS1"
std::string affs_volume_name(void* ctx);  // UTF-8
std::string affs_readonly_reason(void* ctx); // empty unless the filesystem itself forces read-only
