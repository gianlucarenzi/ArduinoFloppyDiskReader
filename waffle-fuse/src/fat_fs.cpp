// waffle-fuse: FAT12 / FAT16 FUSE filesystem driver
// Supports read + write, VFAT long file names (read and create), NT lower-case
// flags for short names, rename/move between directories.

#define FUSE_USE_VERSION 31
#include "fat_fs.h"

#include <cstring>
#include <cctype>
#include <ctime>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <errno.h>
#include <unistd.h>

// ── Little-endian helpers ────────────────────────────────────────────────────

static inline uint16_t le16(const uint8_t* p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t le32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static inline void put_le16(uint8_t* p, uint16_t v)
{
    p[0] = v & 0xFF;
    p[1] = v >> 8;
}
static inline void put_le32(uint8_t* p, uint32_t v)
{
    p[0] = v;
    p[1] = v>>8;
    p[2] = v>>16;
    p[3] = v>>24;
}

// ── BPB (BIOS Parameter Block) ────────────────────────────────────────────────

struct BPB
{
    uint16_t bytesPerSec;
    uint8_t  secPerClus;
    uint16_t rsvdSecCnt;
    uint8_t  numFATs;
    uint16_t rootEntCnt;
    uint32_t totSec;
    uint16_t fatSz;        // FAT sectors per copy
    uint32_t firstDataSec; // computed
    uint32_t rootDirSec;   // first sector of root dir
    uint32_t rootDirSectors;
    uint32_t dataClusterCount;
    bool     isFAT16;
};

static bool parseBPB(const uint8_t* sec0, BPB& bpb)
{
    // Must start with a valid x86 jump opcode or carry the boot signature
    bool jmp = sec0[0] == 0xEB || sec0[0] == 0xE9;
    bool sig = sec0[510] == 0x55 && sec0[511] == 0xAA;
    if (!jmp && !sig) return false;

    bpb.bytesPerSec  = le16(sec0 + 11);
    bpb.secPerClus   = sec0[13];
    bpb.rsvdSecCnt   = le16(sec0 + 14);
    bpb.numFATs      = sec0[16];
    bpb.rootEntCnt   = le16(sec0 + 17);
    uint16_t totSec16 = le16(sec0 + 19);
    bpb.fatSz        = le16(sec0 + 22);
    uint32_t totSec32 = le32(sec0 + 32);
    bpb.totSec = totSec16 ? totSec16 : totSec32;

    // Only 512-byte sectors are supported by the block layer
    if (bpb.bytesPerSec != 512) return false;
    if (bpb.secPerClus == 0 || (bpb.secPerClus & (bpb.secPerClus - 1)) != 0) return false;
    if (bpb.numFATs == 0 || bpb.numFATs > 2) return false;
    if (bpb.rsvdSecCnt == 0 || bpb.fatSz == 0 || bpb.totSec == 0) return false;
    if (bpb.rootEntCnt == 0) return false;   // FAT32 – not supported

    bpb.rootDirSectors = ((uint32_t)bpb.rootEntCnt * 32 + bpb.bytesPerSec - 1) / bpb.bytesPerSec;
    bpb.rootDirSec     = bpb.rsvdSecCnt + bpb.numFATs * bpb.fatSz;
    bpb.firstDataSec   = bpb.rootDirSec + bpb.rootDirSectors;
    if (bpb.firstDataSec >= bpb.totSec) return false;

    bpb.dataClusterCount = (bpb.totSec - bpb.firstDataSec) / bpb.secPerClus;
    bpb.isFAT16          = bpb.dataClusterCount >= 4085; // FAT12 < 4085 clusters

    // The FAT must be large enough to describe every cluster
    uint64_t fatBytes = (uint64_t)bpb.fatSz * bpb.bytesPerSec;
    uint64_t needed   = bpb.isFAT16 ? (uint64_t)(bpb.dataClusterCount + 2) * 2
                        : ((uint64_t)(bpb.dataClusterCount + 2) * 3 + 1) / 2;
    return fatBytes >= needed;
}

// ── Character sets ───────────────────────────────────────────────────────────

// Code page 437 (the Linux vfat default) for bytes 0x80-0xFF in short names
static const uint16_t CP437[128] =
{
    0x00C7,0x00FC,0x00E9,0x00E2,0x00E4,0x00E0,0x00E5,0x00E7,0x00EA,0x00EB,0x00E8,0x00EF,0x00EE,0x00EC,0x00C4,0x00C5,
    0x00C9,0x00E6,0x00C6,0x00F4,0x00F6,0x00F2,0x00FB,0x00F9,0x00FF,0x00D6,0x00DC,0x00A2,0x00A3,0x00A5,0x20A7,0x0192,
    0x00E1,0x00ED,0x00F3,0x00FA,0x00F1,0x00D1,0x00AA,0x00BA,0x00BF,0x2310,0x00AC,0x00BD,0x00BC,0x00A1,0x00AB,0x00BB,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,0x2555,0x2563,0x2551,0x2557,0x255D,0x255C,0x255B,0x2510,
    0x2514,0x2534,0x252C,0x251C,0x2500,0x253C,0x255E,0x255F,0x255A,0x2554,0x2569,0x2566,0x2560,0x2550,0x256C,0x2567,
    0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256B,0x256A,0x2518,0x250C,0x2588,0x2584,0x258C,0x2590,0x2580,
    0x03B1,0x00DF,0x0393,0x03C0,0x03A3,0x03C3,0x00B5,0x03C4,0x03A6,0x0398,0x03A9,0x03B4,0x221E,0x03C6,0x03B5,0x2229,
    0x2261,0x00B1,0x2265,0x2264,0x2320,0x2321,0x00F7,0x2248,0x00B0,0x2219,0x00B7,0x221A,0x207F,0x00B2,0x25A0,0x00A0,
};

static void appendUtf8(std::string& s, uint32_t cp)
{
    if (cp < 0x80) s += (char)cp;
    else if (cp < 0x800)
    {
        s += (char)(0xC0|(cp>>6));
        s += (char)(0x80|(cp&0x3F));
    }
    else if (cp < 0x10000)
    {
        s += (char)(0xE0|(cp>>12));
        s += (char)(0x80|((cp>>6)&0x3F));
        s += (char)(0x80|(cp&0x3F));
    }
    else
    {
        s += (char)(0xF0|(cp>>18));
        s += (char)(0x80|((cp>>12)&0x3F));
        s += (char)(0x80|((cp>>6)&0x3F));
        s += (char)(0x80|(cp&0x3F));
    }
}

static std::string utf16ToUtf8(const std::u16string& u)
{
    std::string s;
    for (size_t i = 0; i < u.size(); i++)
    {
        uint32_t c = u[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < u.size() && u[i+1] >= 0xDC00 && u[i+1] <= 0xDFFF)
        {
            c = 0x10000 + ((c - 0xD800) << 10) + (u[i+1] - 0xDC00);
            i++;
        }
        appendUtf8(s, c);
    }
    return s;
}

static bool utf8ToUtf16(const std::string& s, std::u16string& out)
{
    out.clear();
    for (size_t i = 0; i < s.size(); )
    {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp;
        int n;
        if (c < 0x80)
        {
            cp = c;
            n = 1;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            cp = c & 0x1F;
            n = 2;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            cp = c & 0x0F;
            n = 3;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            cp = c & 0x07;
            n = 4;
        }
        else return false;
        if (i + n > s.size()) return false;
        for (int k = 1; k < n; k++)
        {
            unsigned char cc = (unsigned char)s[i+k];
            if ((cc & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (cc & 0x3F);
        }
        i += n;
        if (cp >= 0x10000)
        {
            cp -= 0x10000;
            out += (char16_t)(0xD800 + (cp >> 10));
            out += (char16_t)(0xDC00 + (cp & 0x3FF));
        }
        else
        {
            out += (char16_t)cp;
        }
    }
    return true;
}

// ASCII-only case-insensitive compare of UTF-8 names (what DOS/Windows do for
// the characters that matter on floppies).
static bool nameEq(const std::string& a, const std::string& b)
{
    return strcasecmp(a.c_str(), b.c_str()) == 0;
}

// ── Directory entries ────────────────────────────────────────────────────────

static const uint8_t ATTR_RO     = 0x01;
static const uint8_t ATTR_VOLUME = 0x08;
static const uint8_t ATTR_DIR    = 0x10;
static const uint8_t ATTR_ARCH   = 0x20;
static const uint8_t ATTR_LFN    = 0x0F;
static const uint8_t NT_LOWER_BASE = 0x08;
static const uint8_t NT_LOWER_EXT  = 0x10;

struct Slot
{
    uint32_t sector = 0;
    uint32_t idx = 0;
};

struct DirItem
{
    std::string name;        // long name if present, else short name (UTF-8)
    std::string shortName;   // 8.3 alias as displayed
    uint8_t     raw[32] = {};
    Slot        slot;        // short entry
    std::vector<Slot> lfnSlots;

    bool     isDir()    const
    {
        return raw[11] & ATTR_DIR;
    }
    uint32_t cluster()  const
    {
        return le16(raw + 26);
    }
    uint32_t fileSize() const
    {
        return le32(raw + 28);
    }
};

static uint8_t lfnChecksum(const uint8_t* shortName11)
{
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++)
        sum = (uint8_t)(((sum & 1) << 7) + (sum >> 1) + shortName11[i]);
    return sum;
}

static std::string shortNameOf(const uint8_t* raw)
{
    auto conv = [](uint8_t c, bool lower, std::string& out)
    {
        if (c >= 0x80) appendUtf8(out, CP437[c - 0x80]);
        else out += (char)(lower ? tolower(c) : c);
    };
    std::string s;
    int baseLen = 8;
    while (baseLen > 0 && raw[baseLen - 1] == ' ') baseLen--;
    int extLen  = 3;
    while (extLen > 0 && raw[8 + extLen - 1] == ' ') extLen--;
    for (int i = 0; i < baseLen; i++)
    {
        uint8_t c = raw[i];
        if (i == 0 && c == 0x05) c = 0xE5;
        conv(c, raw[12] & NT_LOWER_BASE, s);
    }
    if (extLen > 0)
    {
        s += '.';
        for (int i = 0; i < extLen; i++) conv(raw[8 + i], raw[12] & NT_LOWER_EXT, s);
    }
    return s;
}

static time_t dosToUnix(uint16_t date, uint16_t time)
{
    if (date == 0) return 0;
    struct tm t = {};
    t.tm_year  = ((date >> 9) & 0x7F) + 80;
    t.tm_mon   = ((date >> 5) & 0x0F) - 1;
    t.tm_mday  = date & 0x1F;
    t.tm_hour  = (time >> 11) & 0x1F;
    t.tm_min   = (time >> 5) & 0x3F;
    t.tm_sec   = (time & 0x1F) * 2;
    t.tm_isdst = -1;
    return mktime(&t);
}

static void unixToDos(time_t when, uint16_t& date, uint16_t& time)
{
    struct tm t;
    localtime_r(&when, &t);
    if (t.tm_year < 80)
    {
        date = (0 << 9) | (1 << 5) | 1;
        time = 0;
        return;
    }
    date = (uint16_t)(((t.tm_year - 80) << 9) | ((t.tm_mon + 1) << 5) | t.tm_mday);
    time = (uint16_t)((t.tm_hour << 11) | (t.tm_min << 5) | (t.tm_sec / 2));
}

static void setMtime(uint8_t* raw, time_t when)
{
    uint16_t d, t;
    unixToDos(when, d, t);
    put_le16(raw + 22, t);
    put_le16(raw + 24, d);
    put_le16(raw + 18, d);   // last access date
}

// ── FatFs ────────────────────────────────────────────────────────────────────

class FatFs
{
public:
    IDiskImage* disk = nullptr;
    BPB         bpb{};
    std::vector<uint8_t> fat;          // cached copy of FAT #1
    std::set<uint32_t>   fatDirtySecs; // FAT sectors (relative) to write back
    std::string          volumeLabel;
    uint32_t             allocHint = 2;

    bool ro() const
    {
        return disk->isWriteProtected();
    }
    uint32_t clusterBytes() const
    {
        return (uint32_t)bpb.secPerClus * bpb.bytesPerSec;
    }

    // ── Init ──────────────────────────────────────────────────────────────────
    bool init(IDiskImage* d)
    {
        disk = d;
        uint8_t sec0[512];
        if (!disk->readSector(0, sec0)) return false;
        if (!parseBPB(sec0, bpb)) return false;
        if (bpb.totSec > disk->totalSectors()) return false;

        // Load the FAT; fall back to the second copy for unreadable sectors
        fat.assign((size_t)bpb.fatSz * bpb.bytesPerSec, 0);
        for (uint32_t i = 0; i < bpb.fatSz; i++)
        {
            uint8_t buf[512];
            bool ok = disk->readSector(bpb.rsvdSecCnt + i, buf);
            if (!ok && bpb.numFATs > 1)
                ok = disk->readSector(bpb.rsvdSecCnt + bpb.fatSz + i, buf);
            if (!ok) return false;
            memcpy(fat.data() + (size_t)i * 512, buf, 512);
        }

        // Volume label: root directory entry first, then the extended BPB
        std::vector<uint32_t> secs = rootDirSectors();
        for (uint32_t sec : secs)
        {
            uint8_t buf[512];
            if (!disk->readSector(sec, buf)) break;
            bool end = false;
            for (int i = 0; i < 16 && !end; i++)
            {
                const uint8_t* e = buf + i * 32;
                if (e[0] == 0)
                {
                    end = true;
                    break;
                }
                if (e[0] == 0xE5 || (e[11] & 0x3F) == ATTR_LFN) continue;
                if ((e[11] & (ATTR_VOLUME | ATTR_DIR)) == ATTR_VOLUME)
                {
                    volumeLabel = trimLabel(e);
                    end = true;
                }
            }
            if (end) break;
        }
        if (volumeLabel.empty() && sec0[38] == 0x29) volumeLabel = trimLabel(sec0 + 43);
        if (volumeLabel == "NO NAME") volumeLabel.clear();
        return true;
    }

    static std::string trimLabel(const uint8_t* p)
    {
        std::string s(reinterpret_cast<const char*>(p), 11);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
        std::string out;
        for (unsigned char c : s)
        {
            if (c >= 0x80) appendUtf8(out, CP437[c - 0x80]);
            else if (c >= 0x20 && c != '/') out += (char)c;
        }
        return out;
    }

    // ── FAT table ─────────────────────────────────────────────────────────────
    bool validCluster(uint32_t c) const
    {
        return c >= 2 && c < bpb.dataClusterCount + 2;
    }
    uint32_t eocValue() const
    {
        return bpb.isFAT16 ? 0xFFFF : 0x0FFF;
    }
    bool isEOC(uint32_t v) const
    {
        return bpb.isFAT16 ? v >= 0xFFF8 : v >= 0xFF8;
    }

    uint32_t fatGet(uint32_t cluster) const
    {
        if (bpb.isFAT16)
        {
            size_t off = (size_t)cluster * 2;
            if (off + 1 >= fat.size()) return 0xFFFF;
            return le16(fat.data() + off);
        }
        size_t off = cluster + cluster / 2;
        if (off + 1 >= fat.size()) return 0xFFF;
        uint32_t v = fat[off] | ((uint32_t)fat[off + 1] << 8);
        return (cluster & 1) ? (v >> 4) : (v & 0x0FFF);
    }

    void fatSet(uint32_t cluster, uint32_t val)
    {
        size_t off;
        if (bpb.isFAT16)
        {
            off = (size_t)cluster * 2;
            if (off + 1 >= fat.size()) return;
            put_le16(fat.data() + off, (uint16_t)val);
        }
        else
        {
            off = cluster + cluster / 2;
            if (off + 1 >= fat.size()) return;
            if (cluster & 1)
            {
                fat[off]     = (uint8_t)((fat[off] & 0x0F) | ((val & 0x0F) << 4));
                fat[off + 1] = (uint8_t)(val >> 4);
            }
            else
            {
                fat[off]     = (uint8_t)(val & 0xFF);
                fat[off + 1] = (uint8_t)((fat[off + 1] & 0xF0) | ((val >> 8) & 0x0F));
            }
        }
        fatDirtySecs.insert((uint32_t)(off / 512));
        fatDirtySecs.insert((uint32_t)((off + 1) / 512));
    }

    // Write modified FAT sectors back to every copy
    bool flushFAT()
    {
        bool ok = true;
        for (uint32_t s : fatDirtySecs)
        {
            if ((size_t)s * 512 >= fat.size()) continue;
            for (uint8_t copy = 0; copy < bpb.numFATs; copy++)
                ok &= disk->writeSector(bpb.rsvdSecCnt + copy * bpb.fatSz + s, fat.data() + (size_t)s * 512);
        }
        fatDirtySecs.clear();
        return ok;
    }

    uint32_t clusterToSector(uint32_t cluster) const
    {
        return bpb.firstDataSec + (cluster - 2) * bpb.secPerClus;
    }

    // Cluster chain starting at 'start' (guards against loops and bad links)
    std::vector<uint32_t> chain(uint32_t start) const
    {
        std::vector<uint32_t> out;
        uint32_t c = start;
        while (validCluster(c) && out.size() <= bpb.dataClusterCount)
        {
            out.push_back(c);
            uint32_t next = fatGet(c);
            if (isEOC(next) || !validCluster(next)) break;
            c = next;
        }
        return out;
    }

    uint32_t countFree() const
    {
        uint32_t n = 0;
        for (uint32_t c = 2; c < bpb.dataClusterCount + 2; c++)
            if (fatGet(c) == 0) n++;
        return n;
    }

    // Allocate a zero-filled cluster and link it after 'prev' (0 = new chain).
    uint32_t allocCluster(uint32_t prev)
    {
        const uint32_t total = bpb.dataClusterCount;
        for (uint32_t i = 0; i < total; i++)
        {
            uint32_t c = 2 + ((allocHint - 2 + i) % total);
            if (fatGet(c) != 0) continue;
            uint8_t zero[512] = {};
            uint32_t base = clusterToSector(c);
            for (uint8_t s = 0; s < bpb.secPerClus; s++)
                if (!disk->writeSector(base + s, zero)) return 0;
            fatSet(c, eocValue());
            if (prev >= 2) fatSet(prev, c);
            allocHint = c + 1 < total + 2 ? c + 1 : 2;
            return c;
        }
        return 0;
    }

    // Grow 'clusters' to at least 'needed' entries.  0 or -ENOSPC/-EIO.
    int extendChain(std::vector<uint32_t>& clusters, uint32_t needed)
    {
        if (clusters.size() >= needed) return 0;
        if (needed - clusters.size() > countFree()) return -ENOSPC;
        while (clusters.size() < needed)
        {
            uint32_t c = allocCluster(clusters.empty() ? 0 : clusters.back());
            if (!c) return -EIO;
            clusters.push_back(c);
        }
        return 0;
    }

    // Free every cluster of 'clusters' from index 'from' on; terminate the rest.
    void truncateChain(std::vector<uint32_t>& clusters, size_t from)
    {
        for (size_t i = from; i < clusters.size(); i++) fatSet(clusters[i], 0);
        if (from > 0 && from <= clusters.size()) fatSet(clusters[from - 1], eocValue());
        clusters.resize(std::min(from, clusters.size()));
    }

    // Read (rbuf) or write (wbuf, or zeros when both are null) a byte range of a cluster chain.
    int ioRange(const std::vector<uint32_t>& clusters, uint64_t pos, uint8_t* rbuf,
                const uint8_t* wbuf, size_t len)
    {
        const uint32_t cb = clusterBytes();
        size_t done = 0;
        while (done < len)
        {
            uint64_t p = pos + done;
            size_t ci = (size_t)(p / cb);
            if (ci >= clusters.size()) break;
            uint32_t within = (uint32_t)(p % cb);
            uint32_t sector = clusterToSector(clusters[ci]) + within / 512;
            uint32_t secOff = within % 512;
            size_t n = std::min<size_t>(512 - secOff, len - done);
            uint8_t sb[512];
            if (rbuf)
            {
                if (!disk->readSector(sector, sb)) return done ? (int)done : -EIO;
                memcpy(rbuf + done, sb + secOff, n);
            }
            else
            {
                if (n < 512 && !disk->readSector(sector, sb)) return done ? (int)done : -EIO;
                if (wbuf) memcpy(sb + secOff, wbuf + done, n);
                else      memset(sb + secOff, 0, n);
                if (!disk->writeSector(sector, sb)) return done ? (int)done : -EIO;
            }
            done += n;
        }
        return (int)done;
    }

    // ── Directories ───────────────────────────────────────────────────────────
    std::vector<uint32_t> rootDirSectors() const
    {
        std::vector<uint32_t> secs;
        for (uint32_t i = 0; i < bpb.rootDirSectors; i++) secs.push_back(bpb.rootDirSec + i);
        return secs;
    }

    std::vector<uint32_t> dirSectors(uint32_t cluster) const
    {
        if (cluster == 0) return rootDirSectors();
        std::vector<uint32_t> secs;
        for (uint32_t c : chain(cluster))
        {
            uint32_t base = clusterToSector(c);
            for (uint8_t s = 0; s < bpb.secPerClus; s++) secs.push_back(base + s);
        }
        return secs;
    }

    // List a directory ('.' and '..' included).  0 or -EIO.
    int readDir(uint32_t cluster, std::vector<DirItem>& items) const
    {
        items.clear();
        std::map<int, std::u16string> parts;
        std::vector<Slot> lfnSlots;
        int lfnCount = 0, expected = 0;
        uint8_t lfnSum = 0;
        auto reset = [&] { parts.clear(); lfnSlots.clear(); lfnCount = expected = 0; };

        for (uint32_t sec : dirSectors(cluster))
        {
            uint8_t buf[512];
            if (!disk->readSector(sec, buf)) return -EIO;
            for (uint32_t i = 0; i < 16; i++)
            {
                const uint8_t* e = buf + i * 32;
                if (e[0] == 0x00) return 0;
                if (e[0] == 0xE5)
                {
                    reset();
                    continue;
                }

                if ((e[11] & 0x3F) == ATTR_LFN)
                {
                    int seq = e[0] & 0x1F;
                    if (e[0] & 0x40)
                    {
                        reset();
                        lfnCount = seq;
                        lfnSum   = e[13];
                        expected = seq;
                    }
                    if (seq == 0 || seq != expected || e[13] != lfnSum)
                    {
                        reset();
                        continue;
                    }
                    static const int offs[13] = {1,3,5,7,9, 14,16,18,20,22,24, 28,30};
                    std::u16string frag;
                    for (int k = 0; k < 13; k++)
                    {
                        char16_t c = (char16_t)le16(e + offs[k]);
                        if (c == 0x0000) break;
                        frag += c;
                    }
                    parts[seq] = frag;
                    lfnSlots.push_back({sec, i});
                    expected = seq - 1;
                    continue;
                }

                if (e[11] & ATTR_VOLUME)
                {
                    reset();
                    continue;
                }

                DirItem item;
                memcpy(item.raw, e, 32);
                item.slot = {sec, i};
                item.shortName = shortNameOf(e);
                item.name = item.shortName;
                if (lfnCount > 0 && expected == 0 && lfnChecksum(e) == lfnSum)
                {
                    std::u16string u;
                    for (int k = 1; k <= lfnCount; k++) u += parts[k];
                    item.name = utf16ToUtf8(u);
                    item.lfnSlots = lfnSlots;
                }
                reset();
                items.push_back(std::move(item));
            }
        }
        return 0;
    }

    // Resolve a path.  For "/" isRoot is set and item is synthetic.
    int lookup(const char* path, DirItem& item, uint32_t& parentCluster, bool& isRoot) const
    {
        std::vector<std::string> parts;
        std::string cur;
        for (const char* p = path; *p; p++)
        {
            if (*p == '/')
            {
                if (!cur.empty())
                {
                    parts.push_back(cur);
                    cur.clear();
                }
            }
            else cur += *p;
        }
        if (!cur.empty()) parts.push_back(cur);

        isRoot = parts.empty();
        parentCluster = 0;
        if (isRoot)
        {
            item = DirItem{};
            item.raw[11] = ATTR_DIR;
            item.name = "/";
            return 0;
        }

        uint32_t dirCluster = 0;
        for (size_t pi = 0; pi < parts.size(); pi++)
        {
            std::vector<DirItem> items;
            int err = readDir(dirCluster, items);
            if (err) return err;
            bool found = false;
            for (auto& it : items)
            {
                if (it.shortName == "." || it.shortName == "..") continue;
                if (!nameEq(it.name, parts[pi]) && !nameEq(it.shortName, parts[pi])) continue;
                if (pi == parts.size() - 1)
                {
                    item = it;
                    parentCluster = dirCluster;
                    return 0;
                }
                if (!it.isDir()) return -ENOTDIR;
                dirCluster = it.cluster();
                if (dirCluster == 0) return -EIO;   // corrupt: subdirectory without clusters
                found = true;
                break;
            }
            if (!found) return -ENOENT;
        }
        return -ENOENT;
    }

    bool readSlot(const Slot& s, uint8_t raw[32]) const
    {
        uint8_t buf[512];
        if (!disk->readSector(s.sector, buf)) return false;
        memcpy(raw, buf + s.idx * 32, 32);
        return true;
    }

    bool writeSlot(const Slot& s, const uint8_t raw[32])
    {
        uint8_t buf[512];
        if (!disk->readSector(s.sector, buf)) return false;
        memcpy(buf + s.idx * 32, raw, 32);
        return disk->writeSector(s.sector, buf);
    }

    // Find 'n' consecutive free slots, extending a subdirectory if needed.
    int findFreeRun(uint32_t dirCluster, size_t n, std::vector<Slot>& out)
    {
        for (int pass = 0; pass < 2; pass++)
        {
            out.clear();
            for (uint32_t sec : dirSectors(dirCluster))
            {
                uint8_t buf[512];
                if (!disk->readSector(sec, buf)) return -EIO;
                for (uint32_t i = 0; i < 16; i++)
                {
                    uint8_t first = buf[i * 32];
                    if (first == 0x00 || first == 0xE5)
                    {
                        out.push_back({sec, i});
                        if (out.size() == n) return 0;
                    }
                    else
                    {
                        out.clear();
                    }
                }
            }
            if (dirCluster == 0) return -ENOSPC;   // the root directory has a fixed size
            // Add clusters to the subdirectory until the run fits
            std::vector<uint32_t> clusters = chain(dirCluster);
            uint32_t perCluster = clusterBytes() / 32;
            uint32_t extra = (uint32_t)((n - out.size() + perCluster - 1) / perCluster);
            int err = extendChain(clusters, (uint32_t)clusters.size() + extra);
            if (err) return err;
        }
        return -ENOSPC;
    }

    // Is this 11-byte short name already used in the directory?
    static bool shortNameTaken(const std::vector<DirItem>& items, const uint8_t* sn)
    {
        for (auto& it : items)
            if (memcmp(it.raw, sn, 11) == 0) return true;
        return false;
    }

    static bool validShortChar(unsigned char c)
    {
        if (c >= 'A' && c <= 'Z') return true;
        if (c >= '0' && c <= '9') return true;
        return strchr("!#$%&'()-@^_`{}~", c) != nullptr && c != 0;
    }

    // Build the short name for 'name'.  needLfn tells
    // whether long-name entries are required to preserve the name.
    static void makeShortName(const std::string& name, const std::vector<DirItem>& items,
                              uint8_t sn[11], bool& needLfn)
    {
        size_t lastDot = name.rfind('.');
        if (lastDot == 0) lastDot = std::string::npos;      // ".profile": no extension
        std::string base = lastDot == std::string::npos ? name : name.substr(0, lastDot);
        std::string ext  = lastDot == std::string::npos ? "" : name.substr(lastDot + 1);

        bool lossy = false;
        auto filter = [&](const std::string& in)
        {
            std::string out;
            for (unsigned char c : in)
            {
                if (c == ' ' || c == '.')
                {
                    lossy = true;
                    continue;
                }
                unsigned char u = (unsigned char)toupper(c);
                if (c < 0x80 && validShortChar(u)) out += (char)u;
                else
                {
                    out += '_';
                    lossy = true;
                }
            }
            return out;
        };
        std::string fb = filter(base), fe = filter(ext);
        if (fb.size() > 8 || fe.size() > 3) lossy = true;

        // Like Linux (shortname=mixed): a plain short entry only for names that
        // are all upper case already; anything else keeps its case in a long
        // name.  (NT lower-case flags are honoured when reading, but not every
        // system understands them, so they are not used when writing.)
        bool hasLower = false;
        for (unsigned char c : name) if (islower(c)) hasLower = true;
        needLfn = lossy || hasLower;

        memset(sn, ' ', 11);
        if (fb.empty()) fb = "_";
        if (!needLfn)
        {
            memcpy(sn, fb.data(), fb.size());
            memcpy(sn + 8, fe.data(), fe.size());
            if (!shortNameTaken(items, sn)) return;
            needLfn = true;
        }

        // Numeric tail: BASE~N.EXT
        for (uint32_t n = 1; n < 1000000; n++)
        {
            std::string tail = "~" + std::to_string(n);
            std::string b = fb.substr(0, 8 - tail.size()) + tail;
            memset(sn, ' ', 11);
            memcpy(sn, b.data(), b.size());
            memcpy(sn + 8, fe.data(), std::min<size_t>(3, fe.size()));
            if (!shortNameTaken(items, sn)) return;
        }
    }

    static int checkName(const std::string& name, std::u16string& u16)
    {
        if (name.empty() || name == "." || name == "..") return -EINVAL;
        if (!utf8ToUtf16(name, u16)) return -EILSEQ;
        if (u16.size() > 255) return -ENAMETOOLONG;
        for (char16_t c : u16)
            if (c < 0x20 || (c < 0x80 && strchr("\"*/:<>?\\|", (char)c))) return -EINVAL;
        if (name.back() == '.' || name.back() == ' ') return -EINVAL;
        return 0;
    }

    // Create the directory entries for 'name' in 'dirCluster'.  'tmpl' holds the
    // attributes, timestamps, first cluster and size of the new entry.
    int createEntry(uint32_t dirCluster, const std::string& name, const uint8_t tmpl[32], DirItem& out)
    {
        std::u16string u16;
        int err = checkName(name, u16);
        if (err) return err;

        std::vector<DirItem> items;
        err = readDir(dirCluster, items);
        if (err) return err;

        uint8_t sn[11];
        bool needLfn;
        makeShortName(name, items, sn, needLfn);

        size_t nLfn = needLfn ? (u16.size() + 12) / 13 : 0;
        std::vector<Slot> slots;
        err = findFreeRun(dirCluster, nLfn + 1, slots);
        if (err) return err;

        uint8_t sum = lfnChecksum(sn);
        for (size_t k = 0; k < nLfn; k++)
        {
            uint8_t e[32] = {};
            int seq = (int)(nLfn - k);                // entries are stored last part first
            e[0]  = (uint8_t)(seq | (k == 0 ? 0x40 : 0));
            e[11] = ATTR_LFN;
            e[13] = sum;
            static const int offs[13] = {1,3,5,7,9, 14,16,18,20,22,24, 28,30};
            for (int c = 0; c < 13; c++)
            {
                size_t pos = (size_t)(seq - 1) * 13 + c;
                uint16_t v = pos < u16.size() ? u16[pos] : (pos == u16.size() ? 0x0000 : 0xFFFF);
                put_le16(e + offs[c], v);
            }
            if (!writeSlot(slots[k], e)) return -EIO;
        }

        uint8_t e[32];
        memcpy(e, tmpl, 32);
        memcpy(e, sn, 11);
        e[12] = 0;   // no NT case flags
        if (!writeSlot(slots[nLfn], e)) return -EIO;

        out = DirItem{};
        memcpy(out.raw, e, 32);
        out.slot = slots[nLfn];
        out.lfnSlots.assign(slots.begin(), slots.begin() + nLfn);
        out.shortName = shortNameOf(e);
        out.name = needLfn ? name : out.shortName;
        return 0;
    }

    int deleteEntry(const DirItem& it)
    {
        uint8_t raw[32];
        for (const Slot& s : it.lfnSlots)
        {
            if (!readSlot(s, raw)) return -EIO;
            raw[0] = 0xE5;
            if (!writeSlot(s, raw)) return -EIO;
        }
        memcpy(raw, it.raw, 32);
        raw[0] = 0xE5;
        return writeSlot(it.slot, raw) ? 0 : -EIO;
    }

    // Split "/a/b/c" into the parent's cluster and "c"
    int parentOf(const char* path, uint32_t& parentCluster, std::string& name) const
    {
        std::string s(path);
        while (s.size() > 1 && s.back() == '/') s.pop_back();
        size_t sep = s.rfind('/');
        if (sep == std::string::npos) return -EINVAL;
        name = s.substr(sep + 1);
        std::string parent = sep == 0 ? "/" : s.substr(0, sep);
        DirItem pi;
        uint32_t pp;
        bool root;
        int err = lookup(parent.c_str(), pi, pp, root);
        if (err) return err;
        if (!root && !pi.isDir()) return -ENOTDIR;
        parentCluster = root ? 0 : pi.cluster();
        return 0;
    }

    // Cluster of the parent of directory 'cluster' (via its ".." entry)
    uint32_t dotdotOf(uint32_t cluster) const
    {
        uint8_t buf[512];
        if (!disk->readSector(clusterToSector(cluster), buf)) return 0;
        if (buf[32] == '.' && buf[33] == '.') return le16(buf + 32 + 26);
        return 0;
    }
};

// ── FUSE operation implementations ───────────────────────────────────────────

static FatFs* getFS()
{
    return static_cast<FatFs*>(fuse_get_context()->private_data);
}

static void fillStat(FatFs* fs, const DirItem& it, bool isRoot, struct stat* st)
{
    memset(st, 0, sizeof(*st));
    const bool rw = !fs->ro();
    if (isRoot || it.isDir())
    {
        st->st_mode  = S_IFDIR | (rw ? 0755 : 0555);
        st->st_nlink = 2;
    }
    else
    {
        bool fileRw = rw && !(it.raw[11] & ATTR_RO);
        st->st_mode  = S_IFREG | (fileRw ? 0644 : 0444);
        st->st_nlink = 1;
        st->st_size  = it.fileSize();
    }
    st->st_blksize = fs->clusterBytes();
    st->st_blocks  = ((st->st_size + fs->clusterBytes() - 1) / fs->clusterBytes()) *
                     (fs->clusterBytes() / 512);
    st->st_uid = getuid();
    st->st_gid = getgid();
    if (!isRoot)
    {
        st->st_mtime = dosToUnix(le16(it.raw + 24), le16(it.raw + 22));
        st->st_atime = dosToUnix(le16(it.raw + 18), 0);
        st->st_ctime = dosToUnix(le16(it.raw + 16), le16(it.raw + 14));
        if (!st->st_atime) st->st_atime = st->st_mtime;
        if (!st->st_ctime) st->st_ctime = st->st_mtime;
    }
}

static int fat_getattr(const char* path, struct stat* st, struct fuse_file_info*)
{
    FatFs* fs = getFS();
    DirItem it;
    uint32_t parent;
    bool root;
    int err = fs->lookup(path, it, parent, root);
    if (err) return err;
    fillStat(fs, it, root, st);
    return 0;
}

static int fat_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                       off_t, struct fuse_file_info*, enum fuse_readdir_flags)
{
    FatFs* fs = getFS();
    DirItem dir;
    uint32_t parent;
    bool root;
    int err = fs->lookup(path, dir, parent, root);
    if (err) return err;
    if (!root && !dir.isDir()) return -ENOTDIR;

    std::vector<DirItem> items;
    err = fs->readDir(root ? 0 : dir.cluster(), items);
    if (err) return err;

    filler(buf, ".",  nullptr, 0, (fuse_fill_dir_flags)0);
    filler(buf, "..", nullptr, 0, (fuse_fill_dir_flags)0);
    for (auto& item : items)
    {
        if (item.shortName == "." || item.shortName == "..") continue;
        struct stat st;
        fillStat(fs, item, false, &st);
        filler(buf, item.name.c_str(), &st, 0, (fuse_fill_dir_flags)0);
    }
    return 0;
}

static int resizeFile(FatFs* fs, DirItem& it, uint64_t newSize);

static int fat_open(const char* path, struct fuse_file_info* fi)
{
    FatFs* fs = getFS();
    DirItem it;
    uint32_t parent;
    bool root;
    int err = fs->lookup(path, it, parent, root);
    if (err) return err;
    if (root || it.isDir()) return -EISDIR;
    if ((fi->flags & O_ACCMODE) != O_RDONLY && (fs->ro() || (it.raw[11] & ATTR_RO)))
        return fs->ro() ? -EROFS : -EACCES;
    // libfuse enables atomic O_TRUNC: the truncation arrives here, not via truncate()
    if ((fi->flags & O_ACCMODE) != O_RDONLY && (fi->flags & O_TRUNC) && it.fileSize() > 0)
        return resizeFile(fs, it, 0);
    return 0;
}

static int fat_read(const char* path, char* buf, size_t size, off_t offset,
                    struct fuse_file_info*)
{
    FatFs* fs = getFS();
    DirItem it;
    uint32_t parent;
    bool root;
    int err = fs->lookup(path, it, parent, root);
    if (err) return err;
    if (root || it.isDir()) return -EISDIR;

    uint32_t fileSize = it.fileSize();
    if (offset < 0 || (uint64_t)offset >= fileSize) return 0;
    size_t n = std::min<uint64_t>(size, fileSize - (uint64_t)offset);
    int r = fs->ioRange(fs->chain(it.cluster()), (uint64_t)offset, (uint8_t*)buf, nullptr, n);
    // Never return a short read (bad sector, broken cluster chain): the kernel
    // would take it as end of file and silently shrink the file.
    if (r >= 0 && (size_t)r < n) return -EIO;
    return r;
}

// Resize the file to 'newSize' (allocating zeroed clusters or freeing them)
// and update the directory entry.
static int resizeFile(FatFs* fs, DirItem& it, uint64_t newSize)
{
    if (newSize > 0xFFFFFFFFull) return -EFBIG;
    const uint32_t cb = fs->clusterBytes();
    const uint32_t oldSize = it.fileSize();
    std::vector<uint32_t> clusters = fs->chain(it.cluster());
    const uint32_t needed = (uint32_t)((newSize + cb - 1) / cb);

    if (newSize > oldSize)
    {
        // Zero the tail of the last cluster beyond the old end of file
        uint64_t tailEnd = std::min<uint64_t>(newSize, (uint64_t)clusters.size() * cb);
        if (tailEnd > oldSize)
        {
            int r = fs->ioRange(clusters, oldSize, nullptr, nullptr, (size_t)(tailEnd - oldSize));
            if (r < 0) return r;
        }
        int err = fs->extendChain(clusters, needed);
        if (err)
        {
            fs->flushFAT();
            return err;
        }
    }
    else if (clusters.size() > needed)
    {
        fs->truncateChain(clusters, needed);
    }

    put_le32(it.raw + 28, (uint32_t)newSize);
    put_le16(it.raw + 26, clusters.empty() ? 0 : (uint16_t)clusters[0]);
    setMtime(it.raw, time(nullptr));
    it.raw[11] |= ATTR_ARCH;
    bool ok = fs->writeSlot(it.slot, it.raw);
    ok &= fs->flushFAT();
    return ok ? 0 : -EIO;
}

static int fat_write(const char* path, const char* buf, size_t size, off_t offset,
                     struct fuse_file_info*)
{
    FatFs* fs = getFS();
    if (fs->ro()) return -EROFS;
    DirItem it;
    uint32_t parent;
    bool root;
    int err = fs->lookup(path, it, parent, root);
    if (err) return err;
    if (root || it.isDir()) return -EISDIR;
    if (offset < 0) return -EINVAL;

    const uint64_t end = (uint64_t)offset + size;
    if (end > it.fileSize())
    {
        err = resizeFile(fs, it, end);
        if (err) return err;
    }
    int n = fs->ioRange(fs->chain(it.cluster()), (uint64_t)offset, nullptr,
                        (const uint8_t*)buf, size);
    if (n < 0) return n;

    setMtime(it.raw, time(nullptr));
    it.raw[11] |= ATTR_ARCH;
    if (!fs->writeSlot(it.slot, it.raw)) return -EIO;
    return n;
}

static int fat_truncate(const char* path, off_t size, struct fuse_file_info*)
{
    FatFs* fs = getFS();
    if (fs->ro()) return -EROFS;
    if (size < 0) return -EINVAL;
    DirItem it;
    uint32_t parent;
    bool root;
    int err = fs->lookup(path, it, parent, root);
    if (err) return err;
    if (root || it.isDir()) return -EISDIR;
    return resizeFile(fs, it, (uint64_t)size);
}

static int fat_create(const char* path, mode_t, struct fuse_file_info*)
{
    FatFs* fs = getFS();
    if (fs->ro()) return -EROFS;

    DirItem it;
    uint32_t pc;
    bool root;
    if (fs->lookup(path, it, pc, root) == 0) return -EEXIST;

    uint32_t parent;
    std::string name;
    int err = fs->parentOf(path, parent, name);
    if (err) return err;

    uint8_t tmpl[32] = {};
    tmpl[11] = ATTR_ARCH;
    uint16_t d, t;
    unixToDos(time(nullptr), d, t);
    put_le16(tmpl + 14, t);
    put_le16(tmpl + 16, d);
    put_le16(tmpl + 18, d);
    put_le16(tmpl + 22, t);
    put_le16(tmpl + 24, d);

    DirItem out;
    err = fs->createEntry(parent, name, tmpl, out);
    fs->flushFAT();
    return err;
}

static int fat_unlink(const char* path)
{
    FatFs* fs = getFS();
    if (fs->ro()) return -EROFS;
    DirItem it;
    uint32_t parent;
    bool root;
    int err = fs->lookup(path, it, parent, root);
    if (err) return err;
    if (root || it.isDir()) return -EISDIR;

    std::vector<uint32_t> clusters = fs->chain(it.cluster());
    fs->truncateChain(clusters, 0);
    err = fs->deleteEntry(it);
    if (!fs->flushFAT() && !err) err = -EIO;
    return err;
}

static int fat_mkdir(const char* path, mode_t)
{
    FatFs* fs = getFS();
    if (fs->ro()) return -EROFS;

    DirItem ex;
    uint32_t pc;
    bool root;
    if (fs->lookup(path, ex, pc, root) == 0) return -EEXIST;

    uint32_t parent;
    std::string name;
    int err = fs->parentOf(path, parent, name);
    if (err) return err;
    std::u16string u16;
    err = FatFs::checkName(name, u16);
    if (err) return err;

    uint32_t cl = fs->allocCluster(0);
    if (!cl)
    {
        fs->flushFAT();
        return -ENOSPC;
    }

    uint16_t d, t;
    unixToDos(time(nullptr), d, t);

    // "." and ".." entries
    uint8_t buf[512] = {};
    memset(buf, ' ', 11);
    buf[0] = '.';
    buf[11] = ATTR_DIR;
    put_le16(buf + 22, t);
    put_le16(buf + 24, d);
    put_le16(buf + 26, (uint16_t)cl);
    memset(buf + 32, ' ', 11);
    buf[32] = '.';
    buf[33] = '.';
    buf[43] = ATTR_DIR;
    put_le16(buf + 54, t);
    put_le16(buf + 56, d);
    put_le16(buf + 58, (uint16_t)parent);
    if (!fs->disk->writeSector(fs->clusterToSector(cl), buf)) err = -EIO;

    if (!err)
    {
        uint8_t tmpl[32] = {};
        tmpl[11] = ATTR_DIR;
        put_le16(tmpl + 14, t);
        put_le16(tmpl + 16, d);
        put_le16(tmpl + 18, d);
        put_le16(tmpl + 22, t);
        put_le16(tmpl + 24, d);
        put_le16(tmpl + 26, (uint16_t)cl);
        DirItem out;
        err = fs->createEntry(parent, name, tmpl, out);
    }
    if (err) fs->fatSet(cl, 0);
    if (!fs->flushFAT() && !err) err = -EIO;
    return err;
}

static int fat_rmdir(const char* path)
{
    FatFs* fs = getFS();
    if (fs->ro()) return -EROFS;
    DirItem it;
    uint32_t parent;
    bool root;
    int err = fs->lookup(path, it, parent, root);
    if (err) return err;
    if (root) return -EBUSY;
    if (!it.isDir()) return -ENOTDIR;

    std::vector<DirItem> items;
    err = fs->readDir(it.cluster(), items);
    if (err) return err;
    for (auto& i : items)
        if (i.shortName != "." && i.shortName != "..") return -ENOTEMPTY;

    std::vector<uint32_t> clusters = fs->chain(it.cluster());
    fs->truncateChain(clusters, 0);
    err = fs->deleteEntry(it);
    if (!fs->flushFAT() && !err) err = -EIO;
    return err;
}

static int fat_rename(const char* from, const char* to, unsigned int flags)
{
    if (flags) return -EINVAL; // RENAME_EXCHANGE / RENAME_NOREPLACE not supported
    FatFs* fs = getFS();
    if (fs->ro()) return -EROFS;

    DirItem src;
    uint32_t srcParent;
    bool root;
    int err = fs->lookup(from, src, srcParent, root);
    if (err) return err;
    if (root) return -EBUSY;

    uint32_t dstParent;
    std::string dstName;
    err = fs->parentOf(to, dstParent, dstName);
    if (err) return err;
    std::u16string u16;
    err = FatFs::checkName(dstName, u16);
    if (err) return err;

    // A directory cannot be moved below itself
    if (src.isDir())
    {
        uint32_t p = dstParent;
        for (uint32_t guard = 0; p != 0 && guard < 1000; guard++)
        {
            if (p == src.cluster()) return -EINVAL;
            p = fs->dotdotOf(p);
        }
    }

    DirItem dst;
    uint32_t dp;
    bool droot;
    const bool dstExists = fs->lookup(to, dst, dp, droot) == 0;
    const bool sameEntry = dstExists && dst.slot.sector == src.slot.sector &&
                           dst.slot.idx == src.slot.idx;   // e.g. a case-only rename
    if (dstExists && !sameEntry)
    {
        if (src.isDir())
        {
            if (!dst.isDir()) return -ENOTDIR;
            err = fat_rmdir(to);
        }
        else
        {
            if (dst.isDir()) return -EISDIR;
            err = fat_unlink(to);
        }
        if (err) return err;
        // Slots may have moved: look the source up again
        err = fs->lookup(from, src, srcParent, root);
        if (err) return err;
    }

    uint8_t tmpl[32];
    memcpy(tmpl, src.raw, 32);
    err = fs->deleteEntry(src);
    if (err) return err;

    DirItem out;
    err = fs->createEntry(dstParent, dstName, tmpl, out);
    if (err)
    {
        // Put the old entry back so the file is not lost
        std::string srcName;
        uint32_t sp;
        if (fs->parentOf(from, sp, srcName) == 0) fs->createEntry(srcParent, srcName, tmpl, out);
        fs->flushFAT();
        return err;
    }

    // A moved directory must point its ".." at the new parent
    if (src.isDir() && dstParent != srcParent && src.cluster() >= 2)
    {
        uint8_t buf[512];
        uint32_t sec = fs->clusterToSector(src.cluster());
        if (fs->disk->readSector(sec, buf) && buf[32] == '.' && buf[33] == '.')
        {
            put_le16(buf + 32 + 26, (uint16_t)dstParent);
            fs->disk->writeSector(sec, buf);
        }
    }
    return fs->flushFAT() ? 0 : -EIO;
}

static int fat_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info*)
{
    FatFs* fs = getFS();
    if (fs->ro()) return -EROFS;
    DirItem it;
    uint32_t parent;
    bool root;
    int err = fs->lookup(path, it, parent, root);
    if (err) return err;
    if (root) return 0;
    if (tv && tv[1].tv_nsec == UTIME_OMIT) return 0;
    time_t t = (tv && tv[1].tv_nsec != UTIME_NOW) ? tv[1].tv_sec : time(nullptr);
    setMtime(it.raw, t);
    return fs->writeSlot(it.slot, it.raw) ? 0 : -EIO;
}

