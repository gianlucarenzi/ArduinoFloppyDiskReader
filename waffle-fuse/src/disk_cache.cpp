// waffle-fuse: disk cache / hardware abstraction layer implementation

#include "disk_cache.h"
#include "ADFWriter.h"
#include "ArduinoInterface.h"
#include "ibm_sectors.h"
#include "debugmsg.h"

#include <cstring>
#include <algorithm>
#include <iostream>

using namespace ArduinoFloppyReader;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::wstring toWide(const std::string& s)
{
    // Serial port names are plain ASCII (/dev/ttyUSB0, /dev/serial/by-id/...)
    return std::wstring(s.begin(), s.end());
}

const char* diskFormatName(DiskFormat f)
{
    switch (f) {
        case DiskFormat::IBM_FAT:   return "IBM/FAT";
        case DiskFormat::Amiga_ADF: return "Amiga/ADF";
        default:                    return "Unknown";
    }
}

// Fill geometry from a PC boot sector; false if the BPB is not usable.
static bool geometryFromBPB(const uint8_t* boot, DiskGeometry& geo)
{
    const bool sig = boot[510] == 0x55 && boot[511] == 0xAA;
    const bool jmp = boot[0] == 0xEB || boot[0] == 0xE9;
    if (!sig && !jmp) return false;

    uint32_t serial, heads, total, spt, bps;
    if (!IBM::getTrackDetails_IBM(boot, serial, heads, total, spt, bps)) return false;
    if (bps != 512 || spt < 8 || spt > 21 || total == 0) return false;

    geo.format          = DiskFormat::IBM_FAT;
    geo.numHeads        = heads;
    geo.sectorsPerTrack = spt;
    geo.bytesPerSector  = 512;
    geo.numCylinders    = total / (heads * spt);
    if (geo.numCylinders == 0 || geo.numCylinders > 83) geo.numCylinders = 80;
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// ImageDisk
// ═════════════════════════════════════════════════════════════════════════════

ImageDisk::ImageDisk(const std::string& path, bool forceReadOnly)
    : m_path(path), m_readOnly(forceReadOnly)
{}

ImageDisk::~ImageDisk()
{
    if (m_file) fclose(m_file);
}

bool ImageDisk::open()
{
    if (!m_readOnly) m_file = fopen(m_path.c_str(), "r+b");
    if (!m_file) {
        m_file = fopen(m_path.c_str(), "rb");
        m_readOnly = true;
    }
    if (!m_file) return false;

    fseek(m_file, 0, SEEK_END);
    long size = ftell(m_file);
    if (size < 512 * 9) return false;
    m_sectors = (uint32_t)(size / 512);

    uint8_t boot[512] = {};
    if (!readSector(0, boot)) return false;

    m_geo = DiskGeometry{};
    const bool amigaSize = (size == 901120 || size == 1802240);
    if ((boot[0] == 'D' && boot[1] == 'O' && boot[2] == 'S') || amigaSize) {
        m_geo.format          = DiskFormat::Amiga_ADF;
        m_geo.isHD            = size >= 1802240;
        m_geo.sectorsPerTrack = m_geo.isHD ? 22 : 11;
        m_geo.numHeads        = 2;
        m_geo.numCylinders    = m_sectors / (2 * m_geo.sectorsPerTrack);
    } else if (geometryFromBPB(boot, m_geo)) {
        m_geo.isHD = m_geo.sectorsPerTrack > 11;
    } else {
        m_geo.format = DiskFormat::Unknown;
    }
    return true;
}

bool ImageDisk::readSector(uint32_t lba, uint8_t* buf512)
{
    if (!m_file || lba >= m_sectors) return false;
    if (fseek(m_file, (long)lba * 512, SEEK_SET) != 0) return false;
    return fread(buf512, 1, 512, m_file) == 512;
}

bool ImageDisk::writeSector(uint32_t lba, const uint8_t* buf512)
{
    if (!m_file || m_readOnly || lba >= m_sectors) return false;
    if (fseek(m_file, (long)lba * 512, SEEK_SET) != 0) return false;
    return fwrite(buf512, 1, 512, m_file) == 512;
}

bool ImageDisk::flush()
{
    return !m_file || fflush(m_file) == 0;
}

// ═════════════════════════════════════════════════════════════════════════════
// WaffleDisk
// ═════════════════════════════════════════════════════════════════════════════

struct WaffleDisk::Impl {
    ADFWriter writer;
    bool      diskPollSupported = true;
};

bool WaffleDisk::TrackCache::allValid() const
{
    for (bool v : valid) if (!v) return false;
    return !valid.empty();
}

WaffleDisk::WaffleDisk(const Options& opts)
    : m_opts(opts)
{}

WaffleDisk::~WaffleDisk()
{
    close();
}

// ── Open / close ──────────────────────────────────────────────────────────────

WaffleDisk::OpenResult WaffleDisk::open()
{
    if (m_open) return OpenResult::OK;

    m_impl = new Impl();
    // openDevice() also switches the motor on and seeks to track 0
    if (!m_impl->writer.openDevice(toWide(m_opts.port))) {
        delete m_impl;
        m_impl = nullptr;
        return OpenResult::PortError;
    }
    m_motorOn = true;
    m_lastIO = m_lastDiskCheck = Clock::now();

    auto fail = [this](OpenResult r) {
        m_impl->writer.enableReading(false, false);
        m_impl->writer.closeDevice();
        delete m_impl;
        m_impl = nullptr;
        m_cache.clear();
        m_motorOn = false;
        return r;
    };

    DiagnosticResponse wp = m_impl->writer.checkIfDiskIsWriteProtected(true);
    if (wp == DiagnosticResponse::drNoDiskInDrive) return fail(OpenResult::NoDisk);
    if (wp == DiagnosticResponse::drOldFirmware) m_impl->diskPollSupported = false;
    m_writeProtected = (wp == DiagnosticResponse::drWriteProtected);

    bool isHD = false;
    if (m_opts.forcedHD >= 0) {
        isHD = (m_opts.forcedHD == 1);
    } else if (m_impl->writer.GuessDiskDensity(isHD) != ADFResult::adfrComplete) {
        isHD = false;
    }

    // The density hole is only a hint: a DD-formatted HD disk (or a taped one)
    // reads at the other data rate, so unless forced we try both.
    int found = detect(isHD);
    if (found == 0 && m_opts.forcedHD < 0) found = detect(!isHD);
    if (found != 1) {
        if (!m_impl->writer.isDiskInDrive()) return fail(OpenResult::NoDisk);
        return fail(found == 0 ? OpenResult::UnknownFormat : OpenResult::DriveError);
    }

    m_removed = false;
    m_removalReported = false;
    m_open = true;
    return OpenResult::OK;
}

WaffleDisk::OpenResult WaffleDisk::openForFormat(bool& holeIsHD, bool& holeKnown)
{
    holeIsHD = holeKnown = false;
    if (m_open) return OpenResult::OK;

    m_impl = new Impl();
    if (!m_impl->writer.openDevice(toWide(m_opts.port))) {
        delete m_impl;
        m_impl = nullptr;
        return OpenResult::PortError;
    }
    m_motorOn = true;
    m_lastIO = m_lastDiskCheck = Clock::now();

    DiagnosticResponse wp = m_impl->writer.checkIfDiskIsWriteProtected(true);
    if (wp == DiagnosticResponse::drNoDiskInDrive) {
        m_impl->writer.enableReading(false, false);
        m_impl->writer.closeDevice();
        delete m_impl;
        m_impl = nullptr;
        m_motorOn = false;
        return OpenResult::NoDisk;
    }
    m_writeProtected = (wp == DiagnosticResponse::drWriteProtected);
    holeKnown = m_impl->writer.GuessDiskDensity(holeIsHD) == ADFResult::adfrComplete;

    m_removed = false;
    m_removalReported = false;
    m_open = true;
    return OpenResult::OK;
}

int WaffleDisk::writeImage(const DiskGeometry& geo, const std::vector<uint8_t>& image, WriteMode mode,
                           std::function<void(uint32_t, uint32_t, int)> progress)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_open) return -1;
    if (isWriteProtected()) return -1;

    m_geo = geo;
    m_cache.clear();
    const uint32_t spt = geo.sectorsPerTrack;
    if (image.size() < (size_t)geo.numCylinders * geo.numHeads * spt * 512) return -1;

    int failed = 0;
    for (uint32_t cyl = 0; cyl < geo.numCylinders; cyl++) {
        for (uint32_t head = 0; head < geo.numHeads; head++) {
            const size_t base = ((size_t)(cyl * geo.numHeads + head) * spt) * 512;
            TrackCache tc;
            bool empty = true;
            tc.sectors.resize(spt);
            tc.valid.assign(spt, true);
            for (uint32_t s = 0; s < spt; s++) {
                tc.sectors[s].assign(image.begin() + base + s * 512, image.begin() + base + (s + 1) * 512);
                for (uint8_t b : tc.sectors[s]) if (b) { empty = false; break; }
            }
            if (mode == WriteMode::NonEmpty && empty) continue;

            if (mode == WriteMode::Changed) {
                std::vector<std::vector<uint8_t>> cur;
                std::vector<bool> valid;
                if (readTrackRaw(cyl, head, cur, valid)) {
                    bool same = cur.size() == spt;
                    for (uint32_t s = 0; same && s < spt; s++)
                        same = valid[s] && cur[s] == tc.sectors[s];
                    if (same) {
                        if (progress) progress(cyl, head, 1);
                        continue;
                    }
                }
            }

            // A full format starts from a clean track: no leftovers of an older
            // (maybe different) format after the end of the new data
            if (mode == WriteMode::Full && seekTo(cyl, head)) {
                m_impl->writer.device().setDiskCapacity(geo.isHD);
                m_impl->writer.device().eraseCurrentTrack();
            }
            tc.dirty = true;
            const bool ok = flushTrack(cyl * 2 + head, tc);
            if (!ok) {
                failed++;
                if (isWriteProtected()) return -1;
            }
            if (progress) progress(cyl, head, ok ? 0 : -1);
        }
    }
    m_cache.clear();
    return failed;
}

