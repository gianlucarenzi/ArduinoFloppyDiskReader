// waffle-fuse: disk cache / hardware abstraction layer implementation

#include "disk_cache.h"
#include "waffle_log.h"
#include "ADFWriter.h"
#include "ArduinoInterface.h"
#include "ibm_sectors.h"
#include "debugmsg.h"

#include <cstring>
#include <stdexcept>
#include <locale>
#include <codecvt>
#include <iostream>

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::wstring toWide(const std::string& s)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cv;
    return cv.from_bytes(s);
}

// ── WaffleDisk::Impl (owns ADFWriter) ────────────────────────────────────────

struct WaffleDisk::Impl {
    ArduinoFloppyReader::ADFWriter writer;
};

// ── WaffleDisk ────────────────────────────────────────────────────────────────

WaffleDisk::WaffleDisk(const std::string& portName, bool forceReadOnly, int forcedHD)
    : m_portName(portName), m_forceReadOnly(forceReadOnly), m_forcedHD(forcedHD)
{}

WaffleDisk::~WaffleDisk()
{
    close();
}

bool WaffleDisk::open()
{
    if (m_open) return true;

    VLOG("waffle: opening device " << m_portName);
    m_impl = new Impl();
    if (!m_impl->writer.openDevice(toWide(m_portName))) {
        delete m_impl;
        m_impl = nullptr;
        return false;
    }

    m_writeProtected =
        (m_impl->writer.checkIfDiskIsWriteProtected(true) ==
         ArduinoFloppyReader::DiagnosticResponse::drWriteProtected);

    if (!detectGeometry()) {
        m_impl->writer.closeDevice();
        delete m_impl;
        m_impl = nullptr;
        return false;
    }

    m_open = true;
    m_writerOpen = true;
    m_diskPresent = true;
    return true;
}

// Open port only — geometry detection is deferred to remount() (NBD path).
bool WaffleDisk::openDisk()
{
    if (m_open) return true;

    VLOG("waffle: openDisk " << m_portName);
    m_impl = new Impl();
    if (!m_impl->writer.openDevice(toWide(m_portName))) {
        delete m_impl;
        m_impl = nullptr;
        return false;
    }

    m_open = true;
    m_writerOpen = true;
    m_diskPresent = true; // probe already confirmed presence
    return true;
}

void WaffleDisk::close()
{
    if (!m_open) return;
    flush();
    if (m_writerOpen) {
        m_impl->writer.resetDevice();
        m_impl->writer.closeDevice();
        m_writerOpen = false;
    }
    delete m_impl;
    m_impl = nullptr;
    m_cache.clear();
    m_open = false;
    m_diskPresent = false;
}

void WaffleDisk::closeDisk()
{
    close();
}

// Disable motor but keep port open — used before checkPresence() polling
// in Phase 5 so we don't move the head just to check disk presence.
void WaffleDisk::parkDisk()
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_open || !m_writerOpen) return;
    VLOG("waffle: parkDisk (motor off)");
    m_impl->writer.enableReading(false);
}

// Re-detect geometry and write-protection on an already-open port.
bool WaffleDisk::remount()
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_open || !m_writerOpen) return false;

    m_cache.clear();
    m_writeProtected =
        (m_impl->writer.checkIfDiskIsWriteProtected(true) ==
         ArduinoFloppyReader::DiagnosticResponse::drWriteProtected);

    if (!detectGeometry()) {
        diskRemoved();
        return false;
    }

    m_diskPresent = true;

    {
        const char* fmt = (m_geo.format == DiskFormat::Amiga_ADF) ? "affs" :
                          (m_geo.format == DiskFormat::IBM_FAT)   ? "vfat" : "unknown";
        const char* den = m_geo.isHD ? "hd" : "dd";
        std::cerr << "waffle-event: format=" << fmt << " density=" << den << std::endl;
    }

    std::cerr << "waffle: format="
              << (m_geo.format == DiskFormat::Amiga_ADF ? "Amiga" : "PC/FAT")
              << " size=" << (totalSectors() * 512 / 1024) << " KB"
              << (m_writeProtected ? " [RO]" : "") << "\n";
    return true;
}

