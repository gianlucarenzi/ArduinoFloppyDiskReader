#pragma once
// waffle-nbd: Minimal Network Block Device (NBD) server implementation
// Allows exposing a WaffleDisk as a Linux block device (/dev/nbd0).

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include "disk_cache.h"

class NBDServer {
public:
    explicit NBDServer(std::unique_ptr<IDiskImage> disk);
    ~NBDServer();

    // Set the Unix-domain socket path for the control interface.
    // Must be called before run().  If empty, control socket is disabled.
    void setControlSocket(const std::string& path) { m_ctlPath = path; }

    // Start the server listening on a TCP port or Unix socket.
    // blocks until stop() is called or an error occurs.
    bool run(const std::string& address, int port = 10809);

    // Stop the server.
    void stop();

private:
    void handleClient(int clientFd);
    bool handshake(int clientFd);
    bool mainLoop(int clientFd);

    // Control socket (runs in a separate thread).
    void runControlThread();
    void handleControlClient(int fd);

    std::unique_ptr<IDiskImage> m_disk;
    std::atomic<bool>           m_running{false};
    int                         m_serverFd = -1;

    std::string                 m_ctlPath;
    int                         m_ctlFd    = -1;
    std::thread                 m_ctlThread;
};
