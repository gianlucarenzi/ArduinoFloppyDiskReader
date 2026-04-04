// waffle-fuse  –  FUSE driver for DrawBridge / Waffle floppy reader
//
// Usage:
//   waffle-fuse <serial-port> <mountpoint> [options]
//
// Options (in addition to standard FUSE options):
//   -o ro               mount read-only (default for Amiga)
//   -o format=fat|affs  force filesystem type (default: autodetect)
//   -o debug_hw         enable hardware debug messages
//
// Examples:
//   # FAT12 720K PC disk
//   waffle-fuse /dev/ttyUSB0 /mnt/floppy
//
//   # Amiga OFS/FFS disk (always read-only)
//   waffle-fuse /dev/ttyUSB0 /mnt/amiga -o format=affs
//
//   # Once mounted, standard tools work:
//   ls /mnt/floppy
//   cp /mnt/floppy/readme.txt .
//   # For FAT: mount -t msdos -o loop /mnt/floppy/disk.img /mnt/disk
//   # (or use directly without loop — the FUSE driver IS the filesystem)

#include "fuse_compat.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include "disk_cache.h"
#include "fat_fs.h"
#include "affs_fs.h"
#include "debugmsg.h"

// ── GVfs bookmark helpers (Linux only) ───────────────────────────────────────
// File managers that use GVfs (Nautilus, Thunar, Nemo, PCManFM-Qt) watch
// ~/.config/gtk-3.0/bookmarks via inotify and update the sidebar immediately.
// KDE Dolphin uses Solid which detects fuse.waffle from /proc/mounts directly.
// A Desktop eject shortcut is also created so the user can unmount with one
// click from any file manager or the desktop.
// Not applicable on macOS or Windows, where different desktop mechanisms exist.
#if !defined(_WIN32) && !defined(__APPLE__)

#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

static void gvfs_bookmark_add(const std::string& mountPoint, const std::string& displayName)
{
    const char* home = getenv("HOME");
    if (!home) return;

    std::string dir  = std::string(home) + "/.config/gtk-3.0";
    std::string path = dir + "/bookmarks";
    std::string uri  = "file://" + mountPoint;
    std::string entry = uri + " " + displayName;

    mkdir(dir.c_str(), 0755);   // no-op if already exists

    // Read existing lines, dropping any stale entry for this URI
    std::vector<std::string> lines;
    {
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line))
            if (line.rfind(uri, 0) != 0)   // keep lines that don't start with our URI
                lines.push_back(line);
    }
    lines.push_back(entry);

    std::ofstream out(path);
    for (const auto& l : lines)
        out << l << "\n";
}

static void gvfs_bookmark_remove(const std::string& mountPoint)
{
    const char* home = getenv("HOME");
    if (!home) return;

    std::string path = std::string(home) + "/.config/gtk-3.0/bookmarks";
    std::string uri  = "file://" + mountPoint;

    std::vector<std::string> lines;
    {
        std::ifstream in(path);
        if (!in) return;
        std::string line;
        while (std::getline(in, line))
            if (line.rfind(uri, 0) != 0)
                lines.push_back(line);
    }

    std::ofstream out(path);
    for (const auto& l : lines)
        out << l << "\n";
}

