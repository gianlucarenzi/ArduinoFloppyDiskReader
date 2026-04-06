#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <memory>
#include <csignal>
#include <cstring>
#include <atomic>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include "disk_cache.h"
#include "nbd_server.h"
#include "waffle_log.h"

std::atomic<bool> g_verbose{false};

static std::unique_ptr<NBDServer> g_server;

void signal_handler(int /*sig*/) {
    if (g_server) {
        std::cout << "\nnbd: shutting down...\n";
        g_server->stop();
    }
}

// Derive control socket path from serial port name.
// "/dev/ttyUSB0" → "/run/waffle-nbd/ttyUSB0.ctl"
static std::string ctlSocketPath(const std::string& portName)
{
    auto slash = portName.rfind('/');
    std::string base = (slash != std::string::npos) ? portName.substr(slash + 1) : portName;
    return "/run/waffle-nbd/" + base + ".ctl";
}

void usage(const char* progname) {
    std::cerr << "Usage: " << progname << " [--verbose] <serial-port> [nbd-address] [nbd-port]\n"
              << "       " << progname << " --serve-image <image.adf|img> [nbd-address] [nbd-port]\n"
              << "       " << progname << " --probe <serial-port>\n"
              << "       " << progname << " --format <amiga|pc> [--hd] <serial-port>\n"
              << "       " << progname << " --format <amiga|pc> [--hd] <output.adf>\n"
              << "       " << progname << " --ctl <socket> format <amiga|pc> [hd]\n"
              << "       " << progname << " --write-image <amiga|pc> [hd] <output.adf>\n"
              << "       " << progname << " --write-image <input.adf|img> <serial-port>\n"
              << "\n"
              << "  --verbose         Enable verbose debug logging to stderr.\n"
              << "  --serve-image     Serve an ADF or IMG file as an NBD block device.\n"
              << "                    No hardware required; writes are flushed back to the file.\n"
              << "  --probe           Check if a Waffle/DrawBridge device is present and a disk\n"
              << "                    is loaded (exit 0=disk, 1=no disk, 2=port/hardware error).\n"
              << "  --format amiga    Format with blank Amiga OFS filesystem.\n"
              << "  --format pc       Format with blank PC FAT12 filesystem.\n"
              << "                    Target is a /dev/ttyXXX port (hardware) or a file (image).\n"
              << "  --hd              (with --format) Use high-density instead of DD.\n"
              << "  --ctl <socket>    Send a command to a running waffle-nbd server.\n"
              << "                    socket defaults to /run/waffle-nbd/<port>.ctl\n"
              << "                    Commands:\n"
              << "                      format amiga|pc [hd]  — format inserted disk\n"
              << "                      clone <output.adf>    — read disk → image file\n"
              << "                      dump  <input.adf>     — write image file → disk\n"
              << "  --write-image     amiga|pc [hd] <file>: create a blank image file.\n"
              << "                    <file> <port>: write an existing ADF/IMG to hardware\n"
              << "                    (format auto-detected from magic bytes and file size).\n"
              << "\n"
              << "Example:\n"
              << "  " << progname << " /dev/ttyUSB0 127.0.0.1 10809\n"
              << "  " << progname << " --serve-image workbench.adf\n"
              << "  " << progname << " --serve-image workbench.adf 127.0.0.1 10810\n"
              << "  " << progname << " --probe /dev/ttyUSB0\n"
              << "  " << progname << " --format amiga /dev/ttyUSB0\n"
              << "  " << progname << " --format amiga blank.adf\n"
              << "  " << progname << " --format pc --hd /dev/ttyUSB0\n"
              << "  " << progname << " --ctl /run/waffle-nbd/ttyUSB0.ctl format amiga\n"
              << "  " << progname << " --ctl /run/waffle-nbd/ttyUSB0.ctl format pc hd\n"
              << "  " << progname << " --ctl /run/waffle-nbd/ttyUSB0.ctl clone /tmp/backup.adf\n"
              << "  " << progname << " --ctl /run/waffle-nbd/ttyUSB0.ctl dump /tmp/workbench.adf\n"
              << "  " << progname << " --write-image amiga blank.adf\n"
              << "  " << progname << " --write-image pc hd blank.img\n"
              << "  " << progname << " --write-image myworkbench.adf /dev/ttyUSB0\n"
              << "\n"
              << "To connect on Linux:\n"
              << "  sudo modprobe nbd\n"
              << "  sudo nbd-client 127.0.0.1 10809 /dev/nbd0\n"
              << "  sudo mount /dev/nbd0 /mnt -t affs  # for Amiga\n";
}

