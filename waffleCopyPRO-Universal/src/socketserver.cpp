#include "socketserver.h"
#include "compilerdefs.h"
#include <QDebug>
#include "mainwindow.h"
#include <QElapsedTimer>
#include <QApplication>

SocketServer::SocketServer(void)
{
    //qDebug() << __PRETTY_FUNCTION__ << "Called";
}

SocketServer::~SocketServer(void)
{
    //qDebug() << __PRETTY_FUNCTION__ << "Called";
    emit serverDelete();
}

void SocketServer::setup(void)
{
    //qDebug() << __PRETTY_FUNCTION__ << "Called";
}

void SocketServer::process()
{
    //qDebug() << __PRETTY_FUNCTION__ << "Called";
    int rval;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        qDebug() << __PRETTY_FUNCTION__<< "socket creation failed, ---> sending NoSocket() signal";
        emit NoSocket();
        return;;
    } else {
        //qDebug() << __PRETTY_FUNCTION__ << "socket successfully created";
    }
    // CleanUp servaddr struct
    memset(&servaddr, 0, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SOCKET_PORT);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0)
    {
        qDebug() << __PRETTY_FUNCTION__ << "socket bind failed";
        emit NoSocketBind();
        return;
    } else {
        //qDebug() << __PRETTY_FUNCTION__ << "socket successfully binded";
    }

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0)
    {
        qDebug() << __PRETTY_FUNCTION__ << "listen failed";
        emit NoListen();
        return;
    } else {
        //qDebug() << __PRETTY_FUNCTION__ << "server listening...";
    }

    len = sizeof(cli);

    // Accept the data packet from client and verification
    connfd = accept(sockfd, (SA*)&cli, &len);
    if (connfd < 0)
    {
        qDebug() << __PRETTY_FUNCTION__ << "server accept failed";
        emit NoAccept();
    } else {
       // qDebug() << __PRETTY_FUNCTION__ << "server accept the client...";
    }

    QElapsedTimer m_timer;
    m_timer.start();
    for (;;)
    {
        char buff[MAXBUFF];
        memset(buff, 0, MAXBUFF);
        rval = read(connfd, buff, sizeof(buff));
        if (rval < 0)
        {
            qDebug() << __PRETTY_FUNCTION__ << "Error";
            break;
        }
        else
        {
            if (rval == 0)
            {
                qDebug() << __PRETTY_FUNCTION__ << "Nothing to read. Maybe client disconnected?";
                close(connfd);
                connfd = accept(sockfd, (SA *) &cli, &len);
                if (connfd < 0)
                {
                    qDebug() << __PRETTY_FUNCTION__ << "server accept failed";
                    emit NoAccept();
                    rval = connfd;
                    break;
                }
            }
            else
            {
                //qDebug() << buff;
                QStringList m_Protocol;
                QString data = QString::fromLatin1(buff);
                int size = data.size();
                //qDebug() << m_Protocol << "Size" << size;
                // Sometimes all data can be sent alltogether, so we need to split all commands in pieces
                if (size > 9)
                {
                    // We need to split all strings apart
                    // TRACK:003SIDE:01STATUS:0ERROR=1
                    qDebug() << __PRETTY_FUNCTION__ << "Error!!!";
                }
                else
                {
                    if (data.contains("STATUS"))
                    {
                        int val = data.split(QLatin1Char(':'))[1].toInt();
                        //qDebug() << "emit STATUS" << val;
                        emit drStatus(val);
                    }
                    if (data.contains("TRACK"))
                    {
                        int val = data.split(QLatin1Char(':'))[1].toInt();
                        //qDebug() << "emit TRACK" << val;
                        emit drTrack(val);
                    }
                    if (data.contains("SIDE"))
                    {
                        int val = data.split(QLatin1Char(':'))[1].toInt();
                        //qDebug() << "emit SIDE" << val;
                        emit drSide(val);
                    }
                    if (data.contains("ERROR"))
                    {
                        int val = data.split(QLatin1Char(':'))[1].toInt();
                        //qDebug() << "emit ERROR" << val;
                        emit drError(val);
                    }
                }
            }
        }

//       if (m_timer.elapsed() > 1000)
//       {
//           qDebug() << __PRETTY_FUNCTION__ << "Called";
//           counter++;
//           if (counter >= 100) { rval = 199; break; }
//           m_timer.start();
//       }
    }
    qDebug() << __PRETTY_FUNCTION__ << "Finish with " << rval;
    if (connfd >= 0) close(connfd);
    // Close the socket
    if (sockfd >= 0) close(sockfd);
    delete(this);
}
