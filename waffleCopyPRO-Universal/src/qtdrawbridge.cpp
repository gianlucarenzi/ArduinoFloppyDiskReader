#include "qtdrawbridge.h"
#include "compilerdefs.h"
#include <QDebug>
#include "mainwindow.h"

extern int wmain(QStringList list);

QtDrawBridge::QtDrawBridge(void)
{
    qDebug() << __func__ << "Called";
}

void QtDrawBridge::setup(QString port, QString filename, QStringList command)
{
    qDebug() << __func__ << "Called" << "Input Port =" << port << "Filename" << filename << "command" << command;
    m_filename = filename;
    m_command = command;
    m_port += port;
    qDebug() << __func__ << "Constructed m_port =" << m_port;
}

void QtDrawBridge::run()
{
    QStringList list;
    list << m_port;
    list << m_filename;
    list << m_command;
    qDebug() << __func__ << "Called" << "StringList" << list;
    int rval = wmain(list);
    qDebug() << __func__ << "Finish with " << rval;
    emit QtDrawBridgeSignal(rval);
}
