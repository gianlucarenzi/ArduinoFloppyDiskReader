// waffle-fuse: FAT12 / FAT16 FUSE filesystem driver
// Supports read + write, LFN (long filenames) read, 8.3 create.

#define FUSE_USE_VERSION 31
#include "fat_fs.h"

#include <cstring>
#include <cassert>
#include <cctype>
#include <ctime>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <errno.h>

// ── BPB (BIOS Parameter Block) ────────────────────────────────────────────────
// All fields little-endian; we read byte-by-byte to avoid alignment issues.

static inline uint16_t le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static inline void put_le16(uint8_t* p, uint16_t v) {
    p[0] = v & 0xFF; p[1] = v >> 8;
}
static inline void put_le32(uint8_t* p, uint32_t v) {
    p[0] = v; p[1] = v>>8; p[2] = v>>16; p[3] = v>>24;
}

struct BPB {
    uint16_t bytesPerSec;
    uint8_t  secPerClus;
    uint16_t rsvdSecCnt;
    uint8_t  numFATs;
    uint16_t rootEntCnt;
    uint32_t totSec;
    uint16_t fatSz;        // FAT sectors per copy
    uint16_t secPerTrk;
    uint16_t numHeads;
    uint32_t firstDataSec; // computed
    uint32_t rootDirSec;   // first sector of root dir
    uint32_t rootDirSectors;
    uint32_t dataClusterCount;
    bool     isFAT16;
};

static bool parseBPB(const uint8_t* sec0, BPB& bpb)
{
    // Must start with a valid x86 jump opcode
    if (sec0[0] != 0xEB && sec0[0] != 0xE9) return false;
    // Must have the MBR/VBR signature
    if (sec0[510] != 0x55 || sec0[511] != 0xAA) return false;

    bpb.bytesPerSec  = le16(sec0 + 11);
    bpb.secPerClus   = sec0[13];
    bpb.rsvdSecCnt   = le16(sec0 + 14);
    bpb.numFATs      = sec0[16];
    bpb.rootEntCnt   = le16(sec0 + 17);
    uint16_t totSec16 = le16(sec0 + 19);
    bpb.fatSz        = le16(sec0 + 22);
    bpb.secPerTrk    = le16(sec0 + 24);
    bpb.numHeads     = le16(sec0 + 26);
    uint32_t totSec32 = le32(sec0 + 32);

    bpb.totSec = totSec16 ? totSec16 : totSec32;

    // Sanity-check BPB fields to reject Amiga/other disks that accidentally
    // pass the signature check
    if (bpb.bytesPerSec == 0 || bpb.bytesPerSec > 4096) return false;
    if ((bpb.bytesPerSec & (bpb.bytesPerSec - 1)) != 0) return false; // must be power of 2
    if (bpb.secPerClus == 0) return false;
    if ((bpb.secPerClus & (bpb.secPerClus - 1)) != 0) return false;  // must be power of 2
    if (bpb.numFATs == 0 || bpb.numFATs > 2) return false;
    if (bpb.rsvdSecCnt == 0) return false;
    if (bpb.fatSz == 0) return false;
    if (bpb.totSec == 0) return false;

    bpb.rootDirSectors = ((uint32_t)bpb.rootEntCnt * 32 + bpb.bytesPerSec - 1)
                         / bpb.bytesPerSec;
    bpb.firstDataSec   = bpb.rsvdSecCnt + bpb.numFATs * bpb.fatSz + bpb.rootDirSectors;
    bpb.rootDirSec     = bpb.rsvdSecCnt + bpb.numFATs * bpb.fatSz;

    uint32_t dataSectors = bpb.totSec > bpb.firstDataSec
                           ? bpb.totSec - bpb.firstDataSec : 0;
    bpb.dataClusterCount = dataSectors / bpb.secPerClus;
    bpb.isFAT16          = bpb.dataClusterCount >= 4085; // FAT12 < 4085 clusters
    return true;
}

// ── FAT table access ─────────────────────────────────────────────────────────

class FatFs {
public:
    IDiskImage* disk = nullptr;
    BPB         bpb{};
    std::vector<uint8_t> fat; // cached FAT copy 1

