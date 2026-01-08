#include "qtdrawbridge.h"
#include "compilerdefs.h"
#include <QDebug>
#include "mainwindow.h"
#include "adfwritermanager.h" // Include adfwritermanager.h for ADFWriterManager
using namespace ArduinoFloppyReader;

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
    m_port = port;
    qDebug() << __func__ << "Constructed m_port =" << m_port;
}

void QtDrawBridge::run()
{
    qDebug() << __func__ << "Attempting to open device on port:" << m_port;
    ArduinoFloppyReader::DiagnosticResponse openResult = ArduinoFloppyReader::ADFWriterManager::getInstance().openDevice(m_port.toStdWString());
    int rval = static_cast<int>(openResult); // Default to the openResult error code
    qDebug() << __func__ << "OpenResult to Int: " << rval;
    if (openResult != ArduinoFloppyReader::DiagnosticResponse::drOK) {
        qDebug() << __func__ << "Failed to open device on port:" << m_port << "Error:" << QString::fromStdString(ArduinoFloppyReader::ADFWriterManager::getInstance().getLastError());
        // Emit the error directly if opening failed
        emit QtDrawBridgeSignal(rval);
        return;
    }

    // If opening succeeded, proceed with wmain
    QStringList list;
    list << m_port;
    list << m_filename;
    list << m_command;
    qDebug() << __func__ << "Calling wmain with StringList" << list;
    rval = wmain(list);
    qDebug() << __func__ << "Finish with " << rval;
    emit QtDrawBridgeSignal(rval);
}
