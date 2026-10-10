#pragma once
// waffle-fuse: disk cache / hardware abstraction layer
// Bridges the DrawBridge/Waffle Arduino device (or a disk image file) to a
// generic 512-byte LBA block device.

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>

// ── Disk format ──────────────────────────────────────────────────────────────
enum class DiskFormat { Unknown, IBM_FAT, Amiga_ADF };

const char* diskFormatName(DiskFormat f);

struct DiskGeometry {
    DiskFormat format        = DiskFormat::Unknown;
    uint32_t   numCylinders  = 80;
    uint32_t   numHeads      = 2;
    uint32_t   sectorsPerTrack = 9;   // physical MFM sectors per track
    uint32_t   bytesPerSector  = 512;
    bool       isHD          = false; // true = 1.44 MB / Amiga HD
    bool       nonStdTiming  = false; // Atari ST timing
};

// ── IDiskImage ────────────────────────────────────────────────────────────────
// Abstract 512-byte block device; LBA is 0-based.
class IDiskImage {
public:
    virtual ~IDiskImage() = default;
    virtual bool     readSector(uint32_t lba, uint8_t* buf512)        = 0;
    virtual bool     writeSector(uint32_t lba, const uint8_t* buf512) = 0;
    virtual uint32_t totalSectors()   const = 0;
    virtual bool     isWriteProtected() const = 0;
    virtual const DiskGeometry& geometry() const = 0;
    virtual bool     flush() { return true; } // write any cached data to the medium
};

// ── ImageDisk ─────────────────────────────────────────────────────────────────
// IDiskImage backed by a raw sector image (.adf / .img / .ima).  Used for
// testing the filesystem layers without the hardware.
class ImageDisk : public IDiskImage {
public:
    ImageDisk(const std::string& path, bool forceReadOnly);
    ~ImageDisk() override;

    bool open();

    bool     readSector(uint32_t lba, uint8_t* buf512)        override;
    bool     writeSector(uint32_t lba, const uint8_t* buf512) override;
    uint32_t totalSectors()   const override { return m_sectors; }
    bool     isWriteProtected() const override { return m_readOnly; }
    const DiskGeometry& geometry() const override { return m_geo; }
    bool     flush() override;

private:
    std::string  m_path;
    bool         m_readOnly;
    FILE*        m_file    = nullptr;
    uint32_t     m_sectors = 0;
    DiskGeometry m_geo;
};

// ── WaffleDisk ────────────────────────────────────────────────────────────────
// IDiskImage backed by a DrawBridge/Waffle device connected over serial.
//
// Behaves like the Amiga trackdisk.device: whole tracks are cached, writes go
// to the cache and are written back (and verified) by a background thread
// shortly after the last change; the motor is switched off when idle and the
// drive is polled for disk removal.
class WaffleDisk : public IDiskImage {
public:
    struct Options {
        std::string port;
        bool forceReadOnly = false;
        int  forcedHD      = -1;    // -1 = autodetect, 0 = DD, 1 = HD
        int  flushDelayMs  = 1000;  // write back a dirty track this long after its last change
        int  motorOffMs    = 3000;  // switch the motor off after this much idle time
        int  diskPollMs    = 3000;  // check for disk removal this often (trackdisk.device: ~2-3 s)
        bool verifyWrites  = true;  // read every written track back and compare
    };

    enum class OpenResult { OK, PortError, NoDisk, DriveError, UnknownFormat };

    enum class Event {
        DiskRemoved,       // disk taken out, nothing was pending
        DiskRemovedDirty,  // disk taken out while changes were still pending (lost)
        WriteError,        // a track could not be written/verified
    };

    explicit WaffleDisk(const Options& opts);
    ~WaffleDisk() override;

    // Open the device, detect density, geometry and format.
    OpenResult open();
    void close();
    bool isOpen() const { return m_open; }

    // Start/stop the background thread (write-back, motor off, disk-change polling).
    // onEvent is called from that thread, without any lock held.
    void startBackground(std::function<void(Event)> onEvent);
    void stopBackground();

    // Open the device for formatting: no format detection (the disk may be
    // blank or in another format).  holeIsHD/holeKnown report the density
    // hole of the inserted disk.
    OpenResult openForFormat(bool& holeIsHD, bool& holeKnown);