    // ── Init ──────────────────────────────────────────────────────────────────
    bool init(IDiskImage* d) {
        disk = d;
        uint8_t sec0[512];
        if (!disk->readSector(0, sec0)) return false;
        if (!parseBPB(sec0, bpb)) return false;

        // Load FAT table (first copy) into memory
        fat.resize((size_t)bpb.fatSz * bpb.bytesPerSec);
        for (uint32_t i = 0; i < bpb.fatSz; i++) {
            uint8_t buf[512] = {};
            if (disk->readSector(bpb.rsvdSecCnt + i, buf))
                memcpy(fat.data() + i * bpb.bytesPerSec, buf, bpb.bytesPerSec);
        }
        return true;
    }

    // ── FAT entry read/write ──────────────────────────────────────────────────
    uint32_t fatGet(uint32_t cluster) const {
        if (bpb.isFAT16) {
            if (cluster * 2 + 1 >= fat.size()) return 0xFFFF;
            return le16(fat.data() + cluster * 2);
        } else {
            // FAT12
            uint32_t off = cluster + cluster / 2;
            if (off + 1 >= fat.size()) return 0xFFF;
            uint32_t v = fat[off] | ((uint32_t)fat[off + 1] << 8);
            return (cluster & 1) ? (v >> 4) : (v & 0x0FFF);
        }
    }

    bool fatEOC(uint32_t v) const {
        return bpb.isFAT16 ? v >= 0xFFF8 : v >= 0xFF8;
    }
    bool fatFree(uint32_t v) const { return v == 0; }

    void fatSet(uint32_t cluster, uint32_t val) {
        if (bpb.isFAT16) {
            put_le16(fat.data() + cluster * 2, (uint16_t)val);
        } else {
            uint32_t off = cluster + cluster / 2;
            if (off + 1 >= fat.size()) return;
            if (cluster & 1) {
                fat[off]     = (fat[off] & 0x0F) | ((val & 0x0F) << 4);
                fat[off + 1] = (uint8_t)(val >> 4);
            } else {
                fat[off]     = (uint8_t)(val & 0xFF);
                fat[off + 1] = (fat[off + 1] & 0xF0) | ((val >> 8) & 0x0F);
            }
        }
    }

    // Write FAT back to all copies on disk
    void flushFAT() {
        for (uint8_t copy = 0; copy < bpb.numFATs; copy++) {
            uint32_t base = bpb.rsvdSecCnt + copy * bpb.fatSz;
            for (uint32_t i = 0; i < bpb.fatSz; i++) {
                uint8_t buf[512] = {};
                size_t off = (size_t)i * bpb.bytesPerSec;
                if (off < fat.size())
                    memcpy(buf, fat.data() + off,
                           std::min<size_t>(bpb.bytesPerSec, fat.size() - off));
                disk->writeSector(base + i, buf);
            }
        }
    }

    // ── Cluster ↔ sector ──────────────────────────────────────────────────────
    uint32_t clusterToSector(uint32_t cluster) const {
        return bpb.firstDataSec + (cluster - 2) * bpb.secPerClus;
    }

    // Collect all sectors for a cluster chain starting at startCluster.
    std::vector<uint32_t> clusterChain(uint32_t startCluster) const {
        std::vector<uint32_t> sectors;
        if (startCluster < 2) return sectors;
        uint32_t cl = startCluster;
        while (cl >= 2 && !fatEOC(cl) && !fatFree(cl)) {
            uint32_t base = clusterToSector(cl);
            for (uint8_t s = 0; s < bpb.secPerClus; s++)
                sectors.push_back(base + s);
            cl = fatGet(cl);
        }
        return sectors;
    }

    // Allocate a free cluster, link it after 'prevCluster' (0 = new chain), mark EOC.
    // Returns 0 on failure.
    uint32_t allocCluster(uint32_t prevCluster = 0) {
        uint32_t eoc = bpb.isFAT16 ? 0xFFFF : 0x0FFF;
        uint32_t total = bpb.dataClusterCount + 2;
        for (uint32_t c = 2; c < total; c++) {
            if (fatFree(fatGet(c))) {
                fatSet(c, eoc);
                if (prevCluster >= 2) fatSet(prevCluster, c);
                flushFAT();
                // Zero out the cluster data
                uint32_t base = clusterToSector(c);
                uint8_t zero[512] = {};
                for (uint8_t s = 0; s < bpb.secPerClus; s++)
                    disk->writeSector(base + s, zero);
                return c;
            }
        }
        return 0; // disk full
    }