static int fat_chmod(const char* path, mode_t mode, struct fuse_file_info*)
{
    // Map the owner write bit to the DOS read-only attribute
    FatFs* fs = getFS();
    if (fs->ro()) return -EROFS;
    DirItem it;
    uint32_t parent;
    bool root;
    int err = fs->lookup(path, it, parent, root);
    if (err) return err;
    if (root || it.isDir()) return 0;
    uint8_t attr = it.raw[11];
    if (mode & S_IWUSR) attr &= (uint8_t)~ATTR_RO;
    else attr |= ATTR_RO;
    if (attr == it.raw[11]) return 0;
    it.raw[11] = attr;
    return fs->writeSlot(it.slot, it.raw) ? 0 : -EIO;
}

static int fat_chown(const char*, uid_t, gid_t, struct fuse_file_info*)
{
    return 0;
}

static int fat_fsync(const char*, int, struct fuse_file_info*)
{
    return getFS()->disk->flush() ? 0 : -EIO;
}

static int fat_statfs(const char*, struct statvfs* sv)
{
    FatFs* fs = getFS();
    uint32_t free_clus = fs->countFree();
    memset(sv, 0, sizeof(*sv));
    sv->f_bsize   = fs->clusterBytes();
    sv->f_frsize  = fs->clusterBytes();
    sv->f_blocks  = fs->bpb.dataClusterCount;
    sv->f_bfree   = free_clus;
    sv->f_bavail  = free_clus;
    sv->f_namemax = 255;
    return 0;
}

