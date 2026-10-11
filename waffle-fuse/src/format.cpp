// waffle-fuse: build freshly formatted disk images (AmigaDOS OFS/FFS, FAT12)
//
// The layouts follow what AmigaOS Format / amitools and MS-DOS FORMAT /
// mkfs.fat produce, so the result is accepted by real machines and by the
// usual validators (xdfscan, fsck.fat).

#include "format.h"

#include <cstring>
#include <ctime>
#include <cctype>

// ── Helpers ───────────────────────────────────────────────────────────────────

static void put32be(uint8_t* b, size_t o, uint32_t v)
{
    b[o] = (uint8_t)(v >> 24);
    b[o+1] = (uint8_t)(v >> 16);
    b[o+2] = (uint8_t)(v >> 8);
    b[o+3] = (uint8_t)v;
}
static uint32_t get32be(const uint8_t* b, size_t o)
{
    return ((uint32_t)b[o] << 24) | ((uint32_t)b[o+1] << 16) | ((uint32_t)b[o+2] << 8) | b[o+3];
}
static void put16le(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static void put32le(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

// Amiga block checksum: the sum of all longs must be 0
static void amigaChecksum(uint8_t* b, size_t off)
{
    put32be(b, off, 0);
    uint32_t sum = 0;
    for (size_t i = 0; i < 512; i += 4) sum += get32be(b, i);
    put32be(b, off, (uint32_t)(-(int32_t)sum));
}

// Amiga date stamp in local time (days, minutes, ticks since 1-Jan-1978)
static void amigaDate(uint8_t* b, size_t off, time_t t)
{
    struct tm lt;
    localtime_r(&t, &lt);
    time_t local = t + lt.tm_gmtoff - 252460800;
    if (local < 0) local = 0;
    put32be(b, off,     (uint32_t)(local / 86400));
    put32be(b, off + 4, (uint32_t)((local % 86400) / 60));
    put32be(b, off + 8, (uint32_t)((local % 60) * 50));
}

// UTF-8 -> ISO-8859-1; characters outside Latin-1 become '_'
static std::string toLatin1(const std::string& s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); )
    {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80)
        {
            out += (char)c;
            i++;
            continue;
        }
        int n = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
        uint32_t cp = n == 2 ? (((uint32_t)(c & 0x1F) << 6) | (i + 1 < s.size() ? (s[i+1] & 0x3F) : 0)) : 0x100;
        out += cp <= 0xFF ? (char)cp : '_';
        i += n;
    }
    return out;
}

bool parseFormatKind(const std::string& s, FormatSpec& spec)
{
    spec.intl = false;
    if (s == "amiga" || s == "ffs")              spec.kind = FormatSpec::Kind::AmigaFFS;
    else if (s == "ofs")                         spec.kind = FormatSpec::Kind::AmigaOFS;
    else if (s == "amiga-intl" || s == "ffs-intl")
    {
        spec.kind = FormatSpec::Kind::AmigaFFS;
        spec.intl = true;
    }
    else if (s == "ofs-intl")
    {
        spec.kind = FormatSpec::Kind::AmigaOFS;
        spec.intl = true;
    }
    else if (s == "dos" || s == "fat" || s == "fat12") spec.kind = FormatSpec::Kind::FAT12;
    else return false;
    return true;
}

DiskGeometry formatGeometry(const FormatSpec& spec)
{
    DiskGeometry g;
    g.isHD           = spec.hd;
    g.numCylinders   = 80;
    g.numHeads       = 2;
    g.bytesPerSector = 512;
    if (spec.kind == FormatSpec::Kind::FAT12)
    {
        g.format          = DiskFormat::IBM_FAT;
        g.sectorsPerTrack = spec.hd ? 18 : 9;
    }
    else
    {
        g.format          = DiskFormat::Amiga_ADF;
        g.sectorsPerTrack = spec.hd ? 22 : 11;
    }
    return g;
}

// ── AmigaDOS ──────────────────────────────────────────────────────────────────

static std::string buildAmiga(const FormatSpec& spec, std::vector<uint8_t>& img, uint32_t numBlocks)
{
    std::string name = toLatin1(spec.label.empty() ? "Empty" : spec.label);
    if (name.size() > 30) return "volume name longer than 30 characters";
    if (name.find_first_of(":/") != std::string::npos) return "volume name cannot contain ':' or '/'";

    const uint8_t dosType = (spec.kind == FormatSpec::Kind::AmigaFFS ? 1 : 0) | (spec.intl ? 2 : 0);
    const uint32_t root = numBlocks / 2, bitmap = root + 1;
    const time_t now = time(nullptr);

    // Boot block: "DOS\x", not bootable (checksum 0), root block pointer
    uint8_t* boot = img.data();
    boot[0] = 'D';
    boot[1] = 'O';
    boot[2] = 'S';
    boot[3] = dosType;
    put32be(boot, 8, root);

    // Root block
    uint8_t* rb = img.data() + (size_t)root * 512;
    put32be(rb, 0, 2);            // T_HEADER
    put32be(rb, 12, 72);          // hash table size
    put32be(rb, 312, 0xFFFFFFFF); // bm_flag: bitmap valid
    put32be(rb, 316, bitmap);     // bm_pages[0]
    amigaDate(rb, 420, now);      // root altered
    rb[432] = (uint8_t)name.size();
    memcpy(rb + 433, name.data(), name.size());
    amigaDate(rb, 472, now);      // volume altered
    amigaDate(rb, 484, now);      // volume created
    put32be(rb, 508, 1);          // ST_ROOT
    amigaChecksum(rb, 20);

    // Bitmap: bit (n-2) set = block n free; root and bitmap are in use
    uint8_t* bm = img.data() + (size_t)bitmap * 512;
    for (size_t o = 4; o < 512; o += 4) put32be(bm, o, 0xFFFFFFFF);
    for (uint32_t used :
            {
                root, bitmap
            })
    {
        uint32_t bit = used - 2;
        size_t o = 4 + (bit / 32) * 4;
        put32be(bm, o, get32be(bm, o) & ~(1u << (bit % 32)));
    }
    amigaChecksum(bm, 0);
    return "";
}