    // Free an entire cluster chain
    void freeChain(uint32_t startCluster) {
        if (startCluster < 2) return;
        uint32_t cl = startCluster;
        while (cl >= 2 && !fatEOC(cl) && !fatFree(cl)) {
            uint32_t next = fatGet(cl);
            fatSet(cl, 0);
            cl = next;
        }
        flushFAT();
    }

    // ── Directory entry helpers ───────────────────────────────────────────────
    // A directory entry is 32 bytes. Index 0..10 = name+ext.
    // Attr byte at offset 11.  Cluster low at 26, high (FAT32 only, =0) at 20.
    // Size at 28.

    struct DirEntry {
        uint8_t  raw[32];
        uint32_t sector;   // sector where this entry lives
        uint32_t idx;      // entry index within sector (0..bytesPerSec/32-1)

        bool isEnd()     const { return raw[0] == 0x00; }
        bool isDeleted() const { return raw[0] == 0xE5; }
        bool isLFN()     const { return (raw[11] & 0x3F) == 0x0F; }
        bool isDir()     const { return raw[11] & 0x10; }
        bool isVolLabel()const { return (raw[11] & 0x08) && !(raw[11] & 0x10); }

        uint8_t  attr()     const { return raw[11]; }
        uint32_t cluster()  const { return le16(raw + 26); } // FAT12/16
        uint32_t fileSize() const { return le32(raw + 28); }

        // Decode 8.3 name into a string (no trailing spaces, dot inserted)
        std::string shortName() const {
            char name[9] = {}, ext[4] = {};
            int ni = 0, ei = 0;
            for (int i = 0; i < 8; i++) {
                char c = raw[i];
                if (c == 0x05) c = 0xE5; // Kanji lead byte
                if (c == ' ') break;
                name[ni++] = c;
            }
            for (int i = 0; i < 3; i++) {
                char c = raw[8 + i];
                if (c == ' ') break;
                ext[ei++] = c;
            }
            std::string s(name);
            if (ei > 0) { s += '.'; s += ext; }
            return s;
        }

        // DOS date/time to Unix time_t
        time_t mtime() const {
            uint16_t date = le16(raw + 24); // WrtDate
            uint16_t time = le16(raw + 22); // WrtTime
            struct tm t = {};
            t.tm_year = ((date >> 9) & 0x7F) + 80;
            t.tm_mon  = ((date >> 5) & 0x0F) - 1;
            t.tm_mday = date & 0x1F;
            t.tm_hour = (time >> 11) & 0x1F;
            t.tm_min  = (time >> 5) & 0x3F;
            t.tm_sec  = (time & 0x1F) * 2;
            return mktime(&t);
        }

        // Write file size into raw entry
        void setSize(uint32_t sz) { put_le32(raw + 28, sz); }
        void setCluster(uint16_t cl) { put_le16(raw + 26, cl); }
        void markDeleted() { raw[0] = 0xE5; }
    };

    // Read all directory entries (including LFN) from a sequence of sectors.
    // Returns pairs (longName or shortName, DirEntry for the short entry).
    struct DirItem {
        std::string name;
        DirEntry    entry;
    };

    // Helper: decode a UTF-16LE LFN name fragment (up to 13 chars) into ASCII/UTF-8
    static std::string lfnFrag(const uint8_t* e) {
        // 5 chars at offset 1, 6 at offset 14, 2 at offset 28 → 13 UTF-16LE chars
        static const int offsets[] = {1,3,5,7,9, 14,16,18,20,22,24, 28,30};
        std::string s;
        for (int i = 0; i < 13; i++) {
            uint16_t c = le16(e + offsets[i]);
            if (c == 0x0000 || c == 0xFFFF) break;
            if (c < 0x80)       s += (char)c;
            else if (c < 0x800) { s += (char)(0xC0|(c>>6)); s += (char)(0x80|(c&0x3F)); }
            else                { s += (char)(0xE0|(c>>12)); s += (char)(0x80|((c>>6)&0x3F)); s += (char)(0x80|(c&0x3F)); }
        }
        return s;
    }

