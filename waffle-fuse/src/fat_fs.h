#pragma once
// waffle-fuse: FAT12 / FAT16 FUSE filesystem driver

#include "fuse_compat.h"
#include "disk_cache.h"

// Returns filled fuse_operations for a FAT12/FAT16 disk.
// After mount, fuse_get_context()->private_data points to a FatFs instance.
struct fuse_operations fat_get_operations();

// Returns true if the first sector of 'disk' looks like a FAT12/FAT16 volume.
bool fat_detect(IDiskImage* disk);

// Create and initialise a FatFs instance; returns opaque pointer for use as
// FUSE private_data (and with the helpers below). Returns nullptr on failure.
void* fat_fs_new(IDiskImage* disk);

// Free the FatFs instance created by fat_fs_new().
void  fat_fs_free(void* ctx);

// Query helpers (valid only when ctx != nullptr).
bool  fat_is_fat16(void* ctx);  // false => FAT12