// Quick probe: open a fresh connection, check presence, close.
// Port must be closed (m_writerOpen == false) when called.
bool WaffleDisk::probePresent() const
{
    return WaffleDisk::probe(m_portName) == 0;
}

// Check disk presence using the already-open port (safe during I/O pauses).
bool WaffleDisk::checkPresence()
{
    using namespace ArduinoFloppyReader;
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_open || !m_writerOpen) return false;
    m_impl->writer.checkIfDiskIsWriteProtected(true);
    bool present = m_impl->writer.isDiskInDrive();
    if (!present) diskRemoved();
    return present;
}

// ── Probe (lightweight disk presence check, no head movement) ─────────────────

int WaffleDisk::probe(const std::string& portName)
{
    using namespace ArduinoFloppyReader;
    ADFWriter writer;
    if (!writer.openDevice(toWide(portName)))
        return 2;  // port error / hardware not found
    // checkIfDiskIsWriteProtected(true) internally calls checkForDisk(true),
    // which sends the COMMAND_CHECKDISKEXISTS ('^') byte and updates isDiskInDrive().
    writer.checkIfDiskIsWriteProtected(true);
    bool present = writer.isDiskInDrive();
    writer.closeDevice();
    return present ? 0 : 1;
}

// ── Geometry detection ────────────────────────────────────────────────────────

