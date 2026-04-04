#pragma once
// waffle-nbd: Minimal Network Block Device (NBD) server implementation
// Allows exposing a WaffleDisk as a Linux block device (/dev/nbd0).

#include <string>
#include <memory>
#include <atomic>
#include "disk_cache.h"

class NBDServer {
public:
    explicit NBDServer(std::unique_ptr<IDiskImage> disk);
    ~NBDServer();

    // Start the server listening on a TCP port or Unix socket.
    // blocks until stop() is called or an error occurs.
    bool run(const std::string& address, int port = 10809);

    // Stop the server.
    void stop();

private:
    void handleClient(int clientFd);
    bool handshake(int clientFd);
    bool mainLoop(int clientFd);

    std::unique_ptr<IDiskImage> m_disk;
    std::atomic<bool>           m_running{false};
    int                         m_serverFd = -1;
};
