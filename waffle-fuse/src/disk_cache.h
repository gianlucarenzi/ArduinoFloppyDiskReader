#pragma once
// waffle-fuse: disk cache / hardware abstraction layer
// Bridges the DrawBridge/Waffle Arduino device to a generic LBA block device.

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <map>
#include <mutex>

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
    virtual bool     flush() { return true; } // optional: flush dirty cache to hardware
};

// ── WaffleDisk ────────────────────────────────────────────────────────────────
// Concrete IDiskImage backed by a DrawBridge/Waffle device connected over serial.
// Tracks are cached in memory; dirty tracks are written back immediately.
class WaffleDisk : public IDiskImage {
public:
    // forcedHD: -1 = autodetect (default), 0 = force DD, 1 = force HD
    explicit WaffleDisk(const std::string& portName,
                        bool forceReadOnly = false,
                        int  forcedHD = -1);
    ~WaffleDisk() override;

    // Open device, detect disk geometry and format.  Returns true on success.
    bool open();
    void close();
    bool isOpen() const { return m_open; }

    // IDiskImage
    bool     readSector(uint32_t lba, uint8_t* buf512)        override;
    bool     writeSector(uint32_t lba, const uint8_t* buf512) override;
    uint32_t totalSectors()   const override;
    bool     isWriteProtected() const override { return m_writeProtected || m_forceReadOnly; }
    const DiskGeometry& geometry() const override { return m_geo; }

    // Flush all dirty cached tracks to hardware.
    bool flush();

private:
    // Map (cylinder * 2 + head) → decoded sectors for that track.
    // For Amiga: each entry is 512 bytes.
    // For IBM:   each entry has bytesPerSector bytes.
    struct TrackCache {
        std::vector<std::vector<uint8_t>> sectors; // indexed by sector number
        bool dirty = false;
    };

    bool loadTrack(uint32_t cylinder, uint32_t head);
    bool flushTrack(uint32_t cylinder, uint32_t head, TrackCache& tc);
    void lbaToPhysical(uint32_t lba,
                       uint32_t& cylinder, uint32_t& head, uint32_t& sector) const;
    bool detectGeometry();

    std::string           m_portName;
    bool                  m_forceReadOnly;
    int                   m_forcedHD;      // -1=auto, 0=DD, 1=HD
    bool                  m_open           = false;
    bool                  m_writeProtected = false;
    DiskGeometry          m_geo;

    // ADFWriter is held by value (it owns the ArduinoInterface)
    // We forward-declare to avoid pulling Qt-free ADFWriter.h here.
    struct Impl;
    Impl* m_impl = nullptr;

    std::map<uint32_t, TrackCache> m_cache;
    mutable std::mutex             m_mtx;
};
