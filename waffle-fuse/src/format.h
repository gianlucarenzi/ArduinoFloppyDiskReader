#pragma once
// waffle-fuse: build freshly formatted disk images (AmigaDOS OFS/FFS, FAT12)

#include <cstdint>
#include <string>
#include <vector>
#include "disk_cache.h"

struct FormatSpec
{
    enum class Kind { AmigaOFS, AmigaFFS, FAT12 } kind = Kind::AmigaFFS;
    bool        hd    = false;   // DD: 880K / 720K, HD: 1.76M / 1.44M
    bool        intl  = false;   // AmigaDOS international mode (DOS2 / DOS3)
    std::string label;           // volume name (UTF-8)
};

// Parse "amiga" / "ofs" / "ffs" / "dos" / "fat" (+ "-intl" for Amiga).  False if unknown.
bool parseFormatKind(const std::string& s, FormatSpec& spec);

// Geometry of the formatted disk
DiskGeometry formatGeometry(const FormatSpec& spec);

// Build the whole disk image (numCylinders * heads * sectors * 512 bytes).
// Returns an error message, or "" on success.
std::string buildFormatImage(const FormatSpec& spec, std::vector<uint8_t>& image);