void WaffleDisk::close()
{
    stopBackground();
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_open) return;
    flushLocked();
    m_impl->writer.enableReading(false, false);
    m_impl->writer.closeDevice();
    delete m_impl;
    m_impl = nullptr;
    m_cache.clear();
    m_open = false;
    m_motorOn = false;
}

// ── Geometry / format detection ───────────────────────────────────────────────
// Reads cylinder 0 / head 0 and keeps it in the cache: the filesystem layer
// asks for the boot block straight away.

int WaffleDisk::detect(bool isHD)
{
    m_geo = DiskGeometry{};
    m_geo.isHD = isHD;
    m_cache.clear();

    if (!seekTo(0, 0)) return -1;

    // ── IBM / FAT ─────────────────────────────────────────────────────────────
    {
        const uint32_t spt = isHD ? 18 : 9;
        std::vector<std::vector<uint8_t>> sectors;
        std::vector<bool> valid;
        if (!m_impl->writer.readIBMTrack(isHD, 0, spt, sectors, valid, 2)) return -1;

        if (valid[0] && sectors[0].size() >= 512 && geometryFromBPB(sectors[0].data(), m_geo)) {
            m_geo.isHD = isHD;
            if (m_geo.sectorsPerTrack == spt) {
                TrackCache tc;
                tc.sectors = std::move(sectors);
                tc.valid   = std::move(valid);
                for (size_t i = 0; i < tc.sectors.size(); i++)
                    if (tc.sectors[i].size() != 512) tc.valid[i] = false;
                m_cache[0] = std::move(tc);
            }
            return 1;
        }
    }

    // ── Amiga ─────────────────────────────────────────────────────────────────
    {
        std::vector<std::array<uint8_t, 512>> sectors;
        std::vector<bool> valid;
        if (!seekTo(0, 0)) return -1;
        if (!m_impl->writer.readAmigaTrack(isHD, 0, DiskSurface::dsLower, sectors, valid, 3))
            return -1;

        if (valid[0] && sectors[0][0] == 'D' && sectors[0][1] == 'O' && sectors[0][2] == 'S') {
            m_geo.format          = DiskFormat::Amiga_ADF;
            m_geo.numHeads        = 2;
            m_geo.sectorsPerTrack = isHD ? 22 : 11;
            m_geo.numCylinders    = 80;
            m_geo.bytesPerSector  = 512;

            TrackCache tc;
            tc.valid = valid;
            tc.sectors.resize(sectors.size());
            for (size_t i = 0; i < sectors.size(); i++)
                tc.sectors[i].assign(sectors[i].begin(), sectors[i].end());
            m_cache[0] = std::move(tc);
            return 1;
        }
    }

    m_geo.format = DiskFormat::Unknown;
    return 0;
}

