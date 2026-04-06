#include "nbd_server.h"
#include "waffle_log.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <endian.h>
#include <poll.h>
#include <thread>
#include <algorithm>

// ── NBD Protocol Constants ───────────────────────────────────────────────────

#define NBD_MAGIC             0x4e42444d41474943ULL // "NBDMAGIC"
#define NBD_OPTS_MAGIC        0x49484156454f5054ULL // "IHAVEOPT"
#define NBD_REP_MAGIC         0x3e889045565a9ULL     // Magic for option replies

#define NBD_FLAG_FIXED_NEWSTYLE  (1 << 0)
#define NBD_FLAG_NO_ZEROES       (1 << 1)

#define NBD_FLAG_HAS_FLAGS       (1 << 0)
#define NBD_FLAG_READ_ONLY       (1 << 1)
#define NBD_FLAG_SEND_FLUSH      (1 << 2)

#define NBD_OPT_EXPORT_NAME      1
#define NBD_OPT_ABORT            2
#define NBD_OPT_LIST             3
#define NBD_OPT_INFO             6
#define NBD_OPT_GO               7

#define NBD_REP_ACK              1
#define NBD_REP_SERVER           2
#define NBD_REP_INFO             3
#define NBD_REP_ERR_UNSUP        0x80000001

#define NBD_INFO_EXPORT          0

#define NBD_REQUEST_MAGIC     0x25609513
#define NBD_REPLY_MAGIC_REPLY 0x67446698

#define NBD_CMD_READ          0
#define NBD_CMD_WRITE         1
#define NBD_CMD_DISC          2
#define NBD_CMD_FLUSH         3

// ── Helpers ──────────────────────────────────────────────────────────────────

static bool read_all(int fd, void* buf, size_t len) {
    uint8_t* p = (uint8_t*)buf;
    while (len > 0) {
        ssize_t r = read(fd, p, len);
        if (r <= 0) return false;
        p += r;
        len -= r;
    }
    return true;
}

static bool write_all(int fd, const void* buf, size_t len) {
    const uint8_t* p = (const uint8_t*)buf;
    while (len > 0) {
        ssize_t r = write(fd, p, len);
        if (r <= 0) return false;
        p += r;
        len -= r;
    }
    return true;
}

// ── NBDServer Implementation ────────────────────────────────────────────────

NBDServer::NBDServer(std::unique_ptr<IDiskImage> disk)
    : m_disk(std::move(disk))
{}

NBDServer::~NBDServer() {
    stop();
}

void NBDServer::stop() {
    m_running = false;
    if (m_serverFd != -1) {
        shutdown(m_serverFd, SHUT_RDWR);
        close(m_serverFd);
        m_serverFd = -1;
    }
    // Wake up and join the control thread.
    if (m_ctlFd != -1) {
        shutdown(m_ctlFd, SHUT_RDWR);
        close(m_ctlFd);
        m_ctlFd = -1;
    }
    if (m_ctlThread.joinable())
        m_ctlThread.join();
}