    std::vector<DirItem> readDir(const std::vector<uint32_t>& dirSectors) const {
        std::vector<DirItem> items;
        std::vector<std::string> lfnParts;

        for (uint32_t sec : dirSectors) {
            uint8_t buf[512] = {};
            if (!disk->readSector(sec, buf)) continue;

            const int entPerSec = bpb.bytesPerSec / 32;
            for (int i = 0; i < entPerSec; i++) {
                DirEntry e;
                memcpy(e.raw, buf + i * 32, 32);
                e.sector = sec;
                e.idx    = (uint32_t)i;

                if (e.isEnd()) goto done;
                if (e.isDeleted() || e.isVolLabel()) { lfnParts.clear(); continue; }

                if (e.isLFN()) {
                    lfnParts.insert(lfnParts.begin(), lfnFrag(e.raw));
                    continue;
                }

                // Short entry: assemble name
                DirItem item;
                item.entry = e;
                if (!lfnParts.empty()) {
                    for (auto& p : lfnParts) item.name += p;
                    lfnParts.clear();
                } else {
                    item.name = e.shortName();
                }
                items.push_back(std::move(item));
            }
        }
        done:
        return items;
    }

    std::vector<uint32_t> rootDirSectors() const {
        std::vector<uint32_t> secs;
        for (uint32_t i = 0; i < bpb.rootDirSectors; i++)
            secs.push_back(bpb.rootDirSec + i);
        return secs;
    }

    std::vector<uint32_t> dirSectors(uint32_t startCluster) const {
        if (startCluster == 0) return rootDirSectors();
        return clusterChain(startCluster);
    }

    // ── Path resolution ───────────────────────────────────────────────────────
    // Returns {found=true, entry, dirSectors} or {found=false}.
    struct LookupResult {
        bool      found = false;
        DirItem   item;
        std::vector<uint32_t> parentSectors; // sectors of parent directory
    };

    // Split "/foo/bar/baz" → ["foo","bar","baz"]
    static std::vector<std::string> splitPath(const char* path) {
        std::vector<std::string> parts;
        std::string cur;
        for (const char* p = path; *p; p++) {
            if (*p == '/') {
                if (!cur.empty()) { parts.push_back(cur); cur.clear(); }
            } else {
                cur += *p;
            }
        }
        if (!cur.empty()) parts.push_back(cur);
        return parts;
    }

    LookupResult lookup(const char* path) const {
        LookupResult res;
        auto parts = splitPath(path);
        if (parts.empty()) {
            // root itself
            res.found = true;
            // Return a synthetic root entry
            memset(res.item.entry.raw, 0, 32);
            res.item.entry.raw[11] = 0x10; // directory attribute
            res.item.name = "/";
            res.item.entry.sector = bpb.rootDirSec;
            res.item.entry.idx    = 0;
            return res;
        }

        std::vector<uint32_t> curDirSecs = rootDirSectors();

        for (size_t pi = 0; pi < parts.size(); pi++) {
            auto items = readDir(curDirSecs);
            bool found = false;
            for (auto& it : items) {
                if (strcasecmp(it.name.c_str(), parts[pi].c_str()) == 0) {
                    if (pi == parts.size() - 1) {
                        res.found         = true;
                        res.item          = it;
                        res.parentSectors = curDirSecs;
                        return res;
                    }
                    if (!it.entry.isDir()) return res; // not a directory
                    curDirSecs = dirSectors(it.entry.cluster());
                    found = true;
                    break;
                }
            }
            if (!found) return res;
        }
        return res;
    }

    // Write a single 32-byte directory entry back to disk
    bool writeDirEntry(const DirEntry& e) {
        uint8_t buf[512] = {};
        if (!disk->readSector(e.sector, buf)) return false;
        memcpy(buf + e.idx * 32, e.raw, 32);
        return disk->writeSector(e.sector, buf);
    }

    // Append a new 32-byte entry in a directory (finds first free slot).
    // Returns sector/idx via 'out'.  Extends dir cluster chain if needed.
    bool appendDirEntry(const std::vector<uint32_t>& dirSecs,
                        const uint8_t entry[32], DirEntry& out) {
        for (uint32_t sec : dirSecs) {
            uint8_t buf[512] = {};
            if (!disk->readSector(sec, buf)) return false;
            int entPerSec = bpb.bytesPerSec / 32;
            for (int i = 0; i < entPerSec; i++) {
                if (buf[i * 32] == 0x00 || buf[i * 32] == 0xE5) {
                    memcpy(buf + i * 32, entry, 32);
                    // Null-terminate the directory if room
                    if (i + 1 < entPerSec && buf[(i+1)*32] != 0x00) {
                        // already entries after; skip nulling
                    } else if (i + 1 < entPerSec) {
                        buf[(i+1)*32] = 0x00;
                    }
                    if (!disk->writeSector(sec, buf)) return false;
                    out.sector = sec;
                    out.idx    = (uint32_t)i;
                    memcpy(out.raw, entry, 32);
                    return true;
                }
            }
        }
        return false; // full — TODO: extend cluster chain for subdirs
    }