// ── Low level helpers (m_mtx held) ────────────────────────────────────────────

bool WaffleDisk::ensureMotor()
{
    if (m_motorOn) return true;
    // '+' waits for the drive to spin up before answering
    if (m_impl->writer.enableReading(true, false, false) != DiagnosticResponse::drOK) return false;
    m_motorOn = true;
    return true;
}

bool WaffleDisk::seekTo(uint32_t cylinder, uint32_t head)
{
    if (!ensureMotor()) return false;
    const DiskSurface side = (head == 1) ? DiskSurface::dsUpper : DiskSurface::dsLower;
    if (m_impl->writer.selectTrack((unsigned char)cylinder) != DiagnosticResponse::drOK) return false;
    return m_impl->writer.selectSurface(side) == DiagnosticResponse::drOK;
}

bool WaffleDisk::readTrackRaw(uint32_t cylinder, uint32_t head,
                              std::vector<std::vector<uint8_t>>& sectors, std::vector<bool>& valid)
{
    if (!seekTo(cylinder, head)) return false;
    m_lastIO = Clock::now();

    bool ok;
    if (m_geo.format == DiskFormat::Amiga_ADF) {
        std::vector<std::array<uint8_t, 512>> sects;
        const DiskSurface side = (head == 1) ? DiskSurface::dsUpper : DiskSurface::dsLower;
        ok = m_impl->writer.readAmigaTrack(m_geo.isHD, cylinder, side, sects, valid);
        sectors.resize(sects.size());
        for (size_t i = 0; i < sects.size(); i++)
            sectors[i].assign(sects[i].begin(), sects[i].end());
    } else {
        const uint32_t trackNum = cylinder * m_geo.numHeads + head;
        ok = m_impl->writer.readIBMTrack(m_geo.isHD, trackNum, m_geo.sectorsPerTrack, sectors, valid);
        for (size_t i = 0; i < sectors.size(); i++)
            if (sectors[i].size() != 512) valid[i] = false;
    }
    m_lastIO = Clock::now();
    return ok;
}

