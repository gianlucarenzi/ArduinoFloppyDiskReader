#include <iostream>
#include <string>
#include <memory>
#include <csignal>
#include "disk_cache.h"
#include "nbd_server.h"

static std::unique_ptr<NBDServer> g_server;

void signal_handler(int sig) {
    if (g_server) {
        std::cout << "\nnbd: shutting down...\n";
        g_server->stop();
    }
}

void usage(const char* progname) {
    std::cerr << "Usage: " << progname << " <serial-port> [nbd-address] [nbd-port]\n"
              << "\n"
              << "Example:\n"
              << "  " << progname << " /dev/ttyUSB0 127.0.0.1 10809\n"
              << "\n"
              << "To connect on Linux:\n"
              << "  sudo modprobe nbd\n"
              << "  sudo nbd-client 127.0.0.1 10809 /dev/nbd0\n"
              << "  sudo mount /dev/nbd0 /mnt -t affs  # for Amiga\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    std::string serialPort = argv[1];
    std::string nbdAddr = (argc >= 3) ? argv[2] : "127.0.0.1";
    int nbdPort = (argc >= 4) ? std::stoi(argv[3]) : 10809;

    auto disk = std::make_unique<WaffleDisk>(serialPort);
    std::cout << "waffle-nbd: opening " << serialPort << "...\n";
    
    if (!disk->open()) {
        std::cerr << "waffle-nbd: failed to open device " << serialPort << "\n";
        return 1;
    }

    std::cout << "waffle-nbd: disk ready. format=" 
              << (disk->geometry().format == DiskFormat::Amiga_ADF ? "Amiga" : "PC/FAT")
              << " size=" << (disk->totalSectors() * 512 / 1024) << " KB\n";

    g_server = std::make_unique<NBDServer>(std::move(disk));

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    if (!g_server->run(nbdAddr, nbdPort)) {
        std::cerr << "waffle-nbd: failed to start server\n";
        return 1;
    }

    return 0;
}