bool NBDServer::run(const std::string& address, int port) {
    m_serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverFd == -1) return false;

    int opt = 1;
    setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, address.c_str(), &addr.sin_addr);

    if (bind(m_serverFd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        std::cerr << "nbd: cannot bind to " << address << ":" << port << "\n";
        close(m_serverFd);
        m_serverFd = -1;
        return false;
    }

    if (listen(m_serverFd, 5) == -1) {
        close(m_serverFd);
        m_serverFd = -1;
        return false;
    }

    m_running = true;
    std::cout << "nbd: server bound to " << address << ":" << port << "\n";

    // Start control socket thread if a path was configured.
    if (!m_ctlPath.empty()) {
        m_ctlThread = std::thread(&NBDServer::runControlThread, this);
        std::cout << "nbd: control socket at " << m_ctlPath << "\n";
    }

    // When set, skip Phases 1+2: the disk port is already open after a
    // format/dump command and we can go straight to accepting a new client.
    bool skipToPhase3 = false;

    while (m_running) {

        if (!skipToPhase3) {
            // ── Phase 1: probe until disk is detected (port is closed) ───────
            std::cout << "nbd: waiting for disk...\n";
            while (m_running) {
                VLOG("nbd: probing...");
                if (m_disk->probePresent()) break;
                // probe() itself takes ~100-500ms; add a small gap to avoid
                // hammering the serial port between retries.
                std::this_thread::sleep_for(std::chrono::milliseconds(2500));
            }
            if (!m_running) break;

            // ── Phase 2: open port (no geometry detection yet) ───────────────
            if (!m_disk->openDisk()) {
                std::cerr << "nbd: failed to open port after probe, retrying...\n";
                continue;
            }
        }
        skipToPhase3 = false;

        // ── Phase 3+4: accept client and serve, with remount retry ───────────
        // If remount() fails (motor not yet at reading speed), keep the disk
        // port open so the motor continues spinning between attempts.
        // Calling closeDisk()/resetDevice() on each failure restarts the
        // Arduino and the motor spin-up cycle, causing an endless failure loop.
        // Re-emit "disk detected" before each accept so that external handlers
        // (e.g. waffle-nbd-handler.sh) reconnect nbd-client after a retry.
        bool diskGone = false;
        while (m_running && !diskGone) {
            std::cout << "nbd: disk detected, accepting connections on "
                      << address << ":" << port << "\n";
            // Phase 3: accept one client (poll so stop() can interrupt)
            int clientFd = -1;
            while (m_running) {
                struct pollfd pfd{m_serverFd, POLLIN, 0};
                int r = poll(&pfd, 1, 2000);
                if (r > 0) {
                    sockaddr_in clientAddr{};
                    socklen_t clientLen = sizeof(clientAddr);
                    clientFd = accept(m_serverFd, (sockaddr*)&clientAddr, &clientLen);
                    if (clientFd != -1) break;
                }
                // Timeout: verify disk is still present using the open port.
                if (!m_disk->checkPresence()) {
                    std::cout << "nbd: disk removed while waiting for client\n";
                    diskGone = true;
                    break;
                }
            }

            if (diskGone || clientFd == -1) break;

            // Phase 4: detect geometry, then serve
            std::cout << "nbd: client connected, detecting disk format...\n";
            if (m_disk->remount()) {
                handleClient(clientFd);
                close(clientFd);
                std::cout << "nbd: client disconnected\n";
                // Stop motor before polling for absence (port stays open).
                m_disk->parkDisk();
                break; // → Phase 5
            } else {
                // Keep the disk port open so the motor continues to spin up.
                // Resetting the Arduino here would restart the spin-up cycle
                // and cause an endless failure loop.
                std::cerr << "nbd: format detection failed, retrying in 3s...\n";
                close(clientFd);
                std::this_thread::sleep_for(std::chrono::seconds(3));
                // Loop back to Phase 3: accept a new nbd-client connection.
            }
        }

        if (!m_running) break;

        // ── Phase 5: wait for disk to be confirmed absent ─────────────────────
        // Port is open, motor is off (parkDisk was called after serve, or disk
        // was removed). checkPresence() uses only COMMAND_CHECKDISKEXISTS.
        // Skip this phase if format/dump just completed via control socket:
        // the disk port is still open and geometry already updated — go straight
        // to Phase 3 to accept the reconnecting nbd-client.
        if (m_skipAbsentWait.exchange(false)) {
            std::cout << "nbd: skipping disk-absent wait (format/dump just completed)\n";
            skipToPhase3 = true;
            continue; // → skip Phase 1+2, go straight to Phase 3
        }
        if (!diskGone) {
            std::cout << "nbd: waiting for disk removal...\n";
            while (m_running) {
                VLOG("nbd: checking presence...");
                if (!m_disk->checkPresence()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        m_disk->closeDisk();
        std::cout << "nbd: disk absent\n";
    }

    return true;
}

void NBDServer::handleClient(int fd) {
    int opt = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    if (!handshake(fd)) {
        std::cerr << "nbd: handshake failed\n";
        return;
    }
    mainLoop(fd);
}

bool NBDServer::handshake(int fd) {
    // 1. Initial Handshake
    struct {
        uint64_t magic;
        uint64_t opts_magic;
        uint16_t flags;
    } __attribute__((packed)) init;

    init.magic = htobe64(NBD_MAGIC);
    init.opts_magic = htobe64(NBD_OPTS_MAGIC);
    init.flags = htobe16(NBD_FLAG_FIXED_NEWSTYLE | NBD_FLAG_NO_ZEROES);

    if (!write_all(fd, &init, sizeof(init))) return false;

    // 2. Client flags
    uint32_t client_flags_be;
    if (!read_all(fd, &client_flags_be, sizeof(client_flags_be))) return false;
    uint32_t client_flags = be32toh(client_flags_be);
    bool client_no_zeroes = (client_flags & NBD_FLAG_NO_ZEROES) != 0;

    // 3. Option Negotiation
    while (true) {
        struct {
            uint64_t magic;
            uint32_t opt;
            uint32_t len;
        } __attribute__((packed)) option;

        if (!read_all(fd, &option, sizeof(option))) return false;
        option.magic = be64toh(option.magic);
        option.opt = be32toh(option.opt);
        option.len = be32toh(option.len);

        if (option.magic != NBD_OPTS_MAGIC) {
            std::cerr << "nbd: invalid option magic\n";
            return false;
        }

        std::vector<char> data(option.len);
        if (option.len > 0) {
            if (!read_all(fd, data.data(), option.len)) return false;
        }

        if (option.opt == NBD_OPT_EXPORT_NAME) {
            std::string name(data.begin(), data.end());
            std::cout << "nbd: client requested export name '" << name << "'\n";
            uint64_t size = (uint64_t)m_disk->totalSectors() * 512;
            uint16_t flags = NBD_FLAG_HAS_FLAGS | NBD_FLAG_SEND_FLUSH;
            if (m_disk->isWriteProtected()) flags |= NBD_FLAG_READ_ONLY;

            uint64_t size_be = htobe64(size);
            uint16_t flags_be = htobe16(flags);

            if (!write_all(fd, &size_be, sizeof(size_be))) return false;
            if (!write_all(fd, &flags_be, sizeof(flags_be))) return false;

            if (!client_no_zeroes) {
                uint8_t zeroes[124] = {0};
                if (!write_all(fd, zeroes, sizeof(zeroes))) return false;
            }
            return true;
        } 
        else if (option.opt == NBD_OPT_LIST) {
            std::cout << "nbd: client requested list\n";
            struct {
                uint64_t magic;
                uint32_t opt;
                uint32_t reply_type;
                uint32_t len;
                uint32_t name_len;
            } __attribute__((packed)) rep;
            
            const char* name = "waffle";
            uint32_t nlen = strlen(name);
            
            rep.magic = htobe64(NBD_REP_MAGIC);
            rep.opt = htobe32(NBD_OPT_LIST);
            rep.reply_type = htobe32(NBD_REP_SERVER);
            rep.len = htobe32(4 + nlen);
            rep.name_len = htobe32(nlen);
            
            if (!write_all(fd, &rep, sizeof(rep))) return false;
            if (!write_all(fd, name, nlen)) return false;
            
            // Send ACK
            struct {
                uint64_t magic;
                uint32_t opt;
                uint32_t reply_type;
                uint32_t len;
            } __attribute__((packed)) ack;
            ack.magic = htobe64(NBD_REP_MAGIC);
            ack.opt = htobe32(NBD_OPT_LIST);
            ack.reply_type = htobe32(NBD_REP_ACK);
            ack.len = 0;
            if (!write_all(fd, &ack, sizeof(ack))) return false;
        }
        else if (option.opt == NBD_OPT_INFO || option.opt == NBD_OPT_GO) {
            std::cout << "nbd: client requested " << (option.opt == NBD_OPT_GO ? "GO" : "INFO") << "\n";

            uint64_t size = (uint64_t)m_disk->totalSectors() * 512;
            uint16_t xflags = NBD_FLAG_HAS_FLAGS | NBD_FLAG_SEND_FLUSH;
            if (m_disk->isWriteProtected()) xflags |= NBD_FLAG_READ_ONLY;

            // NBD_REP_INFO with NBD_INFO_EXPORT
            struct {
                uint64_t magic;
                uint32_t opt;
                uint32_t reply_type;
                uint32_t len;
                uint16_t info_type;
                uint64_t export_size;
                uint16_t transmission_flags;
            } __attribute__((packed)) info_rep;
            info_rep.magic              = htobe64(NBD_REP_MAGIC);
            info_rep.opt                = htobe32(option.opt);
            info_rep.reply_type         = htobe32(NBD_REP_INFO);
            info_rep.len                = htobe32(sizeof(uint16_t) + sizeof(uint64_t) + sizeof(uint16_t));
            info_rep.info_type          = htobe16(NBD_INFO_EXPORT);
            info_rep.export_size        = htobe64(size);
            info_rep.transmission_flags = htobe16(xflags);
            if (!write_all(fd, &info_rep, sizeof(info_rep))) return false;

            // NBD_REP_ACK
            struct {
                uint64_t magic;
                uint32_t opt;
                uint32_t reply_type;
                uint32_t len;
            } __attribute__((packed)) ack;
            ack.magic      = htobe64(NBD_REP_MAGIC);
            ack.opt        = htobe32(option.opt);
            ack.reply_type = htobe32(NBD_REP_ACK);
            ack.len        = 0;
            if (!write_all(fd, &ack, sizeof(ack))) return false;

            if (option.opt == NBD_OPT_GO)
                return true; // enter transmission phase
            // NBD_OPT_INFO: keep negotiating
        }
        else if (option.opt == NBD_OPT_ABORT) {
            return false;
        } else {
            std::cout << "nbd: client requested unsupported option " << option.opt << "\n";
            struct {
                uint64_t magic;
                uint32_t opt;
                uint32_t reply_type;
                uint32_t len;
            } __attribute__((packed)) reply;
            reply.magic = htobe64(NBD_REP_MAGIC);
            reply.opt = htobe32(option.opt);
            reply.reply_type = htobe32(NBD_REP_ERR_UNSUP);
            reply.len = 0;
            if (!write_all(fd, &reply, sizeof(reply))) return false;
        }
    }
}

bool NBDServer::mainLoop(int fd) {
    while (m_running) {
        // Wait for an incoming request with a 2-second timeout.
        // The timeout lets us detect disk removal even when the client is idle.
        struct pollfd pfd{fd, POLLIN, 0};
        int ret = poll(&pfd, 1, 2000);

        if (ret == 0) {
            // Timeout: check whether the disk is still present.
            if (!m_disk->isDiskPresent()) {
                std::cerr << "nbd: disk removed, closing connection\n";
                break;
            }
            continue;
        }
        if (ret < 0) break;
        struct {
            uint32_t magic;
            uint32_t type;
            uint64_t handle;
            uint64_t offset;
            uint32_t len;
        } __attribute__((packed)) req;

        if (!read_all(fd, &req, sizeof(req))) break;
        if (be32toh(req.magic) != NBD_REQUEST_MAGIC) {
            std::cerr << "nbd: invalid request magic\n";
            break;
        }

        // Disconnect immediately if the disk was removed.
        if (!m_disk->isDiskPresent()) {
            std::cerr << "nbd: disk removed, closing connection\n";
            break;
        }

        uint32_t type = be32toh(req.type);
        uint64_t offset = be64toh(req.offset);
        uint32_t len = be32toh(req.len);

        struct {
            uint32_t magic;
            uint32_t error;
            uint64_t handle;
        } __attribute__((packed)) reply;
        reply.magic = htobe32(NBD_REPLY_MAGIC_REPLY);
        reply.handle = req.handle;
        reply.error = 0;

        if (type == NBD_CMD_DISC) {
            std::cout << "nbd: client disconnect command\n";
            break;
        } else if (type == NBD_CMD_READ) {
            VLOG("nbd: READ offset=" << offset << " len=" << len);
            std::vector<uint8_t> data(len);
            bool ok = true;

            uint32_t first_lba = offset / 512;
            uint32_t last_lba = (offset + len - 1) / 512;
            
            uint32_t buf_pos = 0;
            for (uint32_t lba = first_lba; lba <= last_lba; ++lba) {
                uint8_t sector[512];
                if (!m_disk->readSector(lba, sector)) {
                    ok = false;
                    break;
                }
                
                uint32_t sec_off = (lba == first_lba) ? (offset % 512) : 0;
                uint32_t sec_len = std::min(512u - sec_off, len - buf_pos);
                memcpy(data.data() + buf_pos, sector + sec_off, sec_len);
                buf_pos += sec_len;
            }

            if (!ok) reply.error = htobe32(5); // EIO
            if (!write_all(fd, &reply, sizeof(reply))) break;
            if (ok) {
                if (!write_all(fd, data.data(), len)) break;
            }
        } else if (type == NBD_CMD_WRITE) {
            VLOG("nbd: WRITE offset=" << offset << " len=" << len);
            std::vector<uint8_t> data(len);
            if (!read_all(fd, data.data(), len)) break;

            if (m_disk->isWriteProtected()) {
                reply.error = htobe32(30); // EROFS
            } else {
                bool ok = true;
                uint32_t first_lba = offset / 512;
                uint32_t last_lba = (offset + len - 1) / 512;
                uint32_t buf_pos = 0;

                for (uint32_t lba = first_lba; lba <= last_lba; ++lba) {
                    uint32_t sec_off = (lba == first_lba) ? (offset % 512) : 0;
                    uint32_t sec_len = std::min(512u - sec_off, len - buf_pos);

                    if (sec_len == 512) {
                        if (!m_disk->writeSector(lba, data.data() + buf_pos)) ok = false;
                    } else {
                        uint8_t sector[512];
                        if (!m_disk->readSector(lba, sector)) ok = false;
                        else {
                            memcpy(sector + sec_off, data.data() + buf_pos, sec_len);
                            if (!m_disk->writeSector(lba, sector)) ok = false;
                        }
                    }
                    if (!ok) break;
                    buf_pos += sec_len;
                }
                if (!ok) reply.error = htobe32(5); // EIO
            }
            if (!write_all(fd, &reply, sizeof(reply))) break;
        } else if (type == NBD_CMD_FLUSH) {
            VLOG("nbd: FLUSH");
            m_disk->flush();
            if (!write_all(fd, &reply, sizeof(reply))) break;
        } else {
            reply.error = htobe32(22); // EINVAL
            if (!write_all(fd, &reply, sizeof(reply))) break;
        }
    }
    return true;
}

// ── Control socket ────────────────────────────────────────────────────────────

void NBDServer::runControlThread()
{
    // Ensure the parent directory exists (e.g. /run/waffle-nbd/).
    {
        std::string dir = m_ctlPath;
        auto slash = dir.rfind('/');
        if (slash != std::string::npos) {
            dir = dir.substr(0, slash);
            // mkdir -p: create each component that doesn't exist yet.
            for (size_t i = 1; i <= dir.size(); i++) {
                if (i == dir.size() || dir[i] == '/') {
                    std::string part = dir.substr(0, i);
                    if (mkdir(part.c_str(), 0755) == -1 && errno != EEXIST) {
                        std::cerr << "nbd-ctl: cannot create " << part
                                  << ": " << strerror(errno) << "\n";
                        return;
                    }
                }
            }
        }
    }

    m_ctlFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_ctlFd == -1) {
        std::cerr << "nbd-ctl: socket() failed: " << strerror(errno) << "\n";
        return;
    }

    unlink(m_ctlPath.c_str()); // remove stale socket

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, m_ctlPath.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(m_ctlFd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        std::cerr << "nbd-ctl: bind(" << m_ctlPath << ") failed: " << strerror(errno) << "\n";
        close(m_ctlFd);
        m_ctlFd = -1;
        return;
    }
    // Allow any local process to reach the socket (access is controlled by
    // filesystem permissions on the parent directory, not the socket itself).
    chmod(m_ctlPath.c_str(), 0666);

    if (listen(m_ctlFd, 4) == -1) {
        close(m_ctlFd);
        m_ctlFd = -1;
        return;
    }

    while (m_running) {
        struct pollfd pfd{m_ctlFd, POLLIN, 0};
        if (poll(&pfd, 1, 1000) <= 0) continue;

        sockaddr_un clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(m_ctlFd, (sockaddr*)&clientAddr, &clientLen);
        if (clientFd == -1) continue;

        handleControlClient(clientFd);
        close(clientFd);
    }

    close(m_ctlFd);
    m_ctlFd = -1;
    unlink(m_ctlPath.c_str());
}

// Read a newline-terminated line from fd (with a short per-byte timeout).
static std::string readLine(int fd)
{
    std::string line;
    char c;
    while (line.size() < 256) {
        struct pollfd pfd{fd, POLLIN, 0};
        if (poll(&pfd, 1, 5000) <= 0) break; // 5-second read timeout
        if (read(fd, &c, 1) != 1) break;
        if (c == '\n') break;
        if (c != '\r') line += c;
    }
    return line;
}

void NBDServer::handleControlClient(int fd)
{
    std::string line = readLine(fd);
    VLOG("nbd-ctl: command: '" << line << "'");

    std::istringstream iss(line);
    std::string cmd, arg1, arg2;
    iss >> cmd >> arg1 >> arg2;

    // ── format amiga|pc [hd] ──────────────────────────────────────────────────
    if (cmd == "format") {
        FormatType ftype;
        if      (arg1 == "amiga") ftype = FormatType::Amiga_OFS;
        else if (arg1 == "pc")    ftype = FormatType::PC_FAT12;
        else {
            const char* err = "error: unknown type (use amiga or pc)\n";
            write_all(fd, err, strlen(err));
            return;
        }
        bool isHD = (arg2 == "hd");

        if (!m_disk->isDiskPresent()) {
            const char* err = "error: no disk present\n";
            write_all(fd, err, strlen(err));
            return;
        }

        std::cerr << "nbd-ctl: format request type="
                  << (ftype == FormatType::Amiga_OFS ? "amiga" : "pc")
                  << " hd=" << (isHD ? "1" : "0") << "\n";

        bool ok = m_disk->formatDisk(ftype, isHD,
            [&](int cur, int total, const std::string& msg) {
                int pct = (total > 0) ? (cur * 100 / total) : 100;
                std::string resp = "progress " + std::to_string(pct) +
                                   " " + std::to_string(cur) +
                                   " " + std::to_string(total) +
                                   " " + msg + "\n";
                write_all(fd, resp.c_str(), resp.size());
            });

        if (ok) {
            write_all(fd, "ok\n", 3);
            std::cerr << "waffle-event: format-done type="
                      << (ftype == FormatType::Amiga_OFS ? "affs" : "vfat")
                      << " hd=" << (isHD ? "1" : "0") << std::endl;
            // Allow immediate reconnect without disk removal (geometry changed).
            m_skipAbsentWait = true;
        } else {
            const char* err = "error: format failed (disk write-protected or removed?)\n";
            write_all(fd, err, strlen(err));
        }
        return;
    }

    // ── clone <output-file> ───────────────────────────────────────────────────
    // Reads all sectors from the currently inserted disk and saves to a file.
    if (cmd == "clone") {
        if (arg1.empty()) {
            const char* err = "error: usage: clone <output-file>\n";
            write_all(fd, err, strlen(err));
            return;
        }

        if (!m_disk->isDiskPresent()) {
            const char* err = "error: no disk present\n";
            write_all(fd, err, strlen(err));
            return;
        }

        const uint32_t total = m_disk->totalSectors();
        std::cerr << "nbd-ctl: clone request file=" << arg1
                  << " sectors=" << total << "\n";

        std::ofstream ofs(arg1, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            std::string err = "error: cannot create " + arg1 + "\n";
            write_all(fd, err.c_str(), err.size());
            return;
        }

        int lastPct = -1;
        uint8_t buf[512];
        bool readError = false;
        for (uint32_t lba = 0; lba < total; lba++) {
            if (!m_disk->readSector(lba, buf)) {
                std::string err = "error: read error at sector " + std::to_string(lba) + "\n";
                write_all(fd, err.c_str(), err.size());
                readError = true;
                break;
            }
            ofs.write(reinterpret_cast<char*>(buf), 512);

            int pct = (int)(lba * 100 / total);
            if (pct != lastPct) {
                std::string resp = "progress " + std::to_string(pct) +
                                   " " + std::to_string(lba) +
                                   " " + std::to_string(total) +
                                   " sector " + std::to_string(lba) + "\n";
                write_all(fd, resp.c_str(), resp.size());
                lastPct = pct;
            }
        }

        ofs.close();

        if (!readError) {
            write_all(fd, "ok\n", 3);
            std::cerr << "waffle-event: clone-done file=" << arg1
                      << " sectors=" << total << std::endl;
        }
        return;
    }

    // ── dump <input-file> ─────────────────────────────────────────────────────
    // Writes a disk image file to the currently inserted physical disk.
    // The format is auto-detected from the image (magic bytes + size).
    if (cmd == "dump") {
        if (arg1.empty()) {
            const char* err = "error: usage: dump <input-file>\n";
            write_all(fd, err, strlen(err));
            return;
        }

        if (!m_disk->isDiskPresent()) {
            const char* err = "error: no disk present\n";
            write_all(fd, err, strlen(err));
            return;
        }

        // Load image into memory.
        std::ifstream ifs(arg1, std::ios::binary | std::ios::ate);
        if (!ifs) {
            std::string err = "error: cannot open " + arg1 + "\n";
            write_all(fd, err.c_str(), err.size());
            return;
        }
        std::streamsize fileSize = ifs.tellg();
        ifs.seekg(0);
        if (fileSize % 512 != 0 || fileSize < 512) {
            std::string err = "error: " + arg1 + " is not a valid sector-aligned image\n";
            write_all(fd, err.c_str(), err.size());
            return;
        }

        uint32_t numSectors = static_cast<uint32_t>(fileSize / 512);
        std::vector<std::array<uint8_t, 512>> sectors(numSectors);
        for (auto& sec : sectors) {
            if (!ifs.read(reinterpret_cast<char*>(sec.data()), 512)) {
                const char* err = "error: read error on input file\n";
                write_all(fd, err, strlen(err));
                return;
            }
        }

        // Auto-detect format and density.
        const uint8_t* b0 = sectors[0].data();
        FormatType ftype = (b0[0]=='D' && b0[1]=='O' && b0[2]=='S')
                         ? FormatType::Amiga_OFS : FormatType::PC_FAT12;
        bool isHD = (numSectors > 1760);

        std::cerr << "nbd-ctl: dump request file=" << arg1
                  << " sectors=" << numSectors
                  << " type=" << (ftype == FormatType::Amiga_OFS ? "amiga" : "pc")
                  << " hd=" << (isHD ? "1" : "0") << "\n";

        // Cast to WaffleDisk to call writeDiskImage (it's an extension of IDiskImage).
        auto* wd = dynamic_cast<WaffleDisk*>(m_disk.get());
        if (!wd) {
            const char* err = "error: dump not supported on this device type\n";
            write_all(fd, err, strlen(err));
            return;
        }

        bool ok = wd->writeDiskImage(ftype, isHD, sectors,
            [&](int cur, int total2, const std::string& msg) {
                int pct = (total2 > 0) ? (cur * 100 / total2) : 100;
                std::string resp = "progress " + std::to_string(pct) +
                                   " " + std::to_string(cur) +
                                   " " + std::to_string(total2) +
                                   " " + msg + "\n";
                write_all(fd, resp.c_str(), resp.size());
            });

        if (ok) {
            write_all(fd, "ok\n", 3);
            std::cerr << "waffle-event: dump-done file=" << arg1 << std::endl;
            // Allow immediate reconnect without disk removal (content changed).
            m_skipAbsentWait = true;
        } else {
            const char* err = "error: write failed (disk write-protected or removed?)\n";
            write_all(fd, err, strlen(err));
        }
        return;
    }

    const char* err = "error: unknown command (use: format amiga|pc [hd]  |  clone <file>  |  dump <file>)\n";
    write_all(fd, err, strlen(err));
}
