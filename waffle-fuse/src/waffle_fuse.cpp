// waffle-fuse  –  FUSE driver for DrawBridge / Waffle floppy reader
//
// Usage:
//   waffle-fuse <serial-port> <mountpoint> [options]
//   waffle-fuse <serial-port> --automount <dir> [options]
//   waffle-fuse --image <file> <mountpoint> | --automount <dir> [options]
//   waffle-fuse --probe <serial-port>
//   waffle-fuse --wait-disk <serial-port> [timeout-seconds]
//   waffle-fuse --wait-removal <serial-port> [timeout-seconds]
//
// The disk is opened and its format detected *before* mounting, so a wrong
// or unreadable disk never leaves a dead mountpoint behind.  With --automount
// the disk is mounted at <dir>/<volume name>, like a Workbench disk icon.
//
// Besides the human readable messages on stderr, one line per event is
// printed on stdout for front-ends (see scripts/waffle-diskflashback.py):
//   WAFFLE event=detected format=amiga fs=FFS density=DD readonly=0 volume=Workbench
//   WAFFLE event=mounted mountpoint=/home/user/Floppy/Workbench
//   WAFFLE event=removed | removed-dirty | write-error | flushed
//   WAFFLE error=<port|nodisk|unknown-format|drive|filesystem|mount>

#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <fuse3/fuse_lowlevel.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <atomic>
#include <thread>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "disk_cache.h"
#include "fat_fs.h"
#include "affs_fs.h"
#include "format.h"
#include "debugmsg.h"

// Exit codes (also documented in the README)
enum ExitCode {
    EXIT_OK            = 0,
    EXIT_USAGE         = 1,
    EXIT_PORT          = 2,
    EXIT_NODISK        = 3,
    EXIT_UNKNOWN_FMT   = 4,
    EXIT_DRIVE         = 5,
    EXIT_FILESYSTEM    = 6,
    EXIT_REMOVED       = 7,
    EXIT_REMOVED_DIRTY = 8,
    EXIT_WRITE_ERROR   = 9,
};

// ── FUSE option handling ──────────────────────────────────────────────────────
struct WaffleOptions {
    char* format     = nullptr;   // "fat" or "affs"
    char* density    = nullptr;   // "dd" or "hd"
    int   ro         = 0;
    int   debug_hw   = 0;
    int   flushdelay = 1000;
    int   noverify   = 0;
    int   diskpoll   = 3000;
};

#define WAFFLE_OPT(t, p, v) { t, offsetof(WaffleOptions, p), v }

static const struct fuse_opt waffle_opts[] = {
    WAFFLE_OPT("format=%s",     format,     0),
    WAFFLE_OPT("density=%s",    density,    0),
    WAFFLE_OPT("ro",            ro,         1),
    WAFFLE_OPT("debug_hw",      debug_hw,   1),
    WAFFLE_OPT("flushdelay=%d", flushdelay, 0),
    WAFFLE_OPT("noverify",      noverify,   1),
    WAFFLE_OPT("diskpoll=%d",   diskpoll,   0),
    FUSE_OPT_END
};

// ── Small helpers ─────────────────────────────────────────────────────────────

static void emit(const std::string& line)
{
    std::cout << "WAFFLE " << line << std::endl;
}

// Run a command without a shell; returns its exit status (-1 on failure).
static int runCommand(const std::vector<std::string>& argv)
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        std::vector<char*> a;
        for (auto& s : argv) a.push_back(const_cast<char*>(s.c_str()));
        a.push_back(nullptr);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, 1); dup2(devnull, 2); }
        execvp(a[0], a.data());
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static bool isDirEmpty(const std::string& path)
{
    DIR* d = opendir(path.c_str());
    if (!d) return false;
    bool empty = true;
    while (struct dirent* e = readdir(d)) {
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) { empty = false; break; }
    }
    closedir(d);
    return empty;
}

static bool mkdirs(const std::string& path)
{
    std::string cur;
    for (size_t i = 0; i < path.size(); i++) {
        cur += path[i];
        if ((path[i] == '/' && i > 0) || i == path.size() - 1) {
            if (mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST) return false;
        }
    }
    return true;
}