static void* fat_init(struct fuse_conn_info*, struct fuse_config* cfg)
{
    cfg->kernel_cache = 0;
    return fuse_get_context()->private_data;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool fat_detect(IDiskImage* disk)
{
    uint8_t sec0[512] = {};
    if (!disk->readSector(0, sec0)) return false;
    BPB bpb;
    return parseBPB(sec0, bpb);
}

struct fuse_operations fat_get_operations()
{
    struct fuse_operations ops = {};
    ops.init     = fat_init;
    ops.getattr  = fat_getattr;
    ops.readdir  = fat_readdir;
    ops.open     = fat_open;
    ops.read     = fat_read;
    ops.write    = fat_write;
    ops.create   = fat_create;
    ops.unlink   = fat_unlink;
    ops.mkdir    = fat_mkdir;
    ops.rmdir    = fat_rmdir;
    ops.rename   = fat_rename;
    ops.truncate = fat_truncate;
    ops.utimens  = fat_utimens;
    ops.chmod    = fat_chmod;
    ops.chown    = fat_chown;
    ops.fsync    = fat_fsync;
    ops.statfs   = fat_statfs;
    return ops;
}

// ── Factory functions ─────────────────────────────────────────────────────────

void* fat_fs_new(IDiskImage* disk)
{
    FatFs* fs = new FatFs();
    if (!fs->init(disk))
    {
        delete fs;
        return nullptr;
    }
    return fs;
}

void fat_fs_free(void* ctx)
{
    FatFs* fs = static_cast<FatFs*>(ctx);
    fs->flushFAT();
    delete fs;
}

bool fat_is_fat16(void* ctx)
{
    return static_cast<FatFs*>(ctx)->bpb.isFAT16;
}

std::string fat_volume_label(void* ctx)
{
    return static_cast<FatFs*>(ctx)->volumeLabel;
}
