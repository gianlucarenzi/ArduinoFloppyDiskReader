#include "qtdrawbridge.h"
#include "compilerdefs.h"
#include "inc/debugmsg.h"
#include "mainwindow.h"
#include "adfwritermanager.h" // Include adfwritermanager.h for ADFWriterManager
using namespace ArduinoFloppyReader;

extern int wmain(QStringList list);

QtDrawBridge::QtDrawBridge(void)
{
    DebugMsg::print(__func__, "Called");
}

void QtDrawBridge::setup(QString port, QString filename, QStringList command)
{
    DebugMsg::print(__func__, "Called Input Port =" + port + " Filename" + filename + " command" + command.join(" "));
    m_filename = filename;
    m_command = command;
    m_port = port;
    DebugMsg::print(__func__, "Constructed m_port =" + m_port);
}

void QtDrawBridge::run()
{
    DebugMsg::print(__func__, "Attempting to open device on port:" + m_port);
    ArduinoFloppyReader::DiagnosticResponse openResult = ArduinoFloppyReader::ADFWriterManager::getInstance().openDevice(m_port.toStdWString());
    int rval = static_cast<int>(openResult); // Default to the openResult error code
    DebugMsg::print(__func__, "OpenResult to Int: " + QString::number(rval));
    if (openResult != ArduinoFloppyReader::DiagnosticResponse::drOK) {
        DebugMsg::print(__func__, "Failed to open device on port:" + m_port + " Error:" + QString::fromStdString(ArduinoFloppyReader::ADFWriterManager::getInstance().getLastError()));
        // Emit the error directly if opening failed
        emit QtDrawBridgeSignal(rval);
        return;
    }

    // If opening succeeded, proceed with wmain
    QStringList list;
    list << m_port;
    list << m_filename;
    list << m_command;
    DebugMsg::print(__func__, "Calling wmain with StringList" + list.join(" "));
    rval = wmain(list);
    DebugMsg::print(__func__, "Finish with " + QString::number(rval));
    emit QtDrawBridgeSignal(rval);
}