    // Build a 8.3 short name (uppercase, padded with spaces)
    static void make83(const char* name, uint8_t out[11]) {
        memset(out, ' ', 11);
        const char* dot = strrchr(name, '.');
        int ni = 0, ei = 0;
        for (const char* p = name; *p && p != dot && ni < 8; p++, ni++)
            out[ni] = (uint8_t)toupper((unsigned char)*p);
        if (dot) {
            for (const char* p = dot + 1; *p && ei < 3; p++, ei++)
                out[8 + ei] = (uint8_t)toupper((unsigned char)*p);
        }
    }

    // Current DOS date/time
    static void nowDosDateTime(uint16_t& date, uint16_t& time) {
        struct tm* t;
        time_t now = ::time(nullptr);
        t = localtime(&now);
        date = (uint16_t)(((t->tm_year - 80) << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday);
        time = (uint16_t)((t->tm_hour << 11) | (t->tm_min << 5) | (t->tm_sec / 2));
    }

    // Read file data
    std::vector<uint8_t> readFile(uint32_t startCluster, uint32_t fileSize) const {
        auto sectors = clusterChain(startCluster);
        std::vector<uint8_t> data;
        data.reserve(fileSize);
        for (uint32_t sec : sectors) {
            if (data.size() >= fileSize) break;
            uint8_t buf[512] = {};
            disk->readSector(sec, buf);
            size_t take = std::min<size_t>(bpb.bytesPerSec, fileSize - data.size());
            data.insert(data.end(), buf, buf + take);
        }
        return data;
    }
};

// ── FUSE operation implementations ───────────────────────────────────────────

static FatFs* getFS() {
    return static_cast<FatFs*>(fuse_get_context()->private_data);
}

static int fat_getattr(const char* path, struct stat* st, struct fuse_file_info*)
{
    memset(st, 0, sizeof(*st));
    FatFs* fs = getFS();

    if (strcmp(path, "/") == 0) {
        st->st_mode  = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }

    auto res = fs->lookup(path);
    if (!res.found) return -ENOENT;

    if (res.item.entry.isDir()) {
        st->st_mode  = S_IFDIR | 0755;
        st->st_nlink = 2;
    } else {
        st->st_mode  = S_IFREG | (fs->disk->isWriteProtected() ? 0444 : 0644);
        st->st_nlink = 1;
        st->st_size  = res.item.entry.fileSize();
    }
    st->st_mtime = res.item.entry.mtime();
    st->st_atime = st->st_mtime;
    st->st_ctime = st->st_mtime;
    return 0;
}

static int fat_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                       off_t, struct fuse_file_info*, enum fuse_readdir_flags)
{
    FatFs* fs = getFS();
    filler(buf, ".",  nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);

    std::vector<uint32_t> secs;
    if (strcmp(path, "/") == 0) {
        secs = fs->rootDirSectors();
    } else {
        auto res = fs->lookup(path);
        if (!res.found || !res.item.entry.isDir()) return -ENOENT;
        secs = fs->dirSectors(res.item.entry.cluster());
    }

    for (auto& item : fs->readDir(secs)) {
        if (item.name == "." || item.name == "..") continue;
        struct stat st = {};
        if (item.entry.isDir()) {
            st.st_mode = S_IFDIR | 0755;
        } else {
            st.st_mode = S_IFREG | 0644;
            st.st_size = item.entry.fileSize();
        }
        filler(buf, item.name.c_str(), &st, 0, FUSE_FILL_DIR_PLUS);
    }
    return 0;
}

static int fat_open(const char* path, struct fuse_file_info* fi)
{
    FatFs* fs = getFS();
    auto res = fs->lookup(path);
    if (!res.found) return -ENOENT;
    if (res.item.entry.isDir()) return -EISDIR;
    if (fs->disk->isWriteProtected() && (fi->flags & O_ACCMODE) != O_RDONLY)
        return -EROFS;
    return 0;
}