// Returns the cached track, (re)reading it from the disk if needed.  Sectors
// that failed to decode are retried on later accesses; good ones (and any
// pending changes) are never overwritten by a re-read.
WaffleDisk::TrackCache* WaffleDisk::loadTrack(uint32_t cylinder, uint32_t head)
{
    // Each read already tries several revolutions: a track with bad sectors is
    // retried only a couple of times, not on every access to it.
    static const int kMaxRereads = 2;
    const uint32_t key = cylinder * 2 + head;
    auto it = m_cache.find(key);
    if (it != m_cache.end() && (it->second.allValid() || it->second.rereads >= kMaxRereads))
        return &it->second;

    // Before a re-read, seek far away and back like WaffleCopy (and the Amiga) do:
    // re-settling the head sometimes recovers a track
    if (it != m_cache.end()) {
        m_impl->writer.selectTrack((unsigned char)(cylinder < 40 ? cylinder + 30 : cylinder - 30),
                                   TrackSearchSpeed::tssSlow);
    }

    std::vector<std::vector<uint8_t>> sectors;
    std::vector<bool> valid;
    if (!readTrackRaw(cylinder, head, sectors, valid)) {
        if (m_impl->writer.checkIfDiskIsWriteProtected(true) == DiagnosticResponse::drNoDiskInDrive)
            m_removed = true;
        return nullptr;
    }

    if (it == m_cache.end()) {
        TrackCache tc;
        tc.sectors = std::move(sectors);
        tc.valid   = std::move(valid);
        it = m_cache.emplace(key, std::move(tc)).first;
    } else {
        TrackCache& tc = it->second;
        tc.rereads++;
        for (size_t i = 0; i < tc.valid.size() && i < valid.size(); i++) {
            if (!tc.valid[i] && valid[i]) {
                tc.sectors[i] = std::move(sectors[i]);
                tc.valid[i]   = true;
            }
        }
    }
    return &it->second;
}

