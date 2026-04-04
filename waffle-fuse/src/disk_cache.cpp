// waffle-fuse: disk cache / hardware abstraction layer implementation

#include "disk_cache.h"
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

    m_impl = new Impl();
    if (!m_impl->writer.openDevice(toWide(m_portName))) {
        delete m_impl;
        m_impl = nullptr;
        return false;
    }

    // Check write protection
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
    return true;
}

void WaffleDisk::close()
{
    if (!m_open) return;
    flush();
    m_impl->writer.closeDevice();
    delete m_impl;
    m_impl = nullptr;
    m_cache.clear();
    m_open = false;
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
    // Require BOTH the FAT signature (0x55AA) AND a valid jump instruction to
    // avoid false positives from garbage MFM data.
    {
        m_impl->writer.selectTrack(0);
        m_impl->writer.selectSurface(DiskSurface::dsLower);

        uint32_t trySectors = isHD ? 18 : 9;
        std::vector<std::vector<uint8_t>> ibmSectors;
        bool ok = m_impl->writer.readIBMTrack(isHD, 0, trySectors, ibmSectors, 3);

        if (ok && !ibmSectors.empty() && !ibmSectors[0].empty()) {
            const uint8_t* boot = ibmSectors[0].data();

            bool fatSig = (ibmSectors[0].size() >= 512) &&
                          (boot[510] == 0x55) && (boot[511] == 0xAA);
            bool jmpOk  = (boot[0] == 0xEB || boot[0] == 0xE9);

            // Require both markers to avoid false positives on Amiga data.
            if (fatSig && jmpOk) {
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
                    m_geo.sectorsPerTrack = isHD ? 18 : 9;
                    m_geo.numCylinders    = 80;
                }
                return true;
            }
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

    using namespace ArduinoFloppyReader;
    const DiskSurface side = (head == 1) ? DiskSurface::dsUpper : DiskSurface::dsLower;

    if (m_impl->writer.selectTrack((unsigned char)cylinder) != DiagnosticResponse::drOK)
        return false;
    if (m_impl->writer.selectSurface(side) != DiagnosticResponse::drOK)
        return false;

    TrackCache tc;

    if (m_geo.format == DiskFormat::Amiga_ADF) {
        std::vector<std::array<uint8_t, 512>> sects;
        m_impl->writer.readAmigaTrack(m_geo.isHD, cylinder, side, sects);
        tc.sectors.resize(sects.size());
        for (size_t i = 0; i < sects.size(); i++) {
            tc.sectors[i].assign(sects[i].begin(), sects[i].end());
        }
    } else {
        // IBM / FAT / Unknown
        const uint32_t trackNum = cylinder * m_geo.numHeads + head;
        m_impl->writer.readIBMTrack(m_geo.isHD, trackNum,
                                    m_geo.sectorsPerTrack, tc.sectors);
    }

    tc.dirty = false;
    m_cache[key] = std::move(tc);
    return true;
}

bool WaffleDisk::flushTrack(uint32_t cylinder, uint32_t head, TrackCache& tc)
{
    if (!tc.dirty) return true;
    if (isWriteProtected()) return false;

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
    if (!m_open) return true;
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
    if (!m_open) return false;

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
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_open || isWriteProtected()) return false;

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