static int fat_read(const char* path, char* buf, size_t size, off_t offset,
                    struct fuse_file_info*)
{
    FatFs* fs = getFS();
    auto res = fs->lookup(path);
    if (!res.found) return -ENOENT;
    if (res.item.entry.isDir()) return -EISDIR;

    uint32_t fileSize = res.item.entry.fileSize();
    if ((uint32_t)offset >= fileSize) return 0;

    auto sectors = fs->clusterChain(res.item.entry.cluster());
    size_t read = 0;
    size_t skip = (size_t)offset;
    const size_t bps = fs->bpb.bytesPerSec;

    for (uint32_t sec : sectors) {
        if (read >= size) break;
        if (skip >= bps) { skip -= bps; continue; }

        uint8_t secBuf[512] = {};
        fs->disk->readSector(sec, secBuf);

        size_t from = skip;
        size_t avail = std::min<size_t>(bps - from, fileSize - (offset + (off_t)read));
        size_t take  = std::min(avail, size - read);
        memcpy(buf + read, secBuf + from, take);
        read += take;
        skip = 0;
    }
    return (int)read;
}

static int fat_write(const char* path, const char* buf, size_t size, off_t offset,
                     struct fuse_file_info*)
{
    FatFs* fs = getFS();
    if (fs->disk->isWriteProtected()) return -EROFS;

    auto res = fs->lookup(path);
    if (!res.found) return -ENOENT;
    if (res.item.entry.isDir()) return -EISDIR;

    uint32_t oldSize    = res.item.entry.fileSize();
    uint32_t newSize    = std::max(oldSize, (uint32_t)(offset + size));
    uint32_t startClus  = res.item.entry.cluster();
    const size_t bps    = fs->bpb.bytesPerSec;
    const uint8_t spc   = fs->bpb.secPerClus;

    // Ensure enough clusters allocated
    uint32_t needed = (newSize + (uint32_t)bps * spc - 1) / (bps * spc);
    uint32_t have   = 0;
    uint32_t lastClus = 0;

    if (startClus >= 2) {
        uint32_t cl = startClus;
        while (cl >= 2 && !fs->fatEOC(cl) && !fs->fatFree(cl)) {
            have++;
            lastClus = cl;
            cl = fs->fatGet(cl);
        }
        if (cl < 0xFFF8) { have++; lastClus = cl; }
    }

    if (needed > have) {
        if (startClus < 2) {
            // File has no clusters yet
            startClus = fs->allocCluster(0);
            if (!startClus) return -ENOSPC;
            have++;
            lastClus = startClus;
        }
        while (have < needed) {
            uint32_t nc = fs->allocCluster(lastClus);
            if (!nc) return -ENOSPC;
            lastClus = nc;
            have++;
        }
    }

    // Write data sector-by-sector
    auto sectors = fs->clusterChain(startClus);
    size_t written = 0;

    for (uint32_t i = 0; i < sectors.size() && written < size; i++) {
        off_t secStart = (off_t)i * bps;
        off_t secEnd   = secStart + bps;
        if (secEnd <= offset || secStart >= (off_t)(offset + size)) continue;

        uint8_t secBuf[512] = {};
        fs->disk->readSector(sectors[i], secBuf);

        off_t within = std::max(offset - secStart, (off_t)0);
        size_t srcOff = written + (size_t)(secStart + within - offset);
        if (srcOff > size) break;
        size_t take = std::min<size_t>(bps - within, size - srcOff);
        memcpy(secBuf + within, buf + srcOff, take);
        fs->disk->writeSector(sectors[i], secBuf);
        written += take;
    }

    // Update directory entry (size + cluster)
    FatFs::DirEntry e = res.item.entry;
    e.setSize(newSize);
    if (startClus >= 2) e.setCluster((uint16_t)startClus);
    fs->writeDirEntry(e);

    return (int)written;
}

