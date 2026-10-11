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
#include <csignal>
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
#include "icons_embedded.h"
#include "debugmsg.h"

// Exit codes (also documented in the README)
enum ExitCode
{
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
struct WaffleOptions
{
    char* format     = nullptr;   // "fat" or "affs"
    char* density    = nullptr;   // "dd" or "hd"
    int   ro         = 0;
    int   debug_hw   = 0;
    int   flushdelay = 1000;
    int   noverify   = 0;
    int   diskpoll   = 3000;
    int   trash      = 0;   // allow .Trash-* directories (file manager trash)
    int   novolinfo  = 0;   // no virtual /.xdg-volume-info
};

#define WAFFLE_OPT(t, p, v) { t, offsetof(WaffleOptions, p), v }

static const struct fuse_opt waffle_opts[] =
{
    WAFFLE_OPT("format=%s",     format,     0),
    WAFFLE_OPT("density=%s",    density,    0),
    WAFFLE_OPT("ro",            ro,         1),
    WAFFLE_OPT("debug_hw",      debug_hw,   1),
    WAFFLE_OPT("flushdelay=%d", flushdelay, 0),
    WAFFLE_OPT("noverify",      noverify,   1),
    WAFFLE_OPT("diskpoll=%d",   diskpoll,   0),
    WAFFLE_OPT("trash",         trash,      1),
    WAFFLE_OPT("novolumeinfo",  novolinfo,  1),
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
    if (pid == 0)
    {
        std::vector<char*> a;
        for (auto& s : argv) a.push_back(const_cast<char*>(s.c_str()));
        a.push_back(nullptr);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0)
        {
            dup2(devnull, 1);
            dup2(devnull, 2);
        }
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
    while (struct dirent* e = readdir(d))
    {
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
        {
            empty = false;
            break;
        }
    }
    closedir(d);
    return empty;
}

static bool mkdirs(const std::string& path)
{
    std::string cur;
    for (size_t i = 0; i < path.size(); i++)
    {
        cur += path[i];
        if ((path[i] == '/' && i > 0) || i == path.size() - 1)
        {
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
    for (int n = 1; n < 100; n++)
    {
        std::string path = base + "/" + name + (n > 1 ? " (" + std::to_string(n) + ")" : "");
        struct stat st;
        if (stat(path.c_str(), &st) != 0)
        {
            if (errno == ENOTCONN)
            {
                runCommand({"fusermount3", "-u", "-z", path});
                if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && isDirEmpty(path)) return path;
                continue;
            }
            if (mkdir(path.c_str(), 0755) == 0)
            {
                created = true;
                return path;
            }
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
              "       " << progname << " --write-image <serial-port> <file.adf|img> [--full] [--yes]\n"
              "       " << progname << " --scan <serial-port>        (good sectors per track)\n"
              "       " << progname << " --dump <serial-port> <file.adf|img|scp> [--format amiga|dos] [--density dd|hd]\n"
              "                     (copy the whole disk; .scp = raw flux, for copy-protected disks)\n"
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
              "  trash              let file managers create a .Trash-<uid> folder on the disk\n"
              "  novolumeinfo       no virtual .xdg-volume-info (desktop name and icon)\n"
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
    switch (e)
    {
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

// ── Desktop layer in front of the filesystem ─────────────────────────────────
// * a virtual, read-only /.xdg-volume-info gives file managers the volume name
//   and an Amiga/DOS icon; it is never written to the disk;
// * .Trash / .Trash-<uid> folders are refused, so file managers offer to delete
//   permanently instead of filling an 880K floppy with a trash can.

static struct fuse_operations g_fsOps;          // the filesystem's own operations
static bool                   g_allowTrash = false;
static const uint64_t         VIRTUAL_FH = ~0ull;  // fh of a virtual file (content via path)

// Virtual read-only files: path -> content ("" = absent).  The icon is given
// as IconFile (a file, used as is) as well as Icon (a themed name): with a
// themed name GTK prefers the theme's generic "drive-removable-media"
// fallback over an icon that only exists in hicolor.
static std::string g_volumeInfo;   // /.xdg-volume-info
static std::string g_volumeIcon;   // /.waffle-icon.svg

static const std::string* virtualFile(const char* path)
{
    if (!g_volumeInfo.empty() && strcmp(path, "/.xdg-volume-info") == 0) return &g_volumeInfo;
    if (!g_volumeIcon.empty() && strcmp(path, "/.waffle-icon.svg") == 0)  return &g_volumeIcon;
    return nullptr;
}

static bool isVolumeInfo(const char* path)
{
    return virtualFile(path) != nullptr;
}

static bool isTrash(const char* path)
{
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    return !g_allowTrash && (strcmp(base, ".Trash") == 0 || strncmp(base, ".Trash-", 7) == 0);
}

static int w_getattr(const char* path, struct stat* st, struct fuse_file_info* fi)
{
    if (const std::string* vf = virtualFile(path))
    {
        memset(st, 0, sizeof(*st));
        st->st_mode  = S_IFREG | 0444;
        st->st_nlink = 1;
        st->st_size  = (off_t)vf->size();
        st->st_uid   = getuid();
        st->st_gid   = getgid();
        return 0;
    }
    if (isTrash(path)) return -ENOENT;   // hide one left by an older driver, too
    return g_fsOps.getattr(path, st, fi);
}

static int w_open(const char* path, struct fuse_file_info* fi)
{
    if (isVolumeInfo(path))
    {
        if ((fi->flags & O_ACCMODE) != O_RDONLY) return -EACCES;
        fi->fh = VIRTUAL_FH;
        return 0;
    }
    return g_fsOps.open(path, fi);
}

static int w_read(const char* path, char* buf, size_t size, off_t off, struct fuse_file_info* fi)
{
    if (fi && fi->fh == VIRTUAL_FH)
    {
        const std::string* vf = virtualFile(path);
        if (!vf || off >= (off_t)vf->size()) return 0;
        size_t n = std::min(size, vf->size() - (size_t)off);
        memcpy(buf, vf->data() + off, n);
        return (int)n;
    }
    return g_fsOps.read(path, buf, size, off, fi);
}

static int w_flush(const char* path, struct fuse_file_info* fi)
{
    if (fi && fi->fh == VIRTUAL_FH) return 0;
    return g_fsOps.flush ? g_fsOps.flush(path, fi) : 0;
}

static int w_release(const char* path, struct fuse_file_info* fi)
{
    if (fi && fi->fh == VIRTUAL_FH) return 0;
    return g_fsOps.release ? g_fsOps.release(path, fi) : 0;
}

static int w_mkdir(const char* path, mode_t mode)
{
    if (isTrash(path) || isVolumeInfo(path)) return -EPERM;
    return g_fsOps.mkdir(path, mode);
}

static int w_create(const char* path, mode_t mode, struct fuse_file_info* fi)
{
    if (isTrash(path) || isVolumeInfo(path)) return -EPERM;
    return g_fsOps.create(path, mode, fi);
}

static int w_rename(const char* from, const char* to, unsigned int flags)
{
    if (isTrash(to) || isVolumeInfo(to) || isVolumeInfo(from)) return -EPERM;
    return g_fsOps.rename(from, to, flags);
}

static struct fuse_operations wrapOperations(const struct fuse_operations& fs)
{
    g_fsOps = fs;
    struct fuse_operations ops = fs;
    ops.getattr = w_getattr;
    ops.open    = w_open;
    ops.read    = w_read;
    ops.flush   = w_flush;
    ops.release = w_release;
    ops.mkdir   = w_mkdir;
    ops.create  = w_create;
    ops.rename  = w_rename;
    return ops;
}

// ── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    if (argc >= 3 && strcmp(argv[1], "--probe") == 0)
    {
        int r = WaffleDisk::probe(argv[2]);
        std::cout << (r == 0 ? "disk" : r == 1 ? "nodisk" : "error") << "\n";
        return r;
    }
    if (argc >= 3 && (strcmp(argv[1], "--wait-disk") == 0 || strcmp(argv[1], "--wait-removal") == 0))
    {
        int timeout = argc >= 4 ? atoi(argv[3]) : 0;
        int pollMs  = argc >= 5 ? atoi(argv[4]) : 3000;
        return WaffleDisk::waitForDisk(argv[2], strcmp(argv[1], "--wait-disk") == 0, timeout, pollMs);
    }

    // Long disk operations stop between two tracks on SIGTERM/SIGINT, so the
    // drive is left in a clean state (never in the middle of a track write)
    static std::atomic<bool> stopRequested{false};
    if (argc >= 2 && (strcmp(argv[1], "--write-image") == 0 || strcmp(argv[1], "--format") == 0 ||
                      strcmp(argv[1], "--dump") == 0))
    {
        signal(SIGTERM, [](int)
        {
            stopRequested = true;
        });
        signal(SIGINT,  [](int)
        {
            stopRequested = true;
        });
    }

    // --dump <port> <file> [--format amiga|dos] [--density dd|hd] [--tracks N] [--revs N]
    // Copy the whole disk into an image: ADF/IMG (sectors) or, for a .scp file,
    // the raw magnetic flux (copy-protected and non-DOS disks).
    if (argc >= 4 && strcmp(argv[1], "--dump") == 0)
    {
        const std::string port = argv[2], out = argv[3];
        std::string forceFmt, forceDen;
        unsigned int tracks = 80, revs = 3;
        for (int i = 4; i + 1 < argc; i += 2)
        {
            if (strcmp(argv[i], "--format") == 0)       forceFmt = argv[i + 1];
            else if (strcmp(argv[i], "--density") == 0) forceDen = argv[i + 1];
            else if (strcmp(argv[i], "--tracks") == 0)  tracks = (unsigned)atoi(argv[i + 1]);
            else if (strcmp(argv[i], "--revs") == 0)    revs = (unsigned)atoi(argv[i + 1]);
        }
        const bool scp = out.size() > 4 && strcasecmp(out.c_str() + out.size() - 4, ".scp") == 0;

        WaffleDisk::Options wo;
        wo.port = port;
        wo.forceReadOnly = true;
        if (!forceDen.empty()) wo.forcedHD = forceDen == "hd" ? 1 : 0;
        WaffleDisk wd(wo);

        auto openRaw = [&](bool& isHD) -> int
        {
            bool holeHD, holeKnown;
            switch (wd.openForFormat(holeHD, holeKnown))
            {
            case WaffleDisk::OpenResult::OK:
                break;
            case WaffleDisk::OpenResult::NoDisk:
                std::cerr << "waffle-fuse: no disk in drive\n";
                emit("error=nodisk");
                return EXIT_NODISK;
            default:
                std::cerr << "waffle-fuse: cannot open device " << port << "\n";
                emit("error=port");
                return EXIT_PORT;
            }
            isHD = !forceDen.empty() ? forceDen == "hd" : (holeKnown && holeHD);
            return 0;
        };

        if (scp)
        {
            bool isHD;
            if (int e = openRaw(isHD)) return e;
            std::cerr << "waffle-fuse: flux dump (" << (isHD ? "HD" : "DD") << ", " << tracks
                      << " cylinders, " << revs << " revolutions) to " << out << "\n";
            int r = wd.dumpSCP(out, isHD, std::min(tracks, 84u), std::max(1u, std::min(revs, 5u)),
                               [](int cyl, int head)
            {
                std::cerr << "\r  cylinder " << (cyl < 10 ? "0" : "") << cyl << " head " << head << std::flush;
                emit("event=track cylinder=" + std::to_string(cyl) + " head=" + std::to_string(head) + " status=read");
                return !stopRequested;
            });
            std::cerr << "\n";
            wd.close();
            if (r == -2)
            {
                std::cerr << "waffle-fuse: stopped\n";
                emit("error=stopped");
                return EXIT_USAGE;
            }
            if (r < 0)
            {
                std::cerr << "waffle-fuse: flux dump failed\n";
                emit("error=dump-failed");
                return EXIT_DRIVE;
            }
            std::cerr << "waffle-fuse: flux dump complete" << (r ? " (some tracks had read problems)" : "") << "\n";
            emit(std::string("event=dumped bad=") + (r ? "1" : "0"));
            return r ? EXIT_DRIVE : EXIT_OK;
        }

        // Sector dump: detected format, or forced, or Amiga layout for non-DOS disks
        DiskGeometry geo;
        WaffleDisk::OpenResult opened = forceFmt.empty() ? wd.open() : WaffleDisk::OpenResult::UnknownFormat;
        if (opened == WaffleDisk::OpenResult::OK)
        {
            geo = wd.geometry();
        }
        else if (opened == WaffleDisk::OpenResult::UnknownFormat || opened == WaffleDisk::OpenResult::DriveError)
        {
            bool isHD;
            if (int e = openRaw(isHD)) return e;
            FormatSpec spec;
            spec.kind = forceFmt == "dos" ? FormatSpec::Kind::FAT12 : FormatSpec::Kind::AmigaFFS;
            spec.hd = isHD;
            geo = formatGeometry(spec);
            if (forceFmt.empty())
                std::cerr << "waffle-fuse: not a DOS disk, copying it sector by sector as an Amiga "
                          << (isHD ? "HD" : "DD") << " disk (use a .scp file for copy-protected disks)\n";
        }
        else if (opened == WaffleDisk::OpenResult::NoDisk)
        {
            std::cerr << "waffle-fuse: no disk in drive\n";
            emit("error=nodisk");
            return EXIT_NODISK;
        }
        else
        {
            std::cerr << "waffle-fuse: cannot open device " << port << "\n";
            emit("error=port");
            return EXIT_PORT;
        }

        std::cerr << "waffle-fuse: " << diskFormatName(geo.format) << (geo.isHD ? " HD" : " DD") << ", "
                  << geo.numCylinders * geo.numHeads * geo.sectorsPerTrack << " sectors\n";
        std::vector<uint8_t> image;
        const uint32_t spt = geo.sectorsPerTrack;
        int bad = wd.readImage(geo, image, [spt](uint32_t cyl, uint32_t head, int good)
        {
            std::cerr << "\r  cylinder " << (cyl < 10 ? "0" : "") << cyl << " head " << head
                      << (good == (int)spt ? "            " : "  bad sectors: " + std::to_string(spt - good) + "\n")
                      << std::flush;
            emit("event=track cylinder=" + std::to_string(cyl) + " head=" + std::to_string(head) +
                 " status=" + (good == (int)spt ? "ok" : "bad"));
            return !stopRequested;
        });
        std::cerr << "\n";
        wd.close();
        if (bad == -2)
        {
            std::cerr << "waffle-fuse: stopped\n";
            emit("error=stopped");
            return EXIT_USAGE;
        }
        if (bad < 0)
        {
            std::cerr << "waffle-fuse: drive error\n";
            emit("error=drive");
            return EXIT_DRIVE;
        }
        FILE* f = fopen(out.c_str(), "wb");
        if (!f || fwrite(image.data(), 1, image.size(), f) != image.size())
        {
            perror(out.c_str());
            if (f) fclose(f);
            emit("error=file");
            return EXIT_USAGE;
        }
        fclose(f);
        std::cerr << "waffle-fuse: " << bad << " unreadable sectors (written as zeros)\n";
        emit("event=dumped bad=" + std::to_string(bad));
        return bad ? EXIT_DRIVE : EXIT_OK;
    }

    // Progress line shared by --format and --write-image
    auto trackProgress = [](uint32_t cyl, uint32_t head, int status) -> bool
    {
        std::cerr << "\r  cylinder " << (cyl < 10 ? "0" : "") << cyl << " head " << head
                  << (status == 0 ? " written   " : status == 1 ? " unchanged " : " FAILED\n") << std::flush;
        emit("event=track cylinder=" + std::to_string(cyl) + " head=" + std::to_string(head) +
             " status=" + (status == 0 ? "written" : status == 1 ? "unchanged" : "failed"));
        return !stopRequested;
    };

    // --write-image <port> <file.adf|img> [--full] [--yes] [--force]: put an image on the disk
    if (argc >= 4 && strcmp(argv[1], "--write-image") == 0)
    {
        bool full = false, yes = false, force = false;
        for (int i = 4; i < argc; i++)
        {
            if (strcmp(argv[i], "--full") == 0) full = true;
            else if (strcmp(argv[i], "--yes") == 0) yes = true;
            else if (strcmp(argv[i], "--force") == 0) force = true;
        }
        ImageDisk img(argv[3], true);
        if (!img.open())
        {
            std::cerr << "waffle-fuse: cannot read " << argv[3] << "\n";
            return EXIT_USAGE;
        }
        DiskGeometry geo = img.geometry();
        const uint32_t sectors = img.totalSectors();
        const bool known = (geo.format == DiskFormat::Amiga_ADF && (sectors == 1760 || sectors == 3520)) ||
                           (geo.format == DiskFormat::IBM_FAT && (sectors == 1440 || sectors == 2880));
        if (!known)
        {
            std::cerr << "waffle-fuse: " << argv[3] << " is not an 880K/1.76M ADF or a 720K/1.44M PC image\n";
            return EXIT_USAGE;
        }
        geo.numCylinders = sectors / (geo.numHeads * geo.sectorsPerTrack);
        std::vector<uint8_t> image((size_t)sectors * 512);
        for (uint32_t i = 0; i < sectors; i++) img.readSector(i, image.data() + (size_t)i * 512);

        WaffleDisk::Options wo;
        wo.port = argv[2];
        WaffleDisk wd(wo);
        bool holeHD, holeKnown;
        switch (wd.openForFormat(holeHD, holeKnown))
        {
        case WaffleDisk::OpenResult::OK:
            break;
        case WaffleDisk::OpenResult::NoDisk:
            std::cerr << "waffle-fuse: no disk in drive\n";
            return EXIT_NODISK;
        default:
            std::cerr << "waffle-fuse: cannot open device " << argv[2] << "\n";
            return EXIT_PORT;
        }
        if (wd.isWriteProtected())
        {
            std::cerr << "waffle-fuse: the disk is write-protected\n";
            return EXIT_WRITE_ERROR;
        }
        if (holeKnown && holeHD != geo.isHD && !force)
        {
            std::cerr << "waffle-fuse: the image is " << (geo.isHD ? "HD" : "DD") << " but the disk is "
                      << (holeHD ? "HD" : "DD") << " (use --force to try anyway)\n";
            return EXIT_USAGE;
        }
        if (!yes)
        {
            if (!isatty(0))
            {
                std::cerr << "waffle-fuse: refusing to overwrite the disk without --yes\n";
                return EXIT_USAGE;
            }
            std::cerr << "The disk in " << argv[2] << " will be overwritten with " << argv[3] << ". Type YES: " << std::flush;
            std::string answer;
            std::getline(std::cin, answer);
            if (answer != "YES")
            {
                std::cerr << "waffle-fuse: aborted\n";
                return EXIT_USAGE;
            }
        }
        std::cerr << "waffle-fuse: writing " << argv[3] << (full ? " (all tracks)" : " (changed tracks only)") << "...\n";
        int written = 0;
        int bad = wd.writeImage(geo, image, full ? WaffleDisk::WriteMode::Full : WaffleDisk::WriteMode::Changed,
                                [&](uint32_t c, uint32_t h, int st)
        {
            if (st == 0) written++;
            return trackProgress(c, h, st);
        });
        std::cerr << "\n";
        wd.close();
        if (bad == -2)
        {
            std::cerr << "waffle-fuse: stopped (" << written << " tracks written)\n";
            emit("error=stopped");
            return EXIT_USAGE;
        }
        if (bad < 0)
        {
            std::cerr << "waffle-fuse: the disk is write-protected\n";
            emit("error=write-protected");
            return EXIT_WRITE_ERROR;
        }
        if (bad > 0)
        {
            std::cerr << "waffle-fuse: " << bad << " tracks could not be written\n";
            return EXIT_WRITE_ERROR;
        }
        std::cerr << "waffle-fuse: done, " << written << " tracks written and verified\n";
        emit("event=written tracks=" + std::to_string(written));
        return EXIT_OK;
    }

    // --format <port> / --format-image <file>  <amiga|ofs|ffs|...|dos> <dd|hd> [label] [--quick] [--yes] [--force]
    if (argc >= 5 && (strcmp(argv[1], "--format") == 0 || strcmp(argv[1], "--format-image") == 0))
    {
        const bool toImage = strcmp(argv[1], "--format-image") == 0;
        FormatSpec spec;
        if (!parseFormatKind(argv[3], spec))
        {
            std::cerr << "waffle-fuse: unknown format '" << argv[3] << "' (amiga, ofs, ffs, amiga-intl, ofs-intl, dos)\n";
            return EXIT_USAGE;
        }
        if (strcmp(argv[4], "dd") == 0) spec.hd = false;
        else if (strcmp(argv[4], "hd") == 0) spec.hd = true;
        else
        {
            std::cerr << "waffle-fuse: density must be dd or hd\n";
            return EXIT_USAGE;
        }
        bool quick = false, yes = false, force = false;
        for (int i = 5; i < argc; i++)
        {
            if (strcmp(argv[i], "--quick") == 0) quick = true;
            else if (strcmp(argv[i], "--yes") == 0) yes = true;
            else if (strcmp(argv[i], "--force") == 0) force = true;
            else spec.label = argv[i];
        }

        std::vector<uint8_t> image;
        std::string err = buildFormatImage(spec, image);
        if (!err.empty())
        {
            std::cerr << "waffle-fuse: " << err << "\n";
            return EXIT_USAGE;
        }
        const char* what = spec.kind == FormatSpec::Kind::FAT12 ? "DOS FAT12"
                           : spec.kind == FormatSpec::Kind::AmigaFFS ? "Amiga FFS" : "Amiga OFS";

        if (toImage)
        {
            FILE* f = fopen(argv[2], "wb");
            if (!f || fwrite(image.data(), 1, image.size(), f) != image.size())
            {
                perror(argv[2]);
                if (f) fclose(f);
                return EXIT_USAGE;
            }
            fclose(f);
            std::cerr << "waffle-fuse: wrote " << what << (spec.hd ? " HD" : " DD") << " image " << argv[2] << "\n";
            return EXIT_OK;
        }

        WaffleDisk::Options wo;
        wo.port = argv[2];
        WaffleDisk wd(wo);
        bool holeHD, holeKnown;
        switch (wd.openForFormat(holeHD, holeKnown))
        {
        case WaffleDisk::OpenResult::OK:
            break;
        case WaffleDisk::OpenResult::NoDisk:
            std::cerr << "waffle-fuse: no disk in drive\n";
            emit("error=nodisk");
            return EXIT_NODISK;
        default:
            std::cerr << "waffle-fuse: cannot open device " << argv[2] << "\n";
            emit("error=port");
            return EXIT_PORT;
        }
        if (wd.isWriteProtected())
        {
            std::cerr << "waffle-fuse: the disk is write-protected\n";
            emit("error=write-protected");
            return EXIT_WRITE_ERROR;
        }
        if (holeKnown && holeHD != spec.hd && !force)
        {
            std::cerr << "waffle-fuse: this is a" << (holeHD ? "n HD" : " DD") << " disk, it cannot be reliably formatted as "
                      << (spec.hd ? "HD" : "DD") << " (use --force to try anyway)\n";
            emit("error=density");
            return EXIT_USAGE;
        }
        if (!yes)
        {
            if (!isatty(0))
            {
                std::cerr << "waffle-fuse: refusing to format without --yes\n";
                return EXIT_USAGE;
            }
            std::cerr << "All data on the disk in " << argv[2] << " will be ERASED.\n"
                      << "Format as " << what << (spec.hd ? " HD" : " DD") << (quick ? " (quick)" : "")
                      << (spec.label.empty() ? "" : ", volume \"" + spec.label + "\"") << "? Type YES: " << std::flush;
            std::string answer;
            std::getline(std::cin, answer);
            if (answer != "YES")
            {
                std::cerr << "waffle-fuse: aborted\n";
                return EXIT_USAGE;
            }
        }

        std::cerr << "waffle-fuse: formatting as " << what << (spec.hd ? " HD" : " DD") << (quick ? " (quick)" : "") << "...\n";
        int bad = wd.writeImage(formatGeometry(spec), image,
                                quick ? WaffleDisk::WriteMode::NonEmpty : WaffleDisk::WriteMode::Full,
                                trackProgress);
        std::cerr << "\n";
        wd.close();
        if (bad == -2)
        {
            std::cerr << "waffle-fuse: stopped: the disk is only partly formatted\n";
            emit("error=stopped");
            return EXIT_USAGE;
        }
        if (bad < 0)
        {
            std::cerr << "waffle-fuse: the disk is write-protected\n";
            emit("error=write-protected");
            return EXIT_WRITE_ERROR;
        }
        if (bad > 0)
        {
            std::cerr << "waffle-fuse: " << bad << " tracks could not be written: the disk is damaged\n";
            emit("error=format-failed tracks=" + std::to_string(bad));
            return EXIT_WRITE_ERROR;
        }
        std::cerr << "waffle-fuse: format complete, every track verified\n";
        emit("event=formatted");
        return EXIT_OK;
    }

    // --scan <port>: surface map, good sectors per track
    if (argc >= 3 && strcmp(argv[1], "--scan") == 0)
    {
        WaffleDisk::Options wo;
        wo.port = argv[2];
        wo.forceReadOnly = true;
        WaffleDisk wd(wo);
        if (wd.open() != WaffleDisk::OpenResult::OK)
        {
            std::cerr << "waffle-fuse: cannot read the disk\n";
            return EXIT_DRIVE;
        }
        const DiskGeometry& g = wd.geometry();
        std::cout << diskFormatName(g.format) << (g.isHD ? " HD" : " DD") << ": good sectors per track (of "
                  << g.sectorsPerTrack << "), head 0 / head 1\n";
        int badTracks = 0;
        for (uint32_t c = 0; c < g.numCylinders; c++)
        {
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
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--image") == 0 && i + 1 < argc)          imagePath = argv[++i];
        else if (strcmp(argv[i], "--automount") == 0 && i + 1 < argc) automountBase = argv[++i];
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            usage(argv[0]);
            return EXIT_OK;
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            fuse_argv.push_back(argv[i]);
            fuse_argv.push_back(argv[++i]);
        }
        else if (source.empty() && imagePath.empty() && argv[i][0] != '-') source = argv[i];
        else fuse_argv.push_back(argv[i]);
    }
    if (source.empty() && imagePath.empty())
    {
        usage(argv[0]);
        return EXIT_USAGE;
    }

    struct fuse_args args = FUSE_ARGS_INIT((int)fuse_argv.size(), fuse_argv.data());
    WaffleOptions opts{};
    opts.flushdelay = 1000;
    opts.diskpoll   = 3000;
    if (fuse_opt_parse(&args, &opts, waffle_opts, nullptr) == -1) return EXIT_USAGE;

    struct fuse_cmdline_opts cmd {};
    if (fuse_parse_cmdline(&args, &cmd) != 0) return EXIT_USAGE;
    if (cmd.show_help)
    {
        usage(argv[0]);
        return EXIT_OK;
    }
    if (cmd.show_version)
    {
        fuse_lowlevel_version();
        return EXIT_OK;
    }
    if (!cmd.mountpoint && automountBase.empty())
    {
        usage(argv[0]);
        return EXIT_USAGE;
    }

    enum class FSType { Auto, FAT, AFFS } fsType = FSType::Auto;
    if (opts.format)
    {
        if (strcmp(opts.format, "fat") == 0)       fsType = FSType::FAT;
        else if (strcmp(opts.format, "affs") == 0) fsType = FSType::AFFS;
        else
        {
            std::cerr << "waffle-fuse: unknown format '" << opts.format << "' (use fat or affs)\n";
            return EXIT_USAGE;
        }
    }
    int forcedHD = -1;
    if (opts.density)
    {
        if (strcmp(opts.density, "dd") == 0)      forcedHD = 0;
        else if (strcmp(opts.density, "hd") == 0) forcedHD = 1;
        else
        {
            std::cerr << "waffle-fuse: unknown density '" << opts.density << "' (use dd or hd)\n";
            return EXIT_USAGE;
        }
    }

    DebugMsg::init(opts.debug_hw);
    DebugMsg::initVerbose(opts.debug_hw);

    // ── Open the disk and detect its format ──────────────────────────────────
    std::unique_ptr<IDiskImage> disk;
    WaffleDisk* waffle = nullptr;
    if (!imagePath.empty())
    {
        auto img = std::make_unique<ImageDisk>(imagePath, opts.ro);
        if (!img->open())
        {
            std::cerr << "waffle-fuse: cannot open image " << imagePath << "\n";
            emit("error=port");
            return EXIT_PORT;
        }
        disk = std::move(img);
        source = imagePath;
    }
    else
    {
        WaffleDisk::Options wo;
        wo.port          = source;
        wo.forceReadOnly = opts.ro;
        wo.forcedHD      = forcedHD;
        wo.flushDelayMs  = std::max(100, opts.flushdelay);
        wo.verifyWrites  = !opts.noverify;
        wo.diskPollMs    = opts.diskpoll;
        auto wd = std::make_unique<WaffleDisk>(wo);
        std::cerr << "waffle-fuse: reading disk on " << source << "...\n";
        switch (wd->open())
        {
        case WaffleDisk::OpenResult::OK:
            break;
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
    if (!fsCtx)
    {
        std::cerr << "waffle-fuse: the disk does not contain a valid "
                  << (useAFFS ? "AmigaDOS" : "FAT") << " filesystem\n";
        emit("error=filesystem");
        return EXIT_FILESYSTEM;
    }

    std::string fsName, volume;
    bool readOnly;
    if (useAFFS)
    {
        fsName   = affs_is_ffs(fsCtx) ? "FFS" : "OFS";
        volume   = affs_volume_name(fsCtx);
        readOnly = affs_is_readonly(fsCtx);
    }
    else
    {
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

    auto freeFs = [&]
    {
        if (useAFFS) affs_destroy(fsCtx);
        else fat_fs_free(fsCtx);
        fsCtx = nullptr;
    };

    // ── Mountpoint ────────────────────────────────────────────────────────────
    bool createdDir = false;
    std::string mountpoint;
    // Volume name / label if the disk has one, else e.g. FloppyHDAFFS
    const std::string fallback = std::string("Floppy") + (geo.isHD ? "HD" : "DD") + (useAFFS ? "AFFS" : "DOS");
    const std::string displayName = sanitizeName(volume, fallback);
    if (!automountBase.empty())
    {
        mountpoint = makeAutomountDir(automountBase, displayName, createdDir);
        if (mountpoint.empty())
        {
            std::cerr << "waffle-fuse: cannot create a mountpoint under " << automountBase << "\n";
            emit("error=mount");
            freeFs();
            return EXIT_USAGE;
        }
    }
    else
    {
        mountpoint = cmd.mountpoint;
    }
    g_mountpoint = mountpoint;

    // Name shown by `mount`/`df`; "ro" goes to the kernel when the medium is protected
    std::string fsopts = "-ofsname=waffle:" + source + ",subtype=" + (useAFFS ? "affs" : "vfat");
    fuse_opt_add_arg(&args, fsopts.c_str());
    if (readOnly) fuse_opt_add_arg(&args, "-oro");

    g_allowTrash = opts.trash;
    if (!opts.novolinfo)
    {
        g_volumeIcon = useAFFS ? kAmigaVolumeIcon : kDosVolumeIcon;
        g_volumeInfo = "[Volume Info]\nName=" + displayName +
                       "\nIcon=" + (useAFFS ? "waffle-amiga-floppy" : "waffle-dos-floppy") +
                       "\nIconFile=.waffle-icon.svg\n";
    }
    struct fuse_operations ops = wrapOperations(useAFFS ? affs_get_operations() : fat_get_operations());
    g_fuse = fuse_new(&args, &ops, sizeof(ops), fsCtx);
    if (!g_fuse)
    {
        freeFs();
        if (createdDir) rmdir(mountpoint.c_str());
        emit("error=mount");
        return EXIT_USAGE;
    }
    if (fuse_mount(g_fuse, mountpoint.c_str()) != 0)
    {
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
    if (!flushed && code == EXIT_OK)
    {
        std::cerr << "waffle-fuse: ERROR: some changes could not be written to the disk\n";
        emit("event=write-error");
        code = EXIT_WRITE_ERROR;
    }
    else if (code == EXIT_OK)
    {
        std::cerr << "waffle-fuse: all changes written, the disk can be removed\n";
        emit("event=flushed");
    }

    fuse_opt_free_args(&args);
    free(cmd.mountpoint);
    return code;
}