// ── FAT12 ─────────────────────────────────────────────────────────────────────

// Same "not bootable" stub mkfs.fat installs (code at 0x3E, message at 0x5B)
static const uint8_t kBootStub[] =
{
    0x0E, 0x1F, 0xBE, 0x5B, 0x7C, 0xAC, 0x22, 0xC0, 0x74, 0x0B, 0x56, 0xB4, 0x0E, 0xBB,
    0x07, 0x00, 0xCD, 0x10, 0x5E, 0xEB, 0xF0, 0x32, 0xE4, 0xCD, 0x16, 0xCD, 0x19, 0xEB, 0xFE,
};
static const char kBootMsg[] =
    "This is not a bootable disk.  Please insert a bootable floppy and\r\n"
    "press any key to try again ... \r\n";

static std::string buildFat(const FormatSpec& spec, std::vector<uint8_t>& img)
{
    // Standard PC floppy parameters (what MS-DOS FORMAT writes)
    const bool hd = spec.hd;
    const uint16_t totSec   = hd ? 2880 : 1440;
    const uint8_t  spc      = hd ? 1 : 2;
    const uint16_t rootEnt  = hd ? 224 : 112;
    const uint16_t fatSz    = hd ? 9 : 3;
    const uint16_t spt      = hd ? 18 : 9;
    const uint8_t  media    = hd ? 0xF0 : 0xF9;

    // Label: upper case, characters valid in 8.3 names, at most 11
    std::string label;
    for (unsigned char c : spec.label)
    {
        if (label.size() == 11) break;
        c = (unsigned char)toupper(c);
        if (c >= 0x80 || c < 0x20 || strchr("\"*+,./:;<=>?[\\]|", c)) c = '_';
        label += (char)c;
    }
    if (spec.label.size() > 11) return "FAT volume label longer than 11 characters";

    uint8_t* b = img.data();
    b[0] = 0xEB;
    b[1] = 0x3C;
    b[2] = 0x90;
    memcpy(b + 3, "MSWIN4.1", 8);
    put16le(b + 11, 512);
    b[13] = spc;
    put16le(b + 14, 1);           // reserved sectors
    b[16] = 2;                    // FAT copies
    put16le(b + 17, rootEnt);
    put16le(b + 19, totSec);
    b[21] = media;
    put16le(b + 22, fatSz);
    put16le(b + 24, spt);
    put16le(b + 26, 2);           // heads
    // Extended BPB
    b[36] = 0x00;                 // drive number
    b[38] = 0x29;
    put32le(b + 39, (uint32_t)time(nullptr) ^ 0x5741464Cu);   // serial number
    std::string lbl = label.empty() ? "NO NAME" : label;
    memset(b + 43, ' ', 11);
    memcpy(b + 43, lbl.data(), lbl.size());
    memcpy(b + 54, "FAT12   ", 8);
    memcpy(b + 0x3E, kBootStub, sizeof kBootStub);
    memcpy(b + 0x5B, kBootMsg, sizeof kBootMsg - 1);
    b[510] = 0x55;
    b[511] = 0xAA;

    // Both FATs: media descriptor and end-of-chain for clusters 0 and 1
    for (int copy = 0; copy < 2; copy++)
    {
        uint8_t* f = img.data() + (size_t)(1 + copy * fatSz) * 512;
        f[0] = media;
        f[1] = 0xFF;
        f[2] = 0xFF;
    }

    // Root directory: volume label entry
    if (!label.empty())
    {
        uint8_t* e = img.data() + (size_t)(1 + 2 * fatSz) * 512;
        memset(e, ' ', 11);
        memcpy(e, label.data(), label.size());
        e[11] = 0x08;
        struct tm lt;
        time_t now = time(nullptr);
        localtime_r(&now, &lt);
        put16le(e + 22, (uint16_t)((lt.tm_hour << 11) | (lt.tm_min << 5) | (lt.tm_sec / 2)));
        put16le(e + 24, (uint16_t)(((lt.tm_year - 80) << 9) | ((lt.tm_mon + 1) << 5) | lt.tm_mday));
    }
    return "";
}

std::string buildFormatImage(const FormatSpec& spec, std::vector<uint8_t>& image)
{
    const DiskGeometry g = formatGeometry(spec);
    const uint32_t sectors = g.numCylinders * g.numHeads * g.sectorsPerTrack;
    image.assign((size_t)sectors * 512, 0);
    return spec.kind == FormatSpec::Kind::FAT12 ? buildFat(spec, image) : buildAmiga(spec, image, sectors);
}