// Create ~/Desktop/Eject <label>.desktop so the user can unmount from the GUI.
// fusermount triggers waffle_destroy() which flushes dirty tracks before close.
static std::string desktop_eject_create(const std::string& mountPoint,
                                        const std::string& displayName)
{
    const char* home = getenv("HOME");
    if (!home) return {};

    std::string desktop = std::string(home) + "/Desktop";
    struct stat st{};
    if (::stat(desktop.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
        return {};

    // Build a filesystem-safe filename
    std::string safe = displayName;
    for (char& c : safe)
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?') c = '-';

    std::string path = desktop + "/Eject " + safe + ".desktop";

    std::ofstream out(path);
    out << "[Desktop Entry]\n"
        << "Version=1.0\n"
        << "Type=Application\n"
        << "Name=Eject " << displayName << "\n"
        << "Comment=Flush and unmount " << displayName << "\n"
        << "Exec=sh -c 'fusermount3 -u \"" << mountPoint << "\" 2>/dev/null"
        <<           " || fusermount -u \"" << mountPoint << "\" 2>/dev/null'\n"
        << "Icon=media-eject\n"
        << "Terminal=false\n"
        << "StartupNotify=false\n";

    chmod(path.c_str(), 0755);
    return path;
}

static void desktop_eject_remove(const std::string& path)
{
    if (!path.empty())
        unlink(path.c_str());
}

#else
static void gvfs_bookmark_add(const std::string&, const std::string&) {}
static void gvfs_bookmark_remove(const std::string&) {}
static std::string desktop_eject_create(const std::string&, const std::string&) { return {}; }
static void desktop_eject_remove(const std::string&) {}
#endif

// ── Program state (kept alive for the duration of the mount) ─────────────────
struct WaffleState {
    std::unique_ptr<WaffleDisk> disk;
    void* fsCtx  = nullptr;    // opaque: FatFs* or AffsFs*
    bool  isAFFS = false;
    enum class FSType { Auto, FAT, AFFS } fsType = FSType::Auto;
    std::string serialPort;
    std::string mountPoint;
    std::string ejectDesktopFile;   // path of ~/Desktop/Eject *.desktop (Linux)
    bool forceRO = false;
    bool debugHW = false;
    int  forcedHD = -1;   // -1=auto, 0=force DD, 1=force HD
};

static WaffleState* g_state = nullptr;

// ── FUSE option handling ──────────────────────────────────────────────────────
struct WaffleOptions {
    char* format    = nullptr;
    char* density   = nullptr;   // "dd" or "hd"
    int   ro        = 0;
    int   debug_hw  = 0;
};

#define WAFFLE_OPT(t, p, v) { t, offsetof(WaffleOptions, p), v }

static const struct fuse_opt waffle_opts[] = {
    WAFFLE_OPT("format=%s",  format,   0),
    WAFFLE_OPT("density=%s", density,  0),
    WAFFLE_OPT("ro",         ro,       1),
    WAFFLE_OPT("debug_hw",   debug_hw, 1),
    FUSE_OPT_END
};

// ── Initialisation (called from FUSE after fork) ──────────────────────────────
static void* waffle_init(WF_INIT_SIG(conn))
{
    (void)conn;
    WF_CFG_SET(kernel_cache, 0);
    WF_CFG_SET(direct_io,    1);

    WaffleState* st = g_state;

    DebugMsg::init(st->debugHW);
    DebugMsg::initVerbose(st->debugHW);

    // Open the hardware
    st->disk = std::make_unique<WaffleDisk>(st->serialPort, st->forceRO, st->forcedHD);
    if (!st->disk->open()) {
        std::cerr << "waffle-fuse: cannot open device " << st->serialPort << "\n";
        fuse_exit(fuse_get_context()->fuse);
        return nullptr;
    }

    std::cout << "waffle-fuse: disk opened  "
              << "format=" << (st->disk->geometry().format == DiskFormat::IBM_FAT ? "IBM/FAT" :
                                st->disk->geometry().format == DiskFormat::Amiga_ADF ? "Amiga/ADF" : "Unknown")
              << "  HD=" << (st->disk->geometry().isHD ? "yes" : "no")
              << "  sectors=" << st->disk->totalSectors() << "\n";

    // Build filesystem object based on detected / forced format
    DiskFormat fmt = st->disk->geometry().format;

    bool useAFFS = (st->fsType == WaffleState::FSType::AFFS) ||
                   (st->fsType == WaffleState::FSType::Auto && fmt == DiskFormat::Amiga_ADF);

    if (useAFFS) {
        st->fsCtx = affs_create(st->disk.get());
        if (!st->fsCtx) {
            std::cerr << "waffle-fuse: AFFS init failed, trying FAT\n";
            useAFFS = false;
        } else {
            st->isAFFS = true;
            bool isFFS = affs_is_ffs(st->fsCtx);
            bool isHD  = st->disk->geometry().isHD;
            std::cout << "waffle-fuse: mounted as Amiga " << (isFFS ? "FFS" : "OFS")
                      << (st->disk->isWriteProtected() ? " (read-only)" : " (read-write)") << "\n";
            std::string label = std::string("Amiga Floppy ") + (isFFS ? "FFS" : "OFS")
                                + (isHD ? " HD" : " DD");
            gvfs_bookmark_add(st->mountPoint, label);
            st->ejectDesktopFile = desktop_eject_create(st->mountPoint, label);
            return st->fsCtx;
        }
    }

    // FAT
    st->fsCtx = fat_fs_new(st->disk.get());
    if (!st->fsCtx) {
        std::cerr << "waffle-fuse: FAT init failed\n";
        fuse_exit(fuse_get_context()->fuse);
        return nullptr;
    }
    bool isFAT16 = fat_is_fat16(st->fsCtx);
    bool isHD    = st->disk->geometry().isHD;
    std::cout << "waffle-fuse: mounted as FAT" << (isFAT16 ? "16" : "12")
              << (st->disk->isWriteProtected() ? " (read-only)" : " (read-write)") << "\n";
    {
        std::string label = std::string("DOS Floppy FAT") + (isFAT16 ? "16" : "12")
                            + (isHD ? " HD" : " DD");
        gvfs_bookmark_add(st->mountPoint, label);
        st->ejectDesktopFile = desktop_eject_create(st->mountPoint, label);
    }
    return st->fsCtx;
}

static void waffle_destroy(void* data)
{
    (void)data;
    if (g_state) {
        if (g_state->fsCtx) {
            if (g_state->isAFFS) affs_destroy(g_state->fsCtx);
            else                 fat_fs_free(g_state->fsCtx);
            g_state->fsCtx = nullptr;
        }
        if (g_state->disk) g_state->disk->flush();
        gvfs_bookmark_remove(g_state->mountPoint);
        desktop_eject_remove(g_state->ejectDesktopFile);
    }
}

// ── Dispatch: at init() time we return a pointer to the FS object.
//   The FS-specific callbacks (fat_* / affs_*) retrieve it via
//   fuse_get_context()->private_data and static_cast to the right type.
//   We must override init/destroy to inject our open/close logic, but
//   all other ops come directly from the chosen FS driver.

// ── main ─────────────────────────────────────────────────────────────────────

static void usage(const char* progname)
{
    std::cerr <<
        progname << "  [platform: " WF_PLATFORM_STR
                    "  FUSE API: " WF_FUSE_API_STR
                    "  FUSE runtime: " << wf_fuse_runtime_version() << "]\n"
        "\n"
        "Usage: " << progname << " <serial-port> <mountpoint> [fuse-options]\n"
        "       " << progname << " --probe <serial-port>\n"
        "\n"
        "  --probe    Check if a disk is present (exit 0=disk, 1=no disk, 2=error).\n"
        "\n"
        "Extra options (pass with -o):\n"
        "  format=fat|affs   force filesystem type (default: autodetect)\n"
        "  debug_hw          enable hardware/debug messages\n"
        "\n"
        "Examples:\n"
        "  " << progname << " /dev/ttyUSB0 /mnt/floppy\n"
        "  " << progname << " /dev/ttyUSB0 /mnt/amiga -o format=affs,ro\n"
        "  " << progname << " /dev/ttyUSB0 /mnt/floppy -f   # foreground\n"
        "  " << progname << " --probe /dev/ttyUSB0           # disk check\n";
}

int main(int argc, char* argv[])
{
    // --probe <serial-port>: lightweight disk-presence check; no mount.
    // Exit code: 0 = disk present, 1 = no disk, 2 = port/hardware error.
    if (argc >= 3 && strcmp(argv[1], "--probe") == 0) {
        int r = WaffleDisk::probe(argv[2]);
        switch (r) {
            case 0: std::cout << "disk\n";   break;
            case 1: std::cout << "nodisk\n"; break;
            default:std::cout << "error\n";  break;
        }
        return r;
    }

    // We expect at least two non-option arguments: <port> <mountpoint>
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    WaffleState state;
    g_state = &state;

    // Extract serial port (first positional arg before fuse args)
    state.serialPort = argv[1];
    state.mountPoint = argv[2];

    // Build a new argv without our serial-port argument for FUSE
    std::vector<char*> fuse_argv;
    fuse_argv.push_back(argv[0]);
    for (int i = 2; i < argc; i++)
        fuse_argv.push_back(argv[i]);
    int fuse_argc = (int)fuse_argv.size();

    // Parse our custom options
    struct fuse_args args = FUSE_ARGS_INIT(fuse_argc, fuse_argv.data());
    WaffleOptions opts{};
    if (fuse_opt_parse(&args, &opts, waffle_opts, nullptr) == -1)
        return 1;

    if (opts.format) {
        if (strcmp(opts.format, "fat")  == 0) state.fsType  = WaffleState::FSType::FAT;
        else if (strcmp(opts.format, "affs") == 0) {
            state.fsType  = WaffleState::FSType::AFFS;
        } else {
            std::cerr << "waffle-fuse: unknown format '" << opts.format << "'\n";
            return 1;
        }
    }

    if (opts.density) {
        if (strcmp(opts.density, "dd") == 0)       state.forcedHD = 0;
        else if (strcmp(opts.density, "hd") == 0)  state.forcedHD = 1;
        else {
            std::cerr << "waffle-fuse: unknown density '" << opts.density
                      << "' (use dd or hd)\n";
            return 1;
        }
    }

    state.forceRO = state.forceRO || opts.ro;
    state.debugHW = opts.debug_hw;

    // Choose operations based on forced format; for Auto we decide in init().
    // We pick FAT ops as default; affs ops are used when AFFS is forced.
    struct fuse_operations ops;
    if (state.fsType == WaffleState::FSType::AFFS) {
        ops = affs_get_operations();
    } else {
        ops = fat_get_operations();
    }

    // Override init/destroy to open the hardware first
    ops.init    = waffle_init;
    ops.destroy = waffle_destroy;

    // Ensure the mount is visible as "fuse.waffle" in /proc/mounts so that
    // desktop file managers (Nautilus, Thunar, Dolphin) can identify it.
    // When invoked via the shell-script wrapper the subtype is already set;
    // this is a safe no-op in that case because libfuse uses the last value.
    bool has_subtype = false;
    for (int i = 0; i < args.argc && !has_subtype; ++i) {
        if (args.argv[i] && strstr(args.argv[i], "subtype="))
            has_subtype = true;
    }
    if (!has_subtype)
        fuse_opt_add_arg(&args, "-osubtype=waffle");

    int ret = fuse_main(args.argc, args.argv, &ops, nullptr);

    fuse_opt_free_args(&args);
    return ret;
}
