#include <iostream>
#include <string>
#include <memory>
#include <csignal>
#include <cstring>
#include <atomic>
#include "disk_cache.h"
#include "nbd_server.h"
#include "waffle_log.h"

std::atomic<bool> g_verbose{false};

static std::unique_ptr<NBDServer> g_server;

void signal_handler(int sig) {
    if (g_server) {
        std::cout << "\nnbd: shutting down...\n";
        g_server->stop();
    }
}

void usage(const char* progname) {
    std::cerr << "Usage: " << progname << " [--verbose] <serial-port> [nbd-address] [nbd-port]\n"
              << "       " << progname << " --probe <serial-port>\n"
              << "\n"
              << "  --verbose   Enable verbose debug logging to stderr.\n"
              << "  --probe     Check if a Waffle/DrawBridge device is present and a disk\n"
              << "              is loaded (exit 0=disk, 1=no disk, 2=port/hardware error).\n"
              << "\n"
              << "Example:\n"
              << "  " << progname << " /dev/ttyUSB0 127.0.0.1 10809\n"
              << "  " << progname << " --probe /dev/ttyUSB0\n"
              << "\n"
              << "To connect on Linux:\n"
              << "  sudo modprobe nbd\n"
              << "  sudo nbd-client 127.0.0.1 10809 /dev/nbd0\n"
              << "  sudo mount /dev/nbd0 /mnt -t affs  # for Amiga\n";
}

int main(int argc, char* argv[]) {
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

    // Parse --verbose / -v early, then shift remaining args.
    int argStart = 1;
    if (argc > argStart &&
        (strcmp(argv[argStart], "--verbose") == 0 || strcmp(argv[argStart], "-v") == 0)) {
        g_verbose = true;
        ++argStart;
    }

    if (argc - argStart < 1) {
        usage(argv[0]);
        return 1;
    }

    std::string serialPort = argv[argStart];
    std::string nbdAddr    = (argc - argStart >= 2) ? argv[argStart + 1] : "127.0.0.1";
    int nbdPort            = (argc - argStart >= 3) ? std::stoi(argv[argStart + 2]) : 10809;

    auto disk = std::make_unique<WaffleDisk>(serialPort);
    std::cout << "waffle-nbd: using port " << serialPort << "\n";

    g_server = std::make_unique<NBDServer>(std::move(disk));

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    if (!g_server->run(nbdAddr, nbdPort)) {
        std::cerr << "waffle-nbd: failed to start server\n";
        return 1;
    }

    return 0;
}