int main(int argc, char* argv[]) {
    // Strip --verbose / -v anywhere before the mode flag (e.g. --verbose --write-image ...).
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            g_verbose = true;
            // Shift remaining args left.
            for (int j = i; j < argc - 1; j++) argv[j] = argv[j + 1];
            argc--;
            i--;
        }
    }

    // --probe <serial-port>: lightweight disk-presence check; no server started.
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

    // --format <amiga|pc> [--hd] <serial-port|output-file>: standalone format.
    // If the target starts with /dev/ it formats the physical floppy via hardware;
    // otherwise the target is treated as a file path and a disk image is written.
    if (argc >= 3 && strcmp(argv[1], "--format") == 0) {
        const char* fmtArg = argv[2];

        FormatType ftype;
        if (strcmp(fmtArg, "amiga") == 0)
            ftype = FormatType::Amiga_OFS;
        else if (strcmp(fmtArg, "pc") == 0)
            ftype = FormatType::PC_FAT12;
        else {
            std::cerr << "waffle-nbd: unknown format '" << fmtArg
                      << "' (use 'amiga' or 'pc')\n";
            usage(argv[0]);
            return 1;
        }

        bool wantsHD = false;
        int  tgtIdx  = 3;
        if (argc > 3 && strcmp(argv[3], "--hd") == 0) {
            wantsHD = true;
            tgtIdx  = 4;
        }
        if (tgtIdx >= argc) {
            usage(argv[0]);
            return 1;
        }

        const std::string target = argv[tgtIdx];
        const char* fmtName      = (ftype == FormatType::Amiga_OFS) ? "Amiga OFS" : "PC FAT12";
        const char* densName     = wantsHD ? "HD (1.76 MB)" : "DD (880 KB / 720 KB)";

        // ── File image mode ───────────────────────────────────────────────────
        if (target.rfind("/dev/", 0) != 0) {
            std::cout << "waffle-nbd: building " << fmtName << " " << densName
                      << " image → " << target << " ...\n";
            auto sectors = buildDiskImage(ftype, wantsHD);
            std::ofstream ofs(target, std::ios::binary | std::ios::trunc);
            if (!ofs) {
                std::cerr << "waffle-nbd: cannot create " << target
                          << ": " << strerror(errno) << "\n";
                return 1;
            }
            for (auto& sec : sectors)
                ofs.write(reinterpret_cast<const char*>(sec.data()), sec.size());
            ofs.close();
            if (!ofs) {
                std::cerr << "waffle-nbd: write error on " << target << "\n";
                return 1;
            }
            std::cout << "waffle-nbd: wrote " << sectors.size() << " sectors ("
                      << (sectors.size() * 512 / 1024) << " KiB) to " << target << "\n";
            return 0;
        }

        // ── Hardware mode ─────────────────────────────────────────────────────
        WaffleDisk disk(target);

        std::cout << "waffle-nbd: opening " << target << " ...\n";
        if (!disk.openDisk()) {
            std::cerr << "waffle-nbd: failed to open " << target << "\n";
            return 2;
        }

        std::cout << "waffle-nbd: formatting as " << fmtName
                  << "  density=" << densName << "\n"
                  << "waffle-nbd: writing 160 tracks — this takes about 3-5 minutes\n";

        int lastPct = -1;
        bool ok = disk.formatDisk(ftype, wantsHD,
            [&](int cur, int total, const std::string& msg) {
                int pct = (total > 0) ? (cur * 100 / total) : 100;
                if (pct != lastPct) {
                    std::cout << "  [" << pct << "%] track " << cur
                              << "/" << total << "  " << msg << "\n";
                    lastPct = pct;
                }
            });

        disk.closeDisk();

        if (ok) {
            std::cout << "waffle-nbd: format complete.\n";
            return 0;
        } else {
            std::cerr << "waffle-nbd: format failed.\n";
            return 1;
        }
    }

    // --write-image <input.adf|img> <serial-port>: write an existing image to hardware.
    // --write-image <amiga|pc> [hd] <output.adf|img>: build a blank image file (no hardware).
    if (argc >= 3 && strcmp(argv[1], "--write-image") == 0) {
        const char* arg2 = argv[2];

        // Detect mode: if second arg is "amiga" or "pc" → build blank image to file.
        if (strcmp(arg2, "amiga") == 0 || strcmp(arg2, "pc") == 0) {
            // ── Generate blank image file ─────────────────────────────────────
            FormatType ftype = (strcmp(arg2, "amiga") == 0)
                             ? FormatType::Amiga_OFS : FormatType::PC_FAT12;
            bool isHD   = false;
            int  outIdx = 3;
            if (argc > 4 && strcmp(argv[3], "hd") == 0) {
                isHD   = true;
                outIdx = 4;
            }
            if (outIdx >= argc) { usage(argv[0]); return 1; }

            const std::string outPath = argv[outIdx];
            std::cout << "waffle-nbd: building "
                      << (ftype == FormatType::Amiga_OFS ? "Amiga OFS" : "PC FAT12")
                      << (isHD ? " HD" : " DD") << " image → " << outPath << " ...\n";

            auto sectors = buildDiskImage(ftype, isHD);
            std::ofstream ofs(outPath, std::ios::binary | std::ios::trunc);
            if (!ofs) {
                std::cerr << "waffle-nbd: cannot create " << outPath
                          << ": " << strerror(errno) << "\n";
                return 1;
            }
            for (auto& sec : sectors)
                ofs.write(reinterpret_cast<const char*>(sec.data()), sec.size());
            ofs.close();
            if (!ofs) {
                std::cerr << "waffle-nbd: write error on " << outPath << "\n";
                return 1;
            }
            std::cout << "waffle-nbd: wrote " << sectors.size() << " sectors ("
                      << (sectors.size() * 512 / 1024) << " KiB) to " << outPath << "\n";
            return 0;
        }

        // ── Write existing image file to hardware ─────────────────────────────
        // Usage: --write-image <input.adf|img> <serial-port>
        if (argc < 4) { usage(argv[0]); return 1; }

        const std::string imgPath  = argv[2];
        const std::string portName = argv[3];

        // Load image file.
        std::ifstream ifs(imgPath, std::ios::binary | std::ios::ate);
        if (!ifs) {
            std::cerr << "waffle-nbd: cannot open " << imgPath
                      << ": " << strerror(errno) << "\n";
            return 1;
        }
        std::streamsize fileSize = ifs.tellg();
        ifs.seekg(0);
        if (fileSize % 512 != 0 || fileSize < 512) {
            std::cerr << "waffle-nbd: " << imgPath
                      << " is not a valid sector-aligned image (size=" << fileSize << ")\n";
            return 1;
        }

        uint32_t numSectors = static_cast<uint32_t>(fileSize / 512);
        std::vector<std::array<uint8_t, 512>> sectors(numSectors);
        for (auto& sec : sectors) {
            if (!ifs.read(reinterpret_cast<char*>(sec.data()), 512)) {
                std::cerr << "waffle-nbd: read error on " << imgPath << "\n";
                return 1;
            }
        }

        // Auto-detect format from magic bytes and sector count.
        FormatType ftype;
        bool isHD = false;
        const uint8_t* b0 = sectors[0].data();
        if (b0[0] == 'D' && b0[1] == 'O' && b0[2] == 'S') {
            ftype = FormatType::Amiga_OFS;
            isHD  = (numSectors > 1760);
        } else {
            ftype = FormatType::PC_FAT12;
            isHD  = (numSectors > 1440);
        }

        const char* fmtName  = (ftype == FormatType::Amiga_OFS) ? "Amiga OFS" : "PC FAT12";
        const char* densName = isHD ? "HD" : "DD";
        std::cout << "waffle-nbd: " << imgPath << " detected as " << fmtName
                  << " " << densName << " (" << numSectors << " sectors)\n"
                  << "waffle-nbd: opening " << portName << " ...\n";

        WaffleDisk disk(portName);
        if (!disk.openDisk()) {
            std::cerr << "waffle-nbd: failed to open " << portName << "\n";
            return 2;
        }

        std::cout << "waffle-nbd: writing " << numSectors << " sectors to "
                  << portName << " — this takes about 3-5 minutes\n";

        int lastPct = -1;
        bool ok = disk.writeDiskImage(ftype, isHD, sectors,
            [&](int cur, int total, const std::string& msg) {
                int pct = (total > 0) ? (cur * 100 / total) : 100;
                if (pct != lastPct) {
                    std::cout << "  [" << pct << "%] track " << cur
                              << "/" << total << "  " << msg << "\n";
                    lastPct = pct;
                }
            });

        disk.closeDisk();

        if (ok) {
            std::cout << "waffle-nbd: image written successfully.\n";
            return 0;
        } else {
            std::cerr << "waffle-nbd: write failed.\n";
            return 1;
        }
    }

    // --ctl <socket-path> <command...>: send a command to a running server.
    if (argc >= 2 && strcmp(argv[1], "--ctl") == 0) {
        if (argc < 4) {
            usage(argv[0]);
            return 1;
        }
        const std::string socketPath = argv[2];

        // Build command string from remaining args.
        std::string cmd;
        for (int i = 3; i < argc; i++) {
            if (i > 3) cmd += ' ';
            cmd += argv[i];
        }
        cmd += '\n';

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd == -1) { perror("socket"); return 1; }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

        if (connect(fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
            std::cerr << "waffle-nbd: cannot connect to " << socketPath
                      << ": " << strerror(errno) << "\n";
            close(fd);
            return 2;
        }

        // Send command.
        write(fd, cmd.c_str(), cmd.size());

        // Stream responses to stdout until connection closes.
        int exitCode  = 1;
        std::string lineBuf;
        char c;
        while (true) {
            struct pollfd pfd{fd, POLLIN, 0};
            if (poll(&pfd, 1, 10000) <= 0) break; // 10-second idle timeout
            ssize_t r = read(fd, &c, 1);
            if (r <= 0) break;
            if (c == '\n') {
                if (lineBuf.substr(0, 8) == "progress") {
                    // "progress PCT CUR TOTAL details..."
                    std::istringstream iss(lineBuf);
                    std::string tok; int pct = 0, cur = 0, total = 0;
                    iss >> tok >> pct >> cur >> total;
                    std::string detail;
                    std::getline(iss, detail);
                    std::cout << "  [" << pct << "%] track " << cur
                              << "/" << total << detail << "\n";
                } else if (lineBuf == "ok") {
                    std::cout << "waffle-nbd: format complete.\n";
                    exitCode = 0;
                } else if (!lineBuf.empty()) {
                    // "error: ..." or any other message
                    std::cout << lineBuf << "\n";
                    exitCode = 1;
                }
                lineBuf.clear();
            } else if (c != '\r') {
                lineBuf += c;
            }
        }

        close(fd);
        return exitCode;
    }

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    // --serve-image <file> [nbd-address] [nbd-port]
    if (strcmp(argv[1], "--serve-image") == 0) {
        if (argc < 3) { usage(argv[0]); return 1; }
        const std::string imgPath = argv[2];
        const std::string nbdAddr = (argc >= 4) ? argv[3] : "127.0.0.1";
        const int         nbdPort = (argc >= 5) ? std::stoi(argv[4]) : 10809;

        auto disk = FileDiskImage::load(imgPath);
        if (!disk) {
            std::cerr << "waffle-nbd: cannot load image " << imgPath << "\n";
            return 1;
        }
        const auto& geo = disk->geometry();
        std::cout << "waffle-nbd: serving image " << imgPath << "\n"
                  << "nbd: format="
                  << (geo.format == DiskFormat::Amiga_ADF ? "Amiga" : "PC/FAT")
                  << " size=" << (disk->totalSectors() * 512 / 1024) << " KB\n";

        g_server = std::make_unique<NBDServer>(std::move(disk));
        // Derive a control socket name from the image filename.
        auto slash = imgPath.rfind('/');
        std::string base = (slash != std::string::npos) ? imgPath.substr(slash + 1) : imgPath;
        g_server->setControlSocket("/run/waffle-nbd/" + base + ".ctl");

        std::signal(SIGINT,  signal_handler);
        std::signal(SIGTERM, signal_handler);

        if (!g_server->run(nbdAddr, nbdPort)) {
            std::cerr << "waffle-nbd: failed to start server\n";
            return 1;
        }
        return 0;
    }

    std::string serialPort = argv[1];
    std::string nbdAddr    = (argc >= 3) ? argv[2] : "127.0.0.1";
    int nbdPort            = (argc >= 4) ? std::stoi(argv[3]) : 10809;

    auto disk = std::make_unique<WaffleDisk>(serialPort);
    std::cout << "waffle-nbd: using port " << serialPort << "\n";

    g_server = std::make_unique<NBDServer>(std::move(disk));
    g_server->setControlSocket(ctlSocketPath(serialPort));

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    if (!g_server->run(nbdAddr, nbdPort)) {
        std::cerr << "waffle-nbd: failed to start server\n";
        return 1;
    }

    return 0;
}