static int fat_truncate(const char* path, off_t size, struct fuse_file_info*)
{
    FatFs* fs = getFS();
    if (fs->disk->isWriteProtected()) return -EROFS;
    auto res = fs->lookup(path);
    if (!res.found) return -ENOENT;
    if (res.item.entry.isDir()) return -EISDIR;

    uint32_t oldSize  = res.item.entry.fileSize();
    uint32_t startCl  = res.item.entry.cluster();
    uint32_t needed   = std::max(1u, (uint32_t)(((uint32_t)size + fs->bpb.bytesPerSec *
                                  fs->bpb.secPerClus - 1) /
                                  (fs->bpb.bytesPerSec * fs->bpb.secPerClus)));

    // Walk and truncate chain
    if (size == 0 && startCl >= 2) {
        fs->freeChain(startCl);
        startCl = 0;
    } else if (startCl >= 2) {
        uint32_t cl = startCl;
        uint32_t cnt = 1;
        uint32_t eoc = fs->bpb.isFAT16 ? 0xFFFF : 0x0FFF;
        while (!fs->fatEOC(fs->fatGet(cl)) && !fs->fatFree(fs->fatGet(cl))) {
            uint32_t next = fs->fatGet(cl);
            if (cnt >= needed) {
                fs->freeChain(next);
                fs->fatSet(cl, eoc);
                fs->flushFAT();
                break;
            }
            cl = next;
            cnt++;
        }
    }

    (void)oldSize;
    FatFs::DirEntry e = res.item.entry;
    e.setSize((uint32_t)size);
    if (size == 0) e.setCluster(0);
    return fs->writeDirEntry(e) ? 0 : -EIO;
}

static int fat_create(const char* path, mode_t, struct fuse_file_info* fi)
{
    FatFs* fs = getFS();
    if (fs->disk->isWriteProtected()) return -EROFS;

    // Resolve parent directory
    std::string spath(path);
    size_t sep = spath.rfind('/');
    std::string parent = (sep == 0) ? "/" : spath.substr(0, sep);
    std::string fname  = spath.substr(sep + 1);

    // Check parent exists
    std::vector<uint32_t> parentSecs;
    if (parent == "/") {
        parentSecs = fs->rootDirSectors();
    } else {
        auto res = fs->lookup(parent.c_str());
        if (!res.found || !res.item.entry.isDir()) return -ENOENT;
        parentSecs = fs->dirSectors(res.item.entry.cluster());
    }

    // Build 32-byte directory entry
    uint8_t de[32] = {};
    FatFs::make83(fname.c_str(), de);
    de[11] = 0x20; // Archive attribute
    uint16_t date, time;
    FatFs::nowDosDateTime(date, time);
    put_le16(de + 22, time);
    put_le16(de + 24, date);
    // size = 0, cluster = 0

    FatFs::DirEntry out{};
    if (!fs->appendDirEntry(parentSecs, de, out)) return -ENOSPC;
    (void)fi;
    return 0;
}

static int fat_unlink(const char* path)
{
    FatFs* fs = getFS();
    if (fs->disk->isWriteProtected()) return -EROFS;
    auto res = fs->lookup(path);
    if (!res.found) return -ENOENT;
    if (res.item.entry.isDir()) return -EISDIR;

    uint32_t cl = res.item.entry.cluster();
    if (cl >= 2) fs->freeChain(cl);

    FatFs::DirEntry e = res.item.entry;
    e.markDeleted();
    return fs->writeDirEntry(e) ? 0 : -EIO;
}

static int fat_mkdir(const char* path, mode_t)
{
    FatFs* fs = getFS();
    if (fs->disk->isWriteProtected()) return -EROFS;

    std::string spath(path);
    size_t sep = spath.rfind('/');
    std::string parent = (sep == 0) ? "/" : spath.substr(0, sep);
    std::string dname  = spath.substr(sep + 1);

    std::vector<uint32_t> parentSecs;
    if (parent == "/") {
        parentSecs = fs->rootDirSectors();
    } else {
        auto res = fs->lookup(parent.c_str());
        if (!res.found || !res.item.entry.isDir()) return -ENOENT;
        parentSecs = fs->dirSectors(res.item.entry.cluster());
    }

    // Allocate a cluster for the new directory
    uint32_t cl = fs->allocCluster(0);
    if (!cl) return -ENOSPC;

    uint16_t date, time;
    FatFs::nowDosDateTime(date, time);

    // Write "." and ".." entries into the new cluster
    uint32_t base = fs->clusterToSector(cl);
    uint8_t buf[512] = {};

    // "." entry
    memset(buf, ' ', 11); buf[0]='.'; buf[11]=0x10;
    put_le16(buf+22,time); put_le16(buf+24,date); put_le16(buf+26,(uint16_t)cl);
    // ".." entry
    memset(buf+32, ' ', 11); buf[32]='.'; buf[33]='.'; buf[43]=0x10;
    put_le16(buf+54,time); put_le16(buf+56,date);
    buf[64] = 0; // next entry = end-of-directory
    fs->disk->writeSector(base, buf);

    // Add directory entry in parent
    uint8_t de[32] = {};
    FatFs::make83(dname.c_str(), de);
    de[11] = 0x10; // directory
    put_le16(de+22,time); put_le16(de+24,date); put_le16(de+26,(uint16_t)cl);

    FatFs::DirEntry out{};
    if (!fs->appendDirEntry(parentSecs, de, out)) return -ENOSPC;
    return 0;
}

