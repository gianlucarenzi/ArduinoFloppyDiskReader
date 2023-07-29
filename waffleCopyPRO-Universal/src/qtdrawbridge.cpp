#include "qtdrawbridge.h"
#include "compilerdefs.h"
#include <QDebug>
#include "mainwindow.h"

extern int wmain(QStringList list, QString track, QString side, QString status, QString error);

QtDrawBridge::QtDrawBridge(void)
{
    qDebug() << __PRETTY_FUNCTION__ << "Called";
}

void QtDrawBridge::setup(QString port, QString filename, QStringList command, QString track, QString side, QString status, QString error)
{
    qDebug() << __PRETTY_FUNCTION__ << "Called" << "Port" << port << "Filename" << filename << "command" << command;
    m_filename = filename;
    m_command = command;
    m_port = port;
    m_track = track;
    m_side = side;
    m_status = status;
    m_error = error;
}

void QtDrawBridge::run()
{
    QStringList list;
    list << m_port;
    list << m_filename;
    list << m_command;
    qDebug() << __PRETTY_FUNCTION__ << "Called" << "StringList" << list << "Side: " << m_side << "Track: " << m_track << "Status: " << m_status << "Error: " << m_error;
    int rval = wmain(list, m_track, m_side, m_status, m_error);
    qDebug() << __PRETTY_FUNCTION__ << "Finish with " << rval;
    emit QtDrawBridgeSignal(rval);
}