    // Write a complete image with the given geometry, track by track, each
    // written track verified.  Modes:
    //   Full     – every track, erased first (formatting)
    //   NonEmpty – only tracks with non-zero data (quick format)
    //   Changed  – read each track, write only those that differ from the image
    // progress(cylinder, head, status) is called after each track, status:
    // 0 = written, 1 = unchanged (skipped), -1 = failed; returning false stops
    // between two tracks.  Returns the number of tracks that could not be
    // written, -1 if write-protected, -2 if stopped.
    enum class WriteMode { Full, NonEmpty, Changed };
    int writeImage(const DiskGeometry& geo, const std::vector<uint8_t>& image, WriteMode mode,
                   std::function<bool(uint32_t, uint32_t, int)> progress);

    // Read every track of 'geo' into 'image' (unreadable sectors as zeros).
    // A track with bad sectors is read again (after a seek away and back) a
    // couple of times.  progress(cylinder, head, goodSectors) after each track;
    // returning false stops the copy.  Returns the number of unreadable
    // sectors, -1 on a drive error, -2 if stopped.
    int readImage(const DiskGeometry& geo, std::vector<uint8_t>& image,
                  std::function<bool(uint32_t, uint32_t, int)> progress);

    // Flux-level dump to an SCP file (WaffleCopy's writer), for copy-protected
    // or non-DOS disks.  progress(cylinder, head) before each track; returning
    // false stops the copy.  Returns 0 = ok, 1 = done but some tracks had read
    // problems, -1 = failed, -2 = stopped.
    int dumpSCP(const std::string& path, bool isHD, unsigned int tracks, unsigned int revolutions,
                std::function<bool(int, int)> progress);

    // One-shot disk presence check without spinning the motor up.
    // Returns 0 = disk present, 1 = no disk, 2 = port/hardware error.
    static int probe(const std::string& portName);

    // Keep the serial port open and poll until a disk is present (wantPresent=true)
    // or absent.  timeoutSec <= 0 waits forever.
    // Returns 0 = condition met, 1 = timeout, 2 = port/hardware error.
    static int waitForDisk(const std::string& portName, bool wantPresent, int timeoutSec,
                           int pollMs = 3000);

    // IDiskImage
    bool     readSector(uint32_t lba, uint8_t* buf512)        override;
    bool     writeSector(uint32_t lba, const uint8_t* buf512) override;
    uint32_t totalSectors()   const override;
    bool     isWriteProtected() const override { return m_writeProtected || m_opts.forceReadOnly; }
    const DiskGeometry& geometry() const override { return m_geo; }
    bool     flush() override;

    // Read one track afresh (no retries beyond the normal ones); number of good sectors or -1
    int scanTrack(uint32_t cylinder, uint32_t head);

    bool diskRemoved() const { return m_removed; }
    bool hasPendingWrites() const;

private:
    struct TrackCache {
        std::vector<std::vector<uint8_t>> sectors; // indexed by sector number, 512 bytes each
        std::vector<bool> valid;                   // sector decoded with good checksums
        bool dirty       = false;
        bool writeFailed = false;                  // background retries stop after a failure
        int  rereads     = 0;                      // extra reads tried for unreadable sectors
        std::chrono::steady_clock::time_point firstDirty, lastWrite;
        bool allValid() const;
    };

    using Clock = std::chrono::steady_clock;

    bool ensureMotor();
    bool seekTo(uint32_t cylinder, uint32_t head);
    bool readTrackRaw(uint32_t cylinder, uint32_t head,
                      std::vector<std::vector<uint8_t>>& sectors, std::vector<bool>& valid);
    TrackCache* loadTrack(uint32_t cylinder, uint32_t head);
    bool flushTrack(uint32_t key, TrackCache& tc);
    bool flushLocked();
    void lbaToPhysical(uint32_t lba,
                       uint32_t& cylinder, uint32_t& head, uint32_t& sector) const;
    int  detect(bool isHD);   // 1 = recognised, 0 = unknown format, -1 = drive error
    void backgroundLoop();

    Options      m_opts;
    bool         m_open           = false;
    bool         m_writeProtected = false;
    std::atomic<bool> m_removed{false};
    bool         m_removalReported = false;
    bool         m_motorOn        = false;
    DiskGeometry m_geo;
    Clock::time_point m_lastIO;
    Clock::time_point m_lastDiskCheck;

    struct Impl;           // owns the ADFWriter (keeps Arduino headers out of here)
    Impl* m_impl = nullptr;

    std::map<uint32_t, TrackCache> m_cache;  // key = cylinder * 2 + head
    mutable std::mutex             m_mtx;

    std::thread              m_thread;
    std::mutex               m_threadMtx;
    std::condition_variable  m_threadCv;
    bool                     m_threadStop = false;
    std::function<void(Event)> m_onEvent;
};