// Directory name for a volume: no '/', no leading dot (GIO hides dot paths)
static std::string sanitizeName(const std::string& name, const std::string& fallback)
{
    std::string s;
    for (char c : name) s += (c == '/' || (unsigned char)c < 0x20) ? '_' : c;
    while (!s.empty() && (s[0] == '.' || s[0] == ' ')) s.erase(0, 1);
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s.empty() ? fallback : s;
}

// Pick (and create) <base>/<name>, or <name> (2), (3)...  Stale FUSE mounts
// left behind by a crash are cleaned up on the way.
static std::string makeAutomountDir(const std::string& base, const std::string& name, bool& created)
{
    created = false;
    if (!mkdirs(base)) return "";
    for (int n = 1; n < 100; n++) {
        std::string path = base + "/" + name + (n > 1 ? " (" + std::to_string(n) + ")" : "");
        struct stat st;
        if (stat(path.c_str(), &st) != 0) {
            if (errno == ENOTCONN) {
                runCommand({"fusermount3", "-u", "-z", path});
                if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && isDirEmpty(path)) return path;
                continue;
            }
            if (mkdir(path.c_str(), 0755) == 0) { created = true; return path; }
            continue;
        }
        if (!S_ISDIR(st.st_mode)) continue;
        // An empty directory that is not already a mountpoint can be reused
        struct stat parent;
        std::string up = path + "/..";
        if (stat(up.c_str(), &parent) == 0 && parent.st_dev != st.st_dev) continue;
        if (isDirEmpty(path)) return path;
    }
    return "";
}

static void usage(const char* progname)
{
    std::cerr <<
        "Usage: " << progname << " <serial-port> <mountpoint> [options]\n"
        "       " << progname << " <serial-port> --automount <dir> [options]\n"
        "       " << progname << " --image <file.adf|file.img> <mountpoint> [options]\n"
        "       " << progname << " --probe <serial-port>\n"
        "       " << progname << " --wait-disk <serial-port> [timeout-s]\n"
        "       " << progname << " --wait-removal <serial-port> [timeout-s]\n"
        "       " << progname << " --format <serial-port> <amiga|ofs|ffs|amiga-intl|dos> <dd|hd> [label] [--quick] [--yes]\n"
        "       " << progname << " --format-image <file> <amiga|ofs|ffs|amiga-intl|dos> <dd|hd> [label]\n"
        "       " << progname << " --scan <serial-port>        (good sectors per track)\n"
        "       " << progname << " --dump <serial-port> <file> (read the whole disk into an image)\n"
        "\n"
        "  --automount <dir>  mount at <dir>/<volume name> (created and removed automatically)\n"
        "  --probe            check if a disk is present (exit 0=disk, 1=no disk, 2=error)\n"
        "  --wait-disk        wait until a disk is inserted (exit 0, 1=timeout, 2=error);\n"
        "                     add a poll interval in ms after the timeout (default 3000)\n"
        "  --wait-removal     wait until the disk is taken out\n"
        "\n"
        "Options (-o opt[,opt...]):\n"
        "  format=fat|affs    force filesystem type (default: autodetect)\n"
        "  density=dd|hd      force disk density (default: autodetect)\n"
        "  ro                 mount read-only\n"
        "  flushdelay=MS      write changes back MS milliseconds after the last write (default 1000)\n"
        "  noverify           do not read back written tracks\n"
        "  diskpoll=MS        check for disk removal every MS milliseconds (default 3000)\n"
        "  debug_hw           hardware/serial debug messages\n"
        "  -f                 stay in the foreground\n"
        "\n"
        "Examples:\n"
        "  " << progname << " /dev/ttyUSB0 /mnt/floppy\n"
        "  " << progname << " /dev/ttyUSB0 --automount ~/Floppy -f\n"
        "  " << progname << " --image workbench.adf /tmp/wb\n";
}

// ── Mount state shared with the disk event callback ───────────────────────────