bool WaffleDisk::detectGeometry()
{
    bool isHD = false;
    if (m_forcedHD >= 0) {
        isHD = (m_forcedHD == 1);  // forced by user
    } else {
        m_impl->writer.GuessDiskDensity(isHD);
    }
    m_geo.isHD = isHD;

    // Position to track 0, head 0 (lower = side 0 in Amiga convention).
    // enableReading(reset=true) internally calls findTrack0 which switches to
    // dsUpper, so we must select dsLower AFTER enableReading, not before.
    using namespace ArduinoFloppyReader;
    if (m_impl->writer.enableReading(true, true) != DiagnosticResponse::drOK) return false;
    if (m_impl->writer.selectTrack(0)            != DiagnosticResponse::drOK) return false;
    if (m_impl->writer.selectSurface(DiskSurface::dsLower) != DiagnosticResponse::drOK) return false;

    // ── Try Amiga ADF first ───────────────────────────────────────────────────
    // Strategy: try DD first (most Amiga disks are DD), then HD.  The retry
    // trigger is whether sector 0 contains the 'DOS' bootblock signature —
    // not the sector count — because sector 0 might fail to decode even when
    // other sectors succeed, giving a misleading count.
    // If GuessDiskDensity correctly guessed HD, we start there instead.
    {
        auto hasDos = [](const std::vector<std::array<uint8_t,512>>& s) -> bool {
            return !s.empty() && s[0][0]=='D' && s[0][1]=='O' && s[0][2]=='S';
        };

        // First attempt: use guessed density
        std::vector<std::array<uint8_t, 512>> amigaSectors;
        m_impl->writer.readAmigaTrack(isHD, 0, DiskSurface::dsLower, amigaSectors, 3);
        std::cerr << "waffle-detect: Amiga try1 isHD=" << isHD
                  << " valid=" << [&]{ unsigned n=0; for(auto& s:amigaSectors) for(uint8_t b:s) if(b){++n;break;} return n; }()
                  << " bb=" << std::hex
                  << (int)(amigaSectors.empty()?0:amigaSectors[0][0]) << ","
                  << (int)(amigaSectors.empty()?0:amigaSectors[0][1]) << ","
                  << (int)(amigaSectors.empty()?0:amigaSectors[0][2]) << ","
                  << (int)(amigaSectors.empty()?0:amigaSectors[0][3])
                  << std::dec << "\n";

        if (m_forcedHD < 0 && !hasDos(amigaSectors)) {
            // Retry with opposite density
            bool altHD = !isHD;
            m_impl->writer.selectTrack(0);
            m_impl->writer.selectSurface(DiskSurface::dsLower);
            std::vector<std::array<uint8_t, 512>> altSectors;
            m_impl->writer.readAmigaTrack(altHD, 0, DiskSurface::dsLower, altSectors, 3);
            std::cerr << "waffle-detect: Amiga try2 isHD=" << altHD
                      << " valid=" << [&]{ unsigned n=0; for(auto& s:altSectors) for(uint8_t b:s) if(b){++n;break;} return n; }()
                      << " bb=" << std::hex
                      << (int)(altSectors.empty()?0:altSectors[0][0]) << ","
                      << (int)(altSectors.empty()?0:altSectors[0][1]) << ","
                      << (int)(altSectors.empty()?0:altSectors[0][2]) << ","
                      << (int)(altSectors.empty()?0:altSectors[0][3])
                      << std::dec << "\n";
            if (hasDos(altSectors)) {
                isHD = altHD;
                m_geo.isHD = isHD;
                amigaSectors = std::move(altSectors);
            }
        }

        const unsigned int maxSect = isHD ? 22u : 11u;
        if (hasDos(amigaSectors)) {
            std::cerr << "waffle-detect: Amiga confirmed isHD=" << isHD << "\n";
            m_geo.format          = DiskFormat::Amiga_ADF;
            m_geo.numHeads        = 2;
            m_geo.sectorsPerTrack = maxSect;
            m_geo.numCylinders    = 80;
            m_geo.bytesPerSector  = 512;
            return true;
        }
    }

    // ── Try IBM / FAT ─────────────────────────────────────────────────────────
    // Only reached if Amiga decoding found no valid bootblock.
    // Try both DD (9 sectors) and HD (18 sectors) so that GuessDiskDensity
    // being wrong cannot cause a misidentification.
    // Require BOTH the FAT signature (0x55AA) AND a valid jump instruction to
    // avoid false positives from garbage MFM data.
    {
        // Order: try the density GuessDiskDensity suggested first.
        const bool densities[2] = { isHD, !isHD };

        for (bool tryHD : densities) {
            m_impl->writer.selectTrack(0);
            m_impl->writer.selectSurface(DiskSurface::dsLower);

            const uint32_t trySectors = tryHD ? 18u : 9u;
            std::vector<std::vector<uint8_t>> ibmSectors;
            bool ok = m_impl->writer.readIBMTrack(tryHD, 0, trySectors, ibmSectors, 3);

            if (!ok || ibmSectors.empty() || ibmSectors[0].empty()) continue;

            const uint8_t* boot = ibmSectors[0].data();
            bool fatSig = (ibmSectors[0].size() >= 512) &&
                          (boot[510] == 0x55) && (boot[511] == 0xAA);
            bool jmpOk  = (boot[0] == 0xEB || boot[0] == 0xE9);

            if (!fatSig || !jmpOk) continue;

            std::cerr << "waffle-detect: IBM/FAT confirmed isHD=" << tryHD << "\n";
            m_geo.isHD           = tryHD;
            m_geo.format         = DiskFormat::IBM_FAT;
            m_geo.bytesPerSector = 512;

            uint32_t serialNum, numHeads, totalSectors, sectPerTrack, bytesPerSec;
            if (IBM::getTrackDetails_IBM(boot, serialNum, numHeads, totalSectors,
                                         sectPerTrack, bytesPerSec)) {
                m_geo.numHeads        = numHeads;
                m_geo.sectorsPerTrack = sectPerTrack;
                m_geo.bytesPerSector  = bytesPerSec;
                m_geo.numCylinders    = totalSectors / (numHeads * sectPerTrack);
            } else {
                m_geo.numHeads        = 2;
                m_geo.sectorsPerTrack = trySectors;
                m_geo.numCylinders    = 80;
            }
            return true;
        }
    }

    // Unknown — still return true; the FUSE driver will present a raw image
    m_geo.format          = DiskFormat::Unknown;
    m_geo.numHeads        = 2;
    m_geo.sectorsPerTrack = isHD ? 18 : 9;
    m_geo.numCylinders    = 80;
    m_geo.bytesPerSector  = 512;
    return true;
}

