#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include <QThread>
#include <QString>
#include <QStringList>
#include <stdlib.h>
#include <stdio.h>

// Socket server definitions/includes
#ifndef _WIN32
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <errno.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR   -1
    #define MAXBUFF         256
    #define SOCKET_PORT     10240
#else
    #define _WINSOCKAPI_
    #define _WINSOCK2API_
    #undef UNICODE
    #define WIN32_LEAN_AND_MEAN
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>
    #pragma comment (lib, "Ws2_32.lib") //Winsock Library
    #define MAXBUFF         256
    #define SOCKET_PORT     "10240"
#endif

class SocketServer : public QObject
{
    Q_OBJECT
public:
    SocketServer();
    ~SocketServer();
    void setup(void);

public slots:
    void process();

signals:
    void NoSocket();
    void NoSocketBind();
    void NoListen();
    void NoAccept();
    void serverDelete();
    // Operational signals for TRACK, SIDE, STATUS and ERROR
    void drStatus(int);
    void drSide(int);
    void drTrack(int);
    void drError(int);

private:
    SOCKET sockfd;
    SOCKET connfd;
#ifdef _WIN32
    struct addrinfo *result;
    struct addrinfo hints;
#else
    socklen_t len;
    struct sockaddr_in servaddr;
    struct sockaddr_in cli;
#endif
private:
    void ClearWinSock(void);
};

#endif // SOCKETSERVER_H
