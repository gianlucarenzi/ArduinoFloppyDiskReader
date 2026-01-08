#include "socketserver.h"
#include "compilerdefs.h"
#include "inc/debugmsg.h"
#include "mainwindow.h"
#include <QElapsedTimer>
#include <QApplication>

SocketServer::SocketServer(void)
{
    //DebugMsg::print(__func__, "Called");
}

SocketServer::~SocketServer(void)
{
    //DebugMsg::print(__func__, "Called");
    emit serverDelete();
}

void SocketServer::setup(void)
{
    //DebugMsg::print(__func__, "Called");
}

void SocketServer::ClearWinSock(void)
{
#ifdef _WIN32
    WSACleanup();
#endif

}
void SocketServer::process()
{
    //DebugMsg::print(__func__, "Called");
    int rval;
#ifdef _WIN32
    // Initialize WinSock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        DebugMsg::print(__func__, "Error at WSAStartup");
        emit NoSocket();
        return;
    }
    // struct addrinfo *result;
    result = NULL;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    rval = getaddrinfo(NULL, SOCKET_PORT, &hints, &result);
    if (rval != 0) {
        DebugMsg::print(__func__, "getaddrinfo failed with " + QString::number(rval));
        ClearWinSock();
        emit NoSocket();
        return;
    }

    // Create a SOCKET for the server to listen for client connections.
    sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sockfd == INVALID_SOCKET)
    {
        DebugMsg::print(__func__, "socket creation failed, ---> sending NoSocket() signal");
        emit NoSocket();
        freeaddrinfo(result);
        ClearWinSock();
        return;
    }

    // Setup the TCP listening socket
    rval = bind(sockfd, result->ai_addr, (int)result->ai_addrlen);
    if (rval == SOCKET_ERROR)
    {
        DebugMsg::print(__func__, "binding socket failed, ---> sending NoBinding() signal");
        emit NoSocketBind();
        freeaddrinfo(result);
        closesocket(sockfd);
        ClearWinSock();
        return;
    }

    freeaddrinfo(result);

    rval = listen(sockfd, SOMAXCONN);
    if (rval == SOCKET_ERROR)
    {
        emit NoListen();
        closesocket(sockfd);
        ClearWinSock();
        return;
    }

    // Accept a client socket
    connfd = accept(sockfd, NULL, NULL);
    if (connfd == INVALID_SOCKET)
    {
        emit NoAccept();
        closesocket(sockfd);
        ClearWinSock();
        return;
    }
#else
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET)
    {
        DebugMsg::print(__func__, "socket creation failed, ---> sending NoSocket() signal");
        emit NoSocket();
        ClearWinSock();
        return;
    } else {
        //DebugMsg::print(__func__, "socket successfully created");
    }
    // CleanUp servaddr struct
    memset(&servaddr, 0, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SOCKET_PORT);

    // Binding newly created socket to given IP and verification
    rval = bind(sockfd, (sockaddr *) &servaddr, sizeof(servaddr));
    if (rval == SOCKET_ERROR)
    {
        DebugMsg::print(__func__, "socket bind failed");
        emit NoSocketBind();
        close(sockfd);
        ClearWinSock();
        return;
    } else {
        //DebugMsg::print(__func__, "socket successfully binded");
    }

    // Now server is ready to listen and verification
    rval = listen(sockfd, 5);
    if (rval == SOCKET_ERROR)
    {
        DebugMsg::print(__func__, "listen failed");
        emit NoListen();
        close(sockfd);
        ClearWinSock();
        return;
    } else {
        //DebugMsg::print(__func__, "server listening...");
    }

    len = sizeof(cli);

    // Accept the data packet from client and verification
    connfd = accept(sockfd, (sockaddr *) &cli, &len);
    if (connfd == INVALID_SOCKET)
    {
        DebugMsg::print(__func__, "server accept failed");
        emit NoAccept();
        close(sockfd);
        ClearWinSock();
    } else {
       // DebugMsg::print(__func__, "server accept the client...");
    }

#endif

    QElapsedTimer m_timer;
    m_timer.start();
    for (;;)
    {
        char buff[MAXBUFF];
        memset(buff, 0, MAXBUFF);
        rval = recv(connfd, buff, sizeof(buff), 0);
        if (rval < 0)
        {
            DebugMsg::print(__func__, "Error");
            break;
        }
        else
        {
            if (rval == 0)
            {
                DebugMsg::print(__func__, "Nothing to read. Maybe client disconnected?");
#ifdef _WIN32
                closesocket(connfd);
#else
                close(connfd);
#endif

#ifdef _WIN32
                connfd = accept(sockfd, NULL, NULL);
                if (connfd == INVALID_SOCKET)
                {
                    DebugMsg::print(__func__, "server accept failed");
                    emit NoAccept();
                    rval = connfd;
                    break;
                }
#else
                connfd = accept(sockfd, (sockaddr*) &cli, &len);
                if (connfd < 0)
                {
                    DebugMsg::print(__func__, "server accept failed");
                    emit NoAccept();
                    rval = connfd;
                    break;
                }
#endif
            }
            else
            {
                //DebugMsg::print(__func__, buff);
                QStringList m_Protocol;
                QString data = QString::fromLatin1(buff);
                int size = data.size();
                //DebugMsg::print(__func__, m_Protocol + "Size" + QString::number(size));
                // Sometimes all data can be sent alltogether, so we need to split all commands in pieces
                if (size > 9)
                {
                    // We need to split all strings apart
                    // TRACK:003SIDE:01STATUS:0ERROR=1
                    DebugMsg::print(__func__, "Error!!!");
                }
                else
                {
                    if (data.contains("STATUS"))
                    {
                        int val = data.split(QLatin1Char(':'))[1].toInt();
                        //DebugMsg::print(__func__, "emit STATUS" + QString::number(val));
                        emit drStatus(val);
                    }
                    if (data.contains("TRACK"))
                    {
                        int val = data.split(QLatin1Char(':'))[1].toInt();
                        //DebugMsg::print(__func__, "emit TRACK" + QString::number(val));
                        emit drTrack(val);
                    }
                    if (data.contains("SIDE"))
                    {
                        int val = data.split(QLatin1Char(':'))[1].toInt();
                        //DebugMsg::print(__func__, "emit SIDE" + QString::number(val));
                        emit drSide(val);
                    }
                    if (data.contains("ERROR"))
                    {
                        int val = data.split(QLatin1Char(':'))[1].toInt();
                        //DebugMsg::print(__func__, "emit ERROR" + QString::number(val));
                        emit drError(val);
                    }
                }
            }
        }

//       if (m_timer.elapsed() > 1000)
//       {
//           DebugMsg::print(__func__, "Called");
//           counter++;
//           if (counter >= 100) { rval = 199; break; }
//           m_timer.start();
//       }
    }
    DebugMsg::print(__func__, "Finish with " + QString::number(rval));
    if (connfd >= 0)
#ifdef _WIN32
        closesocket(connfd);
#else
        close(connfd);
#endif
    // Close the socket
    if (sockfd >= 0)
#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif

    delete(this);
}