// ── LBA ↔ physical ────────────────────────────────────────────────────────────

void WaffleDisk::lbaToPhysical(uint32_t lba,
                                uint32_t& cylinder, uint32_t& head,
                                uint32_t& sector) const
{
    const uint32_t spt = m_geo.sectorsPerTrack;
    const uint32_t nh  = m_geo.numHeads;
    cylinder = lba / (nh * spt);
    head     = (lba / spt) % nh;
    sector   = lba % spt;
}

uint32_t WaffleDisk::totalSectors() const
{
    return m_geo.numCylinders * m_geo.numHeads * m_geo.sectorsPerTrack;
}

// ── Track cache ───────────────────────────────────────────────────────────────

bool WaffleDisk::loadTrack(uint32_t cylinder, uint32_t head)
{
    const uint32_t key = cylinder * 2 + head;
    if (m_cache.count(key)) return true; // already cached

    if (!m_writerOpen) return false;

    VLOG("waffle: loadTrack cyl=" << cylinder << " head=" << head);

    using namespace ArduinoFloppyReader;
    const DiskSurface side = (head == 1) ? DiskSurface::dsUpper : DiskSurface::dsLower;

    if (m_impl->writer.selectTrack((unsigned char)cylinder) != DiagnosticResponse::drOK) {
        diskRemoved();
        return false;
    }
    if (m_impl->writer.selectSurface(side) != DiagnosticResponse::drOK) {
        diskRemoved();
        return false;
    }

    TrackCache tc;
    bool trackOk = true;

    if (m_geo.format == DiskFormat::Amiga_ADF) {
        std::vector<std::array<uint8_t, 512>> sects;
        m_impl->writer.readAmigaTrack(m_geo.isHD, cylinder, side, sects);
        if (sects.empty()) {
            trackOk = false;
        } else {
            tc.sectors.resize(sects.size());
            for (size_t i = 0; i < sects.size(); i++)
                tc.sectors[i].assign(sects[i].begin(), sects[i].end());
        }
    } else {
        // IBM / FAT / Unknown
        const uint32_t trackNum = cylinder * m_geo.numHeads + head;
        trackOk = m_impl->writer.readIBMTrack(m_geo.isHD, trackNum,
                                              m_geo.sectorsPerTrack, tc.sectors);
    }

    if (!trackOk) {
        diskRemoved();
        return false;
    }

    tc.dirty = false;
    m_cache[key] = std::move(tc);
    return true;
}

// Called under m_mtx when an I/O failure suggests the disk was removed.
void WaffleDisk::diskRemoved()
{
    if (!m_diskPresent.exchange(false)) return; // already known absent
    m_cache.clear();
    if (m_writerOpen) {
        m_impl->writer.closeDevice();
        m_writerOpen = false;
    }
    std::cerr << "waffle: disk removed (I/O error)\n";
}