static struct fuse*        g_fuse = nullptr;
static std::string         g_mountpoint;
static std::atomic<int>    g_exitCode{EXIT_OK};

// Called from the disk background thread
static void onDiskEvent(WaffleDisk::Event e)
{
    switch (e) {
    case WaffleDisk::Event::WriteError:
        std::cerr << "waffle-fuse: ERROR writing to the disk (changes kept in memory, will retry on unmount)\n";
        emit("event=write-error");
        return;
    case WaffleDisk::Event::DiskRemoved:
        std::cerr << "waffle-fuse: disk removed, unmounting\n";
        emit("event=removed");
        g_exitCode = EXIT_REMOVED;
        break;
    case WaffleDisk::Event::DiskRemovedDirty:
        std::cerr << "waffle-fuse: disk removed while changes were pending: THEY ARE LOST\n";
        emit("event=removed-dirty");
        g_exitCode = EXIT_REMOVED_DIRTY;
        break;
    }
    // Leave the FUSE loop: set the exit flag, then generate one request so the
    // loop (blocked reading /dev/fuse) wakes up and notices it.
    if (g_fuse) fuse_exit(g_fuse);
    std::string mp = g_mountpoint;
    std::thread([mp] { struct stat st; stat(mp.c_str(), &st); }).detach();
}

// ── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    if (argc >= 3 && strcmp(argv[1], "--probe") == 0) {
        int r = WaffleDisk::probe(argv[2]);
        std::cout << (r == 0 ? "disk" : r == 1 ? "nodisk" : "error") << "\n";
        return r;
    }
    if (argc >= 3 && (strcmp(argv[1], "--wait-disk") == 0 || strcmp(argv[1], "--wait-removal") == 0)) {
        int timeout = argc >= 4 ? atoi(argv[3]) : 0;
        int pollMs  = argc >= 5 ? atoi(argv[4]) : 3000;
        return WaffleDisk::waitForDisk(argv[2], strcmp(argv[1], "--wait-disk") == 0, timeout, pollMs);
    }

    // --dump <port> <file>: read the whole disk into an ADF/IMG, listing unreadable sectors
    if (argc >= 4 && strcmp(argv[1], "--dump") == 0) {
        WaffleDisk::Options wo;
        wo.port = argv[2];
        wo.forceReadOnly = true;
        WaffleDisk wd(wo);
        if (wd.open() != WaffleDisk::OpenResult::OK) { std::cerr << "waffle-fuse: cannot read the disk\n"; return EXIT_DRIVE; }
        FILE* out = fopen(argv[3], "wb");
        if (!out) { perror(argv[3]); return EXIT_USAGE; }
        const DiskGeometry& g = wd.geometry();
        std::cerr << "waffle-fuse: " << diskFormatName(g.format) << (g.isHD ? " HD" : " DD")
                  << ", " << wd.totalSectors() << " sectors\n";
        uint32_t bad = 0;
        for (uint32_t lba = 0; lba < wd.totalSectors(); lba++) {
            uint8_t buf[512];
            if (!wd.readSector(lba, buf)) {
                const uint32_t spt = g.sectorsPerTrack;
                std::cerr << "  unreadable sector " << lba << " (cylinder " << lba / (spt * g.numHeads)
                          << ", head " << (lba / spt) % g.numHeads << ", sector " << lba % spt << ")\n";
                bad++;
            }
            fwrite(buf, 1, 512, out);
        }
        fclose(out);
        std::cerr << "waffle-fuse: " << bad << " unreadable sectors (written as zeros)\n";
        return bad ? EXIT_DRIVE : EXIT_OK;
    }

    // --format <port> / --format-image <file>  <amiga|ofs|ffs|...|dos> <dd|hd> [label] [--quick] [--yes] [--force]
    if (argc >= 5 && (strcmp(argv[1], "--format") == 0 || strcmp(argv[1], "--format-image") == 0)) {
        const bool toImage = strcmp(argv[1], "--format-image") == 0;
        FormatSpec spec;
        if (!parseFormatKind(argv[3], spec)) {
            std::cerr << "waffle-fuse: unknown format '" << argv[3] << "' (amiga, ofs, ffs, amiga-intl, ofs-intl, dos)\n";
            return EXIT_USAGE;
        }
        if (strcmp(argv[4], "dd") == 0) spec.hd = false;
        else if (strcmp(argv[4], "hd") == 0) spec.hd = true;
        else { std::cerr << "waffle-fuse: density must be dd or hd\n"; return EXIT_USAGE; }
        bool quick = false, yes = false, force = false;
        for (int i = 5; i < argc; i++) {
            if (strcmp(argv[i], "--quick") == 0) quick = true;
            else if (strcmp(argv[i], "--yes") == 0) yes = true;
            else if (strcmp(argv[i], "--force") == 0) force = true;
            else spec.label = argv[i];
        }

        std::vector<uint8_t> image;
        std::string err = buildFormatImage(spec, image);
        if (!err.empty()) { std::cerr << "waffle-fuse: " << err << "\n"; return EXIT_USAGE; }
        const char* what = spec.kind == FormatSpec::Kind::FAT12 ? "DOS FAT12"
                         : spec.kind == FormatSpec::Kind::AmigaFFS ? "Amiga FFS" : "Amiga OFS";

        if (toImage) {
            FILE* f = fopen(argv[2], "wb");
            if (!f || fwrite(image.data(), 1, image.size(), f) != image.size()) { perror(argv[2]); if (f) fclose(f); return EXIT_USAGE; }
            fclose(f);
            std::cerr << "waffle-fuse: wrote " << what << (spec.hd ? " HD" : " DD") << " image " << argv[2] << "\n";
            return EXIT_OK;
        }

        WaffleDisk::Options wo;
        wo.port = argv[2];
        WaffleDisk wd(wo);
        bool holeHD, holeKnown;
        switch (wd.openForFormat(holeHD, holeKnown)) {
        case WaffleDisk::OpenResult::OK: break;
        case WaffleDisk::OpenResult::NoDisk: std::cerr << "waffle-fuse: no disk in drive\n"; emit("error=nodisk"); return EXIT_NODISK;
        default: std::cerr << "waffle-fuse: cannot open device " << argv[2] << "\n"; emit("error=port"); return EXIT_PORT;
        }
        if (wd.isWriteProtected()) {
            std::cerr << "waffle-fuse: the disk is write-protected\n";
            emit("error=write-protected");
            return EXIT_WRITE_ERROR;
        }
        if (holeKnown && holeHD != spec.hd && !force) {
            std::cerr << "waffle-fuse: this is a" << (holeHD ? "n HD" : " DD") << " disk, it cannot be reliably formatted as "
                      << (spec.hd ? "HD" : "DD") << " (use --force to try anyway)\n";
            emit("error=density");
            return EXIT_USAGE;
        }
        if (!yes) {
            if (!isatty(0)) { std::cerr << "waffle-fuse: refusing to format without --yes\n"; return EXIT_USAGE; }
            std::cerr << "All data on the disk in " << argv[2] << " will be ERASED.\n"
                      << "Format as " << what << (spec.hd ? " HD" : " DD") << (quick ? " (quick)" : "")
                      << (spec.label.empty() ? "" : ", volume \"" + spec.label + "\"") << "? Type YES: " << std::flush;
            std::string answer;
            std::getline(std::cin, answer);
            if (answer != "YES") { std::cerr << "waffle-fuse: aborted\n"; return EXIT_USAGE; }
        }

        std::cerr << "waffle-fuse: formatting as " << what << (spec.hd ? " HD" : " DD") << (quick ? " (quick)" : "") << "...\n";
        int bad = wd.formatDisk(formatGeometry(spec), image, quick, [](uint32_t cyl, uint32_t head, bool ok) {
            std::cerr << "\r  cylinder " << (cyl < 10 ? "0" : "") << cyl << " head " << head << (ok ? "   " : " FAILED\n") << std::flush;
            emit("event=format-progress cylinder=" + std::to_string(cyl) + " head=" + std::to_string(head) + " ok=" + (ok ? "1" : "0"));
        });
        std::cerr << "\n";
        wd.close();
        if (bad < 0) { std::cerr << "waffle-fuse: the disk is write-protected\n"; emit("error=write-protected"); return EXIT_WRITE_ERROR; }
        if (bad > 0) {
            std::cerr << "waffle-fuse: " << bad << " tracks could not be written: the disk is damaged\n";
            emit("error=format-failed tracks=" + std::to_string(bad));
            return EXIT_WRITE_ERROR;
        }
        std::cerr << "waffle-fuse: format complete, every track verified\n";
        emit("event=formatted");
        return EXIT_OK;
    }

    // --scan <port>: surface map, good sectors per track
    if (argc >= 3 && strcmp(argv[1], "--scan") == 0) {
        WaffleDisk::Options wo;
        wo.port = argv[2];
        wo.forceReadOnly = true;
        WaffleDisk wd(wo);
        if (wd.open() != WaffleDisk::OpenResult::OK) { std::cerr << "waffle-fuse: cannot read the disk\n"; return EXIT_DRIVE; }
        const DiskGeometry& g = wd.geometry();
        std::cout << diskFormatName(g.format) << (g.isHD ? " HD" : " DD") << ": good sectors per track (of "
                  << g.sectorsPerTrack << "), head 0 / head 1\n";
        int badTracks = 0;
        for (uint32_t c = 0; c < g.numCylinders; c++) {
            char line[64];
            int h0 = wd.scanTrack(c, 0), h1 = wd.scanTrack(c, 1);
            badTracks += (h0 != (int)g.sectorsPerTrack) + (h1 != (int)g.sectorsPerTrack);
            snprintf(line, sizeof line, "cyl %02u: %2d %2d%s", c, h0, h1,
                     (h0 == (int)g.sectorsPerTrack && h1 == (int)g.sectorsPerTrack) ? "" : "   <--");
            std::cout << line << std::endl;
        }
        std::cout << badTracks << " incomplete tracks\n";
        return badTracks ? EXIT_DRIVE : EXIT_OK;
    }

    // ── Positional arguments ──────────────────────────────────────────────────
    std::string source, imagePath, automountBase;
    std::vector<char*> fuse_argv{argv[0]};
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0 && i + 1 < argc)          imagePath = argv[++i];
        else if (strcmp(argv[i], "--automount") == 0 && i + 1 < argc) automountBase = argv[++i];
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) { usage(argv[0]); return EXIT_OK; }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) { fuse_argv.push_back(argv[i]); fuse_argv.push_back(argv[++i]); }
        else if (source.empty() && imagePath.empty() && argv[i][0] != '-') source = argv[i];
        else fuse_argv.push_back(argv[i]);
    }
    if (source.empty() && imagePath.empty()) { usage(argv[0]); return EXIT_USAGE; }

    struct fuse_args args = FUSE_ARGS_INIT((int)fuse_argv.size(), fuse_argv.data());
    WaffleOptions opts{};
    opts.flushdelay = 1000;
    opts.diskpoll   = 3000;
    if (fuse_opt_parse(&args, &opts, waffle_opts, nullptr) == -1) return EXIT_USAGE;

    struct fuse_cmdline_opts cmd{};
    if (fuse_parse_cmdline(&args, &cmd) != 0) return EXIT_USAGE;
    if (cmd.show_help)    { usage(argv[0]); return EXIT_OK; }
    if (cmd.show_version) { fuse_lowlevel_version(); return EXIT_OK; }
    if (!cmd.mountpoint && automountBase.empty()) { usage(argv[0]); return EXIT_USAGE; }

    enum class FSType { Auto, FAT, AFFS } fsType = FSType::Auto;
    if (opts.format) {
        if (strcmp(opts.format, "fat") == 0)       fsType = FSType::FAT;
        else if (strcmp(opts.format, "affs") == 0) fsType = FSType::AFFS;
        else { std::cerr << "waffle-fuse: unknown format '" << opts.format << "' (use fat or affs)\n"; return EXIT_USAGE; }
    }
    int forcedHD = -1;
    if (opts.density) {
        if (strcmp(opts.density, "dd") == 0)      forcedHD = 0;
        else if (strcmp(opts.density, "hd") == 0) forcedHD = 1;
        else { std::cerr << "waffle-fuse: unknown density '" << opts.density << "' (use dd or hd)\n"; return EXIT_USAGE; }
    }

    DebugMsg::init(opts.debug_hw);
    DebugMsg::initVerbose(opts.debug_hw);

    // ── Open the disk and detect its format ──────────────────────────────────
    std::unique_ptr<IDiskImage> disk;
    WaffleDisk* waffle = nullptr;
    if (!imagePath.empty()) {
        auto img = std::make_unique<ImageDisk>(imagePath, opts.ro);
        if (!img->open()) {
            std::cerr << "waffle-fuse: cannot open image " << imagePath << "\n";
            emit("error=port");
            return EXIT_PORT;
        }
        disk = std::move(img);
        source = imagePath;
    } else {
        WaffleDisk::Options wo;
        wo.port          = source;
        wo.forceReadOnly = opts.ro;
        wo.forcedHD      = forcedHD;
        wo.flushDelayMs  = std::max(100, opts.flushdelay);
        wo.verifyWrites  = !opts.noverify;
        wo.diskPollMs    = opts.diskpoll;
        auto wd = std::make_unique<WaffleDisk>(wo);
        std::cerr << "waffle-fuse: reading disk on " << source << "...\n";
        switch (wd->open()) {
        case WaffleDisk::OpenResult::OK: break;
        case WaffleDisk::OpenResult::PortError:
            std::cerr << "waffle-fuse: cannot open device " << source << "\n";
            emit("error=port");
            return EXIT_PORT;
        case WaffleDisk::OpenResult::NoDisk:
            std::cerr << "waffle-fuse: no disk in drive\n";
            emit("error=nodisk");
            return EXIT_NODISK;
        case WaffleDisk::OpenResult::UnknownFormat:
            std::cerr << "waffle-fuse: not a DOS disk (neither AmigaDOS nor FAT): unformatted, NDOS or copy-protected\n";
            emit("error=unknown-format");
            return EXIT_UNKNOWN_FMT;
        case WaffleDisk::OpenResult::DriveError:
            std::cerr << "waffle-fuse: drive error while reading track 0\n";
            emit("error=drive");
            return EXIT_DRIVE;
        }
        waffle = wd.get();
        disk = std::move(wd);
    }

    const DiskGeometry& geo = disk->geometry();
    std::cerr << "waffle-fuse: disk opened  format=" << diskFormatName(geo.format)
              << "  HD=" << (geo.isHD ? "yes" : "no")
              << "  sectors=" << disk->totalSectors() << "\n";

    // ── Filesystem ────────────────────────────────────────────────────────────
    bool useAFFS = fsType == FSType::AFFS || (fsType == FSType::Auto && geo.format == DiskFormat::Amiga_ADF);
    void* fsCtx = useAFFS ? affs_create(disk.get()) : fat_fs_new(disk.get());
    if (!fsCtx) {
        std::cerr << "waffle-fuse: the disk does not contain a valid "
                  << (useAFFS ? "AmigaDOS" : "FAT") << " filesystem\n";
        emit("error=filesystem");
        return EXIT_FILESYSTEM;
    }

    std::string fsName, volume;
    bool readOnly;
    if (useAFFS) {
        fsName   = affs_is_ffs(fsCtx) ? "FFS" : "OFS";
        volume   = affs_volume_name(fsCtx);
        readOnly = affs_is_readonly(fsCtx);
    } else {
        fsName   = fat_is_fat16(fsCtx) ? "FAT16" : "FAT12";
        volume   = fat_volume_label(fsCtx);
        readOnly = disk->isWriteProtected();
    }
    std::cerr << "waffle-fuse: " << (useAFFS ? "Amiga " : "") << fsName
              << (useAFFS ? " (" + affs_dostype(fsCtx) + ")" : "")
              << (volume.empty() ? "" : " volume \"" + volume + "\"")
              << (readOnly ? " (read-only)" : " (read-write)") << "\n";
    if (useAFFS && !affs_readonly_reason(fsCtx).empty())
        std::cerr << "waffle-fuse: read-only because the " << affs_readonly_reason(fsCtx)
                  << " (validate the disk on an Amiga to make it writable)\n";
    emit(std::string("event=detected format=") + (useAFFS ? "amiga" : "fat") +
         " fs=" + fsName + " density=" + (geo.isHD ? "HD" : "DD") +
         " readonly=" + (readOnly ? "1" : "0") + " volume=" + volume);

    auto freeFs = [&] {
        if (useAFFS) affs_destroy(fsCtx); else fat_fs_free(fsCtx);
        fsCtx = nullptr;
    };

    // ── Mountpoint ────────────────────────────────────────────────────────────
    bool createdDir = false;
    std::string mountpoint;
    if (!automountBase.empty()) {
        std::string fallback = useAFFS ? "Amiga Floppy" : "DOS Floppy";
        mountpoint = makeAutomountDir(automountBase, sanitizeName(volume, fallback), createdDir);
        if (mountpoint.empty()) {
            std::cerr << "waffle-fuse: cannot create a mountpoint under " << automountBase << "\n";
            emit("error=mount");
            freeFs();
            return EXIT_USAGE;
        }
    } else {
        mountpoint = cmd.mountpoint;
    }
    g_mountpoint = mountpoint;

    // Name shown by `mount`/`df`; "ro" goes to the kernel when the medium is protected
    std::string fsopts = "-ofsname=waffle:" + source + ",subtype=" + (useAFFS ? "affs" : "vfat");
    fuse_opt_add_arg(&args, fsopts.c_str());
    if (readOnly) fuse_opt_add_arg(&args, "-oro");

    struct fuse_operations ops = useAFFS ? affs_get_operations() : fat_get_operations();
    g_fuse = fuse_new(&args, &ops, sizeof(ops), fsCtx);
    if (!g_fuse) {
        freeFs();
        if (createdDir) rmdir(mountpoint.c_str());
        emit("error=mount");
        return EXIT_USAGE;
    }
    if (fuse_mount(g_fuse, mountpoint.c_str()) != 0) {
        std::cerr << "waffle-fuse: cannot mount on " << mountpoint << "\n";
        fuse_destroy(g_fuse);
        freeFs();
        if (createdDir) rmdir(mountpoint.c_str());
        emit("error=mount");
        return EXIT_USAGE;
    }
    struct fuse_session* se = fuse_get_session(g_fuse);
    fuse_set_signal_handlers(se);

    std::cerr << "waffle-fuse: mounted on " << mountpoint << "\n";
    emit("event=mounted mountpoint=" + mountpoint);
    fuse_daemonize(cmd.foreground);

    // Threads must be started after fuse_daemonize() has forked
    if (waffle) waffle->startBackground(onDiskEvent);

    // The filesystem code is not thread-safe: use the single-threaded loop
    fuse_loop(g_fuse);

    // ── Teardown: unmount, write pending changes, release the drive ───────────
    fuse_remove_signal_handlers(se);
    fuse_unmount(g_fuse);
    if (waffle) waffle->stopBackground();

    freeFs();     // commits buffered AFFS files, writes the FAT
    bool flushed = disk->flush();
    if (waffle) waffle->close();
    fuse_destroy(g_fuse);
    g_fuse = nullptr;
    if (createdDir) rmdir(mountpoint.c_str());

    int code = g_exitCode;
    if (!flushed && code == EXIT_OK) {
        std::cerr << "waffle-fuse: ERROR: some changes could not be written to the disk\n";
        emit("event=write-error");
        code = EXIT_WRITE_ERROR;
    } else if (code == EXIT_OK) {
        std::cerr << "waffle-fuse: all changes written, the disk can be removed\n";
        emit("event=flushed");
    }

    fuse_opt_free_args(&args);
    free(cmd.mountpoint);
    return code;
}