bool WaffleDisk::flushTrack(uint32_t key, TrackCache& tc)
{
    if (!tc.dirty) return true;
    if (isWriteProtected() || m_removed) return false;

    const uint32_t cylinder = key / 2;
    const uint32_t head     = key % 2;
    const DiskSurface side  = (head == 1) ? DiskSurface::dsUpper : DiskSurface::dsLower;

    for (int attempt = 0; attempt < 3; attempt++) {
        if (!seekTo(cylinder, head)) return false;
        m_lastIO = Clock::now();

        DiagnosticResponse resp;
        if (m_geo.format == DiskFormat::Amiga_ADF) {
            std::vector<std::array<uint8_t, 512>> sects(tc.sectors.size());
            for (size_t i = 0; i < tc.sectors.size(); i++)
                memcpy(sects[i].data(), tc.sectors[i].data(), 512);
            resp = m_impl->writer.writeAmigaTrack(m_geo.isHD, cylinder, side, sects, true);
        } else {
            const uint32_t trackNum = cylinder * m_geo.numHeads + head;
            resp = m_impl->writer.writeIBMTrack(m_geo.isHD, trackNum, m_geo.nonStdTiming,
                                                tc.sectors, true);
        }
        m_lastIO = Clock::now();

        if (resp == DiagnosticResponse::drWriteProtected) {
            m_writeProtected = true;
            return false;
        }
        if (resp != DiagnosticResponse::drOK) continue;

        if (!m_opts.verifyWrites) {
            tc.dirty = false;
            return true;
        }

        std::vector<std::vector<uint8_t>> back;
        std::vector<bool> valid;
        if (!readTrackRaw(cylinder, head, back, valid)) continue;
        bool same = back.size() == tc.sectors.size();
        for (size_t i = 0; same && i < back.size(); i++)
            same = valid[i] && back[i] == tc.sectors[i];
        if (same) {
            tc.dirty = false;
            tc.writeFailed = false;
            return true;
        }
        DEBUGMSG("verify failed on cylinder %u head %u, retrying", cylinder, head);
    }
    return false;
}

bool WaffleDisk::flushLocked()
{
    if (!m_open) return true;
    bool ok = true;
    for (auto& kv : m_cache)
        if (kv.second.dirty)
            ok &= flushTrack(kv.first, kv.second);
    return ok;
}

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

// ── Public I/O ────────────────────────────────────────────────────────────────

int WaffleDisk::scanTrack(uint32_t cylinder, uint32_t head)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_open) return -1;
    m_cache.erase(cylinder * 2 + head);
    TrackCache* tc = loadTrack(cylinder, head);
    if (!tc) return -1;
    int good = 0;
    for (bool v : tc->valid) good += v;
    m_cache.erase(cylinder * 2 + head);
    return good;
}

bool WaffleDisk::flush()
{
    std::lock_guard<std::mutex> lk(m_mtx);
    return flushLocked();
}

bool WaffleDisk::hasPendingWrites() const
{
    std::lock_guard<std::mutex> lk(m_mtx);
    for (auto& kv : m_cache) if (kv.second.dirty) return true;
    return false;
}

bool WaffleDisk::readSector(uint32_t lba, uint8_t* buf512)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_open || m_removed || lba >= totalSectors()) return false;

    uint32_t cylinder, head, sector;
    lbaToPhysical(lba, cylinder, head, sector);

    TrackCache* tc = loadTrack(cylinder, head);
    m_lastIO = Clock::now();
    if (!tc || sector >= tc->sectors.size() || !tc->valid[sector]) {
        memset(buf512, 0, 512);
        return false;
    }
    memcpy(buf512, tc->sectors[sector].data(), 512);
    return true;
}

bool WaffleDisk::writeSector(uint32_t lba, const uint8_t* buf512)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_open || m_removed || isWriteProtected() || lba >= totalSectors()) return false;

    uint32_t cylinder, head, sector;
    lbaToPhysical(lba, cylinder, head, sector);

    // The whole track gets rewritten, so every other sector on it must be
    // known: never write back a track that still has unreadable sectors.
    TrackCache* tc = loadTrack(cylinder, head);
    m_lastIO = Clock::now();
    if (!tc || sector >= tc->sectors.size()) return false;
    if (!tc->allValid()) return false;

    if (memcmp(tc->sectors[sector].data(), buf512, 512) == 0) return true;

    tc->sectors[sector].assign(buf512, buf512 + 512);
    const auto now = Clock::now();
    if (!tc->dirty) tc->firstDirty = now;
    tc->dirty       = true;
    tc->writeFailed = false;
    tc->lastWrite   = now;
    return true;
}

// ── Background thread ─────────────────────────────────────────────────────────

void WaffleDisk::startBackground(std::function<void(Event)> onEvent)
{
    stopBackground();
    m_onEvent = std::move(onEvent);
    m_threadStop = false;
    m_thread = std::thread([this] { backgroundLoop(); });
}

void WaffleDisk::stopBackground()
{
    if (!m_thread.joinable()) return;
    {
        std::lock_guard<std::mutex> tl(m_threadMtx);
        m_threadStop = true;
    }
    m_threadCv.notify_all();
    m_thread.join();
}