bool WaffleDisk::flushTrack(uint32_t cylinder, uint32_t head, TrackCache& tc)
{
    if (!tc.dirty) return true;
    if (isWriteProtected()) return false;
    if (!m_writerOpen) return false;

    using namespace ArduinoFloppyReader;
    const DiskSurface side = (head == 1) ? DiskSurface::dsUpper : DiskSurface::dsLower;

    if (m_impl->writer.selectTrack((unsigned char)cylinder) != DiagnosticResponse::drOK)
        return false;
    if (m_impl->writer.selectSurface(side) != DiagnosticResponse::drOK)
        return false;

    bool ok;
    if (m_geo.format == DiskFormat::Amiga_ADF) {
        std::vector<std::array<uint8_t, 512>> sects(tc.sectors.size());
        for (size_t i = 0; i < tc.sectors.size(); i++)
            memcpy(sects[i].data(), tc.sectors[i].data(),
                   std::min<size_t>(512, tc.sectors[i].size()));
        ok = m_impl->writer.writeAmigaTrack(m_geo.isHD, cylinder, side, sects,
                                            cylinder >= 40);
    } else {
        const uint32_t trackNum = cylinder * m_geo.numHeads + head;
        ok = m_impl->writer.writeIBMTrack(m_geo.isHD, trackNum,
                                          m_geo.nonStdTiming, tc.sectors,
                                          cylinder >= 40);
    }

    if (ok) tc.dirty = false;
    return ok;
}

bool WaffleDisk::flush()
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_open || !m_writerOpen) return true;
    bool ok = true;
    for (auto& kv : m_cache) {
        if (kv.second.dirty) {
            const uint32_t cylinder = kv.first / 2;
            const uint32_t head     = kv.first % 2;
            ok &= flushTrack(cylinder, head, kv.second);
        }
    }
    return ok;
}

// ── readSector / writeSector ──────────────────────────────────────────────────

bool WaffleDisk::readSector(uint32_t lba, uint8_t* buf512)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_open || !m_diskPresent) return false;

    VLOG("waffle: readSector lba=" << lba);

    uint32_t cylinder, head, sector;
    lbaToPhysical(lba, cylinder, head, sector);

    if (!loadTrack(cylinder, head)) return false;

    const uint32_t key = cylinder * 2 + head;
    const TrackCache& tc = m_cache.at(key);

    if (sector >= tc.sectors.size() || tc.sectors[sector].empty()) {
        memset(buf512, 0, 512);
        return false;
    }
    const auto& s = tc.sectors[sector];
    memcpy(buf512, s.data(), std::min<size_t>(512, s.size()));
    if (s.size() < 512)
        memset(buf512 + s.size(), 0, 512 - s.size());
    return true;
}

bool WaffleDisk::writeSector(uint32_t lba, const uint8_t* buf512)
{    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_open || !m_diskPresent || isWriteProtected()) return false;

    VLOG("waffle: writeSector lba=" << lba);

    uint32_t cylinder, head, sector;
    lbaToPhysical(lba, cylinder, head, sector);

    if (!loadTrack(cylinder, head)) return false;

    const uint32_t key = cylinder * 2 + head;
    TrackCache& tc = m_cache[key];

    if (sector >= tc.sectors.size())
        tc.sectors.resize(sector + 1);
    tc.sectors[sector].assign(buf512, buf512 + 512);
    tc.dirty = true;

    // Write-through: flush the track immediately
    return flushTrack(cylinder, head, tc);
}

// ── Format helpers ────────────────────────────────────────────────────────────

static void put32be(uint8_t* p, uint32_t v)
{
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >>  8) & 0xFF;
    p[3] =  v        & 0xFF;
}