static int fat_rmdir(const char* path)
{
    FatFs* fs = getFS();
    if (fs->disk->isWriteProtected()) return -EROFS;
    auto res = fs->lookup(path);
    if (!res.found || !res.item.entry.isDir()) return -ENOTDIR;

    // Check if empty
    auto secs  = fs->dirSectors(res.item.entry.cluster());
    auto items = fs->readDir(secs);
    bool empty = true;
    for (auto& it : items)
        if (it.name != "." && it.name != "..") { empty = false; break; }
    if (!empty) return -ENOTEMPTY;

    uint32_t cl = res.item.entry.cluster();
    if (cl >= 2) fs->freeChain(cl);

    FatFs::DirEntry e = res.item.entry;
    e.markDeleted();
    return fs->writeDirEntry(e) ? 0 : -EIO;
}

static int fat_rename(const char* from, const char* to, unsigned int flags)
{
    if (flags) return -EINVAL; // RENAME_EXCHANGE / RENAME_NOREPLACE not supported
    FatFs* fs = getFS();
    if (fs->disk->isWriteProtected()) return -EROFS;

    auto srcRes = fs->lookup(from);
    if (!srcRes.found) return -ENOENT;

    // Remove destination if exists
    auto dstRes = fs->lookup(to);
    if (dstRes.found) {
        if (dstRes.item.entry.isDir()) {
            fat_rmdir(to);
        } else {
            fat_unlink(to);
        }
    }

    // Determine new name
    std::string stopath(to);
    size_t sep  = stopath.rfind('/');
    std::string newName = stopath.substr(sep + 1);

    // Update the short name in the existing entry
    FatFs::DirEntry e = srcRes.item.entry;
    FatFs::make83(newName.c_str(), e.raw);
    return fs->writeDirEntry(e) ? 0 : -EIO;
}

static int fat_chmod(const char*, mode_t, struct fuse_file_info*) { return 0; }
static int fat_chown(const char*, uid_t, gid_t, struct fuse_file_info*) { return 0; }

static int fat_fsync(const char*, int, struct fuse_file_info*)
{
    getFS()->disk->flush();
    return 0;
}

static int fat_statfs(const char*, struct statvfs* sv)
{
    FatFs* fs = getFS();
    uint32_t free_clus = 0;
    uint32_t total = fs->bpb.dataClusterCount + 2;
    for (uint32_t c = 2; c < total; c++)
        if (fs->fatFree(fs->fatGet(c))) free_clus++;
    uint32_t bpc = (uint32_t)fs->bpb.secPerClus * 512u;
    memset(sv, 0, sizeof(*sv));
    sv->f_bsize   = bpc;
    sv->f_frsize  = bpc;
    sv->f_blocks  = fs->bpb.dataClusterCount;
    sv->f_bfree   = free_clus;
    sv->f_bavail  = free_clus;
    sv->f_namemax = 255;
    return 0;
}

static void* fat_init(struct fuse_conn_info*, struct fuse_config* cfg)
{
    cfg->kernel_cache = 0;
    cfg->direct_io    = 1;
    return fuse_get_context()->private_data;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool fat_detect(IDiskImage* disk)
{
    uint8_t sec0[512] = {};
    if (!disk->readSector(0, sec0)) return false;
    if (sec0[510] != 0x55 || sec0[511] != 0xAA) return false;
    return (sec0[0] == 0xEB || sec0[0] == 0xE9);
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
    if (!fs->init(disk)) { delete fs; return nullptr; }
    return fs;
}

void fat_fs_free(void* ctx)
{
    delete static_cast<FatFs*>(ctx);
}

bool fat_is_fat16(void* ctx)
{
    return static_cast<FatFs*>(ctx)->bpb.isFAT16;
}
