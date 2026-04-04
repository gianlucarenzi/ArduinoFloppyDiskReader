#pragma once
// waffle-fuse: disk cache / hardware abstraction layer
// Bridges the DrawBridge/Waffle Arduino device to a generic LBA block device.

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <mutex>
#include <atomic>

// ── Disk format ──────────────────────────────────────────────────────────────
enum class DiskFormat { Unknown, IBM_FAT, Amiga_ADF };

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
    virtual bool     flush() { return true; }
    virtual bool     isDiskPresent() const { return true; }

    // NBD lifecycle hooks (no-ops for non-hardware implementations):

    // Open hardware port without detecting geometry.  Must be called before I/O.
    virtual bool     openDisk()   { return true; }
    // Close hardware port; safe to call after diskRemoved().
    virtual void     closeDisk()  {}
    // Disable motor but keep port open; call before checkPresence() polling.
    virtual void     parkDisk()   {}
    // Re-detect disk format and size (call after openDisk(), e.g. at client connect).
    virtual bool     remount()    { return true; }
    // Lightweight presence probe: fresh open/check/close, motor moves to track 0.
    // Port must be closed (openDisk() not yet called, or closeDisk() already called).
    virtual bool     probePresent() const { return true; }
    // Check presence using the already-open port, no motor movement.
    virtual bool     checkPresence() { return isDiskPresent(); }
};

// ── WaffleDisk ────────────────────────────────────────────────────────────────
// Concrete IDiskImage backed by a DrawBridge/Waffle device connected over serial.
// Tracks are cached in memory; dirty tracks are written back on write.
class WaffleDisk : public IDiskImage {
public:
    // forcedHD: -1 = autodetect (default), 0 = force DD, 1 = force HD
    explicit WaffleDisk(const std::string& portName,
                        bool forceReadOnly = false,
                        int  forcedHD = -1);
    ~WaffleDisk() override;

    // Open device and detect geometry (used by waffle-fuse).
    bool open();
    void close();
    bool isOpen() const { return m_open; }

    // Lightweight probe: open port, check disk presence, close (no head movement).
    // Returns:  0 = disk present
    //           1 = no disk in drive
    //           2 = port error / hardware not found
    static int probe(const std::string& portName);

    // IDiskImage
    bool     readSector(uint32_t lba, uint8_t* buf512)        override;
    bool     writeSector(uint32_t lba, const uint8_t* buf512) override;
    uint32_t totalSectors()   const override;
    bool     isWriteProtected() const override { return m_writeProtected || m_forceReadOnly; }
    const DiskGeometry& geometry() const override { return m_geo; }
    bool     flush() override;
    bool     isDiskPresent() const override { return m_diskPresent.load(); }

    // NBD lifecycle
    bool     openDisk()      override; // open port only (no geometry detection)
    void     closeDisk()     override; // close port
    void     parkDisk()      override; // disable motor, keep port open
    bool     remount()       override; // re-detect geometry on open port
    bool     probePresent()  const override; // static probe (port must be closed)
    bool     checkPresence() override; // check presence using open port, no motor

private:
    struct TrackCache {
        std::vector<std::vector<uint8_t>> sectors;
        bool dirty = false;
    };

    bool loadTrack(uint32_t cylinder, uint32_t head);
    bool flushTrack(uint32_t cylinder, uint32_t head, TrackCache& tc);
    void diskRemoved(); // call under m_mtx on I/O failure
    void lbaToPhysical(uint32_t lba,
                       uint32_t& cylinder, uint32_t& head, uint32_t& sector) const;
    bool detectGeometry();

    std::string           m_portName;
    bool                  m_forceReadOnly;
    int                   m_forcedHD;
    bool                  m_open           = false;
    bool                  m_writerOpen     = false;
    bool                  m_writeProtected = false;
    DiskGeometry          m_geo;

    struct Impl;
    Impl* m_impl = nullptr;

    std::map<uint32_t, TrackCache> m_cache;
    mutable std::mutex             m_mtx;
    std::atomic<bool>              m_diskPresent{false};
};