static uint32_t get32be(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

// Amiga bootblock checksum: one's complement sum with end-around carry.
// Called with the checksum field zeroed; store the returned value at offset 4.
static uint32_t amigaBootChecksum(const uint8_t* bb1024)
{
    uint32_t sum = 0;
    for (int i = 0; i < 256; i++) {
        uint32_t w = get32be(bb1024 + i * 4);
        uint32_t prev = sum;
        sum += w;
        if (sum < prev) sum++; // end-around carry
    }
    return ~sum;
}

// Standard Amiga filesystem block checksum: -sum of all 32-bit words (checksum word = 0).
// csWordIdx is the index (0-based in 32-bit words) of the checksum field.
static uint32_t amigaBlockChecksum(const uint8_t* block512, int csWordIdx)
{
    uint32_t sum = 0;
    for (int i = 0; i < 128; i++) {
        if (i == csWordIdx) continue;
        sum += get32be(block512 + i * 4);
    }
    return static_cast<uint32_t>(-static_cast<int32_t>(sum));
}

// Build a blank Amiga OFS disk image as a flat array of 512-byte sectors.
// Returns total sector count (1760 for DD, 3520 for HD).
static std::vector<std::array<uint8_t, 512>> buildAmigaDiskImage(bool isHD)
{
    const uint32_t spt        = isHD ? 22u : 11u;
    const uint32_t total      = 80u * 2u * spt;        // 1760 / 3520
    const uint32_t rootBlock  = total / 2;              // 880  / 1760
    const uint32_t bitmapBlock = rootBlock + 1;         // 881  / 1761

    std::vector<std::array<uint8_t, 512>> img(total);
    for (auto& s : img) s.fill(0);

    // ── Bootblock (LBA 0-1, 1024 bytes total) ────────────────────────────────
    uint8_t bb[1024] = {};
    bb[0] = 'D'; bb[1] = 'O'; bb[2] = 'S'; bb[3] = 0x00; // OFS
    put32be(bb + 4,  0);          // checksum placeholder
    put32be(bb + 8,  rootBlock);  // root block pointer
    put32be(bb + 4, amigaBootChecksum(bb));
    memcpy(img[0].data(), bb,       512);
    memcpy(img[1].data(), bb + 512, 512);

    // ── Root block ────────────────────────────────────────────────────────────
    {
        uint8_t* rb = img[rootBlock].data();
        put32be(rb +   0, 2);           // T_HEADER
        put32be(rb +   4, rootBlock);   // header_key (own block number)
        put32be(rb +   8, 0);           // high_seq
        put32be(rb +  12, 72);          // ht_size = 0x48
        put32be(rb +  16, 0);           // first_data
        put32be(rb +  20, 0);           // checksum placeholder
        // rb[24..311]: hash table (72 entries × 4 bytes) – already zero
        put32be(rb + 312, 0xFFFFFFFF);  // bm_flag = -1 (bitmap valid)
        put32be(rb + 316, bitmapBlock); // bm_pages[0]
        // rb[320..415]: bm_pages[1..24] – already zero (no secondary bitmap)
        put32be(rb + 416, 0);           // bm_ext
        // r_days / r_mins / r_ticks at 420/424/428 – zero (epoch)
        const char* label = "Empty";
        rb[432] = (uint8_t)strlen(label);
        memcpy(rb + 433, label, strlen(label));
        // v_days/v_mins/v_ticks and c_days/c_mins/c_ticks – zero
        put32be(rb + 508, 1);           // sec_type = ST_ROOT
        put32be(rb +  20, amigaBlockChecksum(rb, 5)); // word 5 = offset 20
    }

    // ── Bitmap block ──────────────────────────────────────────────────────────
    {
        uint8_t* bm = img[bitmapBlock].data();
        // [0..3]: checksum (set later)
        // [4..507]: 504 bytes = 4032 bits; 1 = free, 0 = used
        memset(bm + 4, 0xFF, 504);

        // Mark root and bitmap blocks as used (bit = 0).
        // Bit index for block N = N - 2; stored in 32-bit BE words, LSB = lower block.
        auto clearBit = [&](uint32_t blockNum) {
            uint32_t idx     = blockNum - 2;
            uint32_t wordIdx = idx / 32;
            uint32_t bitIdx  = idx % 32;
            uint8_t* wp = bm + 4 + wordIdx * 4;
            put32be(wp, get32be(wp) & ~(1u << bitIdx));
        };
        clearBit(rootBlock);
        clearBit(bitmapBlock);

        // Zero out bits beyond the last allocatable block.
        const uint32_t validBits = total - 2; // blocks 2..total-1
        for (uint32_t b = validBits; b < 4032; b++) {
            uint8_t* wp = bm + 4 + (b / 32) * 4;
            put32be(wp, get32be(wp) & ~(1u << (b % 32)));
        }

        // Bitmap block checksum: -sum of words 1..127 (word 0 is checksum).
        uint32_t sum = 0;
        for (int i = 1; i < 128; i++) sum += get32be(bm + i * 4);
        put32be(bm, static_cast<uint32_t>(-static_cast<int32_t>(sum)));
    }

    return img;
}

// Build a blank PC FAT12 disk image.
static std::vector<std::array<uint8_t, 512>> buildFAT12DiskImage(bool isHD)
{
    const uint32_t spt         = isHD ? 18u : 9u;
    const uint32_t numHeads    = 2u;
    const uint32_t numCyl      = 80u;
    const uint32_t total       = numCyl * numHeads * spt;  // 1440 / 2880
    const uint32_t sectPerClus = isHD ? 1u : 2u;
    const uint32_t reserved    = 1u;
    const uint32_t numFATs     = 2u;
    const uint32_t fatSize     = isHD ? 9u : 3u;
    const uint32_t rootEntries = isHD ? 224u : 112u;
    const uint8_t  mediaType   = isHD ? 0xF0 : 0xF9;

    std::vector<std::array<uint8_t, 512>> img(total);
    for (auto& s : img) s.fill(0);

    // ── Boot sector ───────────────────────────────────────────────────────────
    {
        uint8_t* b = img[0].data();
        b[0] = 0xEB; b[1] = 0x3C; b[2] = 0x90; // JMP + NOP
        memcpy(b + 3, "MSDOS5.0", 8);           // OEM name

        // BPB (little-endian)
        b[11] = 0x00; b[12] = 0x02;                         // bytes per sector = 512
        b[13] = (uint8_t)sectPerClus;                       // sectors per cluster
        b[14] = (uint8_t)reserved; b[15] = 0;               // reserved sectors
        b[16] = (uint8_t)numFATs;                           // number of FATs
        b[17] = (uint8_t)rootEntries; b[18] = (uint8_t)(rootEntries >> 8);
        b[19] = (uint8_t)total;       b[20] = (uint8_t)(total >> 8); // total sectors
        b[21] = mediaType;
        b[22] = (uint8_t)fatSize; b[23] = 0;               // sectors per FAT
        b[24] = (uint8_t)spt;     b[25] = 0;               // sectors per track
        b[26] = (uint8_t)numHeads; b[27] = 0;              // number of heads
        b[28] = b[29] = b[30] = b[31] = 0;                 // hidden sectors
        b[32] = b[33] = b[34] = b[35] = 0;                 // large sector count

        // Extended BPB
        b[36] = 0x00;                               // drive number
        b[37] = 0x00;                               // reserved
        b[38] = 0x29;                               // extended boot signature
        b[39] = 0xDE; b[40] = 0xAD; b[41] = 0xBE; b[42] = 0xEF; // volume serial
        memcpy(b + 43, "NO NAME    ", 11);          // volume label (11 chars)
        memcpy(b + 54, "FAT12   ",    8);           // filesystem type (8 chars)

        b[510] = 0x55; b[511] = 0xAA;              // boot signature
    }

    // ── FAT tables (FAT1 at sector 'reserved', FAT2 at 'reserved + fatSize') ─
    for (uint32_t f = 0; f < numFATs; f++) {
        uint8_t* fat = img[reserved + f * fatSize].data();
        // First 3 bytes encode media descriptor in entry[0] and 0xFFF in entry[1].
        fat[0] = mediaType;
        fat[1] = 0xFF;
        fat[2] = 0xFF;
    }

    return img;
}

// ── WaffleDisk::formatDisk ────────────────────────────────────────────────────

bool WaffleDisk::formatDisk(FormatType type, bool isHD,
    std::function<void(int, int, const std::string&)> progress)
{
    auto img = buildDiskImage(type, isHD);
    return writeDiskImage(type, isHD, img, progress);
}

bool WaffleDisk::writeDiskImage(FormatType type, bool isHD,
    const std::vector<std::array<uint8_t, 512>>& img,
    std::function<void(int, int, const std::string&)> progress)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_open || !m_writerOpen) return false;

    // Honour hardware write-protection.
    bool wp = (m_impl->writer.checkIfDiskIsWriteProtected(true) ==
               ArduinoFloppyReader::DiagnosticResponse::drWriteProtected);
    if (wp) {
        std::cerr << "waffle: disk is write-protected, cannot format\n";
        return false;
    }

    const uint32_t spt = isHD
        ? (type == FormatType::Amiga_OFS ? 22u : 18u)
        : (type == FormatType::Amiga_OFS ? 11u :  9u);
    const uint32_t numCyl       = 80u;
    const uint32_t numHeads     = 2u;
    const uint32_t totalTracks  = numCyl * numHeads;

    // Enable the drive motor and seek to track 0.
    using namespace ArduinoFloppyReader;
    if (m_impl->writer.enableReading(true, true) != DiagnosticResponse::drOK)
        return false;

    int tracksDone = 0;
    for (uint32_t cyl = 0; cyl < numCyl; cyl++) {
        for (uint32_t head = 0; head < numHeads; head++) {
            const DiskSurface side =
                (head == 1) ? DiskSurface::dsUpper : DiskSurface::dsLower;
            const uint32_t trackNum = cyl * numHeads + head;
            const bool usePrecomp   = (cyl >= 40);

            if (m_impl->writer.selectTrack((unsigned char)cyl) != DiagnosticResponse::drOK)
                return false;
            if (m_impl->writer.selectSurface(side) != DiagnosticResponse::drOK)
                return false;

            if (progress) {
                progress(tracksDone, (int)totalTracks,
                    "cyl " + std::to_string(cyl) + " head " + std::to_string(head));
            }

            bool ok;
            if (type == FormatType::Amiga_OFS) {
                std::vector<std::array<uint8_t, 512>> sects(spt);
                for (uint32_t s = 0; s < spt; s++) {
                    uint32_t lba = trackNum * spt + s;
                    sects[s] = (lba < img.size()) ? img[lba]
                                                  : std::array<uint8_t,512>{};
                }
                ok = m_impl->writer.writeAmigaTrack(isHD, cyl, side, sects, usePrecomp);
            } else {
                std::vector<std::vector<uint8_t>> sects(spt);
                for (uint32_t s = 0; s < spt; s++) {
                    uint32_t lba = trackNum * spt + s;
                    const auto& sec = (lba < img.size()) ? img[lba]
                                                         : std::array<uint8_t,512>{};
                    sects[s].assign(sec.begin(), sec.end());
                }
                ok = m_impl->writer.writeIBMTrack(isHD, trackNum, false, sects, usePrecomp);
            }

            if (!ok) {
                std::cerr << "waffle: write error at cyl=" << cyl
                          << " head=" << head << "\n";
                return false;
            }
            tracksDone++;
        }
    }

    if (progress)
        progress((int)totalTracks, (int)totalTracks, "done");

    // Update cached geometry to reflect the newly written disk.
    m_cache.clear();
    m_geo.format          = (type == FormatType::Amiga_OFS) ? DiskFormat::Amiga_ADF
                                                             : DiskFormat::IBM_FAT;
    m_geo.isHD            = isHD;
    m_geo.numCylinders    = numCyl;
    m_geo.numHeads        = numHeads;
    m_geo.sectorsPerTrack = spt;
    m_geo.bytesPerSector  = 512;
    m_geo.nonStdTiming    = false;

    return true;
}

// ── Public image builder ───────────────────────────────────────────────────────

std::vector<std::array<uint8_t, 512>> buildDiskImage(FormatType type, bool isHD)
{
    return (type == FormatType::Amiga_OFS) ? buildAmigaDiskImage(isHD)
                                           : buildFAT12DiskImage(isHD);
}
