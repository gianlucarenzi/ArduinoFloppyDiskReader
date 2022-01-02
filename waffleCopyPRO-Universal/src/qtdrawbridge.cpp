#include "qtdrawbridge.h"
#include <QDebug>

extern int wmain(QStringList list);

QtDrawBridge::QtDrawBridge()
{
    qDebug() << __PRETTY_FUNCTION__ << "Called";
}

void QtDrawBridge::setup(QString port, QString filename, QString command)
{
    qDebug() << __PRETTY_FUNCTION__ << "Called" << "Port" << port << "Filename" << filename << "command" << command;
    m_filename = filename;
    m_command = command;
    m_port = port;
}

void QtDrawBridge::run()
{
    QStringList list;
    list << m_port;
    list << m_filename;
    list << m_command;
    qDebug() << __PRETTY_FUNCTION__ << "Called" << "StringList" << list;
    wmain(list);
    qDebug() << __PRETTY_FUNCTION__ << "Finish";
}
