#include "qtdrawbridge.h"
#include "compilerdefs.h"
#include <QDebug>

extern int wmain(QStringList list, QString track, QString side, QString status);

QtDrawBridge::QtDrawBridge()
{
    qDebug() << __PRETTY_FUNCTION__ << "Called";
}

void QtDrawBridge::setup(QString port, QString filename, QStringList command, QString track, QString side, QString status)
{
    qDebug() << __PRETTY_FUNCTION__ << "Called" << "Port" << port << "Filename" << filename << "command" << command;
    m_filename = filename;
    m_command = command;
    m_port = port;
    m_track = track;
    m_side = side;
    m_status = status;
}

void QtDrawBridge::run()
{
    QStringList list;
    list << m_port;
    list << m_filename;
    list << m_command;
    qDebug() << __PRETTY_FUNCTION__ << "Called" << "StringList" << list << "Side: " << m_side << "Track: " << m_track << "Status: " << m_status;
    wmain(list, m_track, m_side, m_status);
    qDebug() << __PRETTY_FUNCTION__ << "Finish";
}