void WaffleDisk::backgroundLoop()
{
    using namespace std::chrono;
    const auto flushDelay = milliseconds(m_opts.flushDelayMs);
    const auto maxDirty   = milliseconds(m_opts.flushDelayMs * 4);
    const auto motorOff   = milliseconds(m_opts.motorOffMs);
    const auto pollEvery  = milliseconds(std::max(500, m_opts.diskPollMs));

    for (;;) {
        {
            std::unique_lock<std::mutex> tl(m_threadMtx);
            m_threadCv.wait_for(tl, milliseconds(200), [this] { return m_threadStop; });
            if (m_threadStop) return;
        }

        std::vector<Event> events;
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            if (!m_open || m_removalReported) continue;
            const auto now = Clock::now();

            // Removal noticed by a failed read/seek: report it once
            if (m_removed) {
                bool dirty = false;
                for (auto& kv : m_cache) dirty |= kv.second.dirty;
                m_cache.clear();
                m_removalReported = true;
                events.push_back(dirty ? Event::DiskRemovedDirty : Event::DiskRemoved);
            }

            // Write back one track per pass so FUSE requests are not starved.
            bool pending = false;
            for (auto& kv : m_cache) {
                TrackCache& tc = kv.second;
                if (!tc.dirty || tc.writeFailed) continue;
                if (now - tc.lastWrite >= flushDelay || now - tc.firstDirty >= maxDirty) {
                    if (!flushTrack(kv.first, tc)) {
                        tc.writeFailed = true;
                        events.push_back(Event::WriteError);
                    }
                    if (tc.dirty && !tc.writeFailed) pending = true;
                    break;
                }
                pending = true;
            }
            for (auto& kv : m_cache)
                if (kv.second.dirty && !kv.second.writeFailed) pending = true;

            // Disk-change polling while idle
            if (!m_removed && m_impl->diskPollSupported && !pending &&
                now - m_lastIO >= pollEvery && now - m_lastDiskCheck >= pollEvery) {
                m_lastDiskCheck = now;
                DiagnosticResponse r = m_impl->writer.device().checkForDisk(true);
                if (r == DiagnosticResponse::drNoDiskInDrive) {
                    bool dirty = false;
                    for (auto& kv : m_cache) dirty |= kv.second.dirty;
                    m_removed = true;
                    m_removalReported = true;
                    m_cache.clear();
                    events.push_back(dirty ? Event::DiskRemovedDirty : Event::DiskRemoved);
                } else if (r == DiagnosticResponse::drOldFirmware) {
                    m_impl->diskPollSupported = false;
                } else if (r == DiagnosticResponse::drOK || r == DiagnosticResponse::drStatusError) {
                    m_writeProtected = m_impl->writer.checkIfDiskIsWriteProtected(false) ==
                                       DiagnosticResponse::drWriteProtected;
                }
            }

            // Motor off when idle, like the Amiga does
            if (!m_removed && m_motorOn && !pending && now - m_lastIO >= motorOff) {
                m_impl->writer.enableReading(false, false);
                m_motorOn = false;
            }
        }
        for (Event e : events)
            if (m_onEvent) m_onEvent(e);
    }
}

// ── Disk presence check / wait (no mount) ─────────────────────────────────────

int WaffleDisk::probe(const std::string& portName)
{
    ArduinoInterface dev;
    if (dev.openPort(toWide(portName)) != DiagnosticResponse::drOK) return 2;
    DiagnosticResponse r = dev.checkForDisk(true);
    const bool present = dev.isDiskInDrive();   // read before closePort(), which resets it
    dev.closePort();
    if (r == DiagnosticResponse::drOK || r == DiagnosticResponse::drStatusError ||
        r == DiagnosticResponse::drNoDiskInDrive)
        return present ? 0 : 1;
    return 2;
}

int WaffleDisk::waitForDisk(const std::string& portName, bool wantPresent, int timeoutSec, int pollMs)
{
    ArduinoInterface dev;
    if (dev.openPort(toWide(portName)) != DiagnosticResponse::drOK) return 2;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSec);
    int errors = 0;
    for (;;) {
        DiagnosticResponse r = dev.checkForDisk(true);
        if (r == DiagnosticResponse::drOldFirmware) { dev.closePort(); return 2; }
        if (r == DiagnosticResponse::drOK || r == DiagnosticResponse::drNoDiskInDrive ||
            r == DiagnosticResponse::drStatusError) {
            errors = 0;
            if (dev.isDiskInDrive() == wantPresent) { dev.closePort(); return 0; }
        } else if (++errors > 5) {
            dev.closePort();
            return 2;
        }
        if (timeoutSec > 0 && std::chrono::steady_clock::now() >= deadline) {
            dev.closePort();
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(std::max(500, pollMs)));
    }
}
