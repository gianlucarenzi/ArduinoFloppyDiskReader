#include <QDebug>

#include "diagnosticthread.h"
#include "diagnosticconfig.h"
#include "ADFWriter.h"
#include "ArduinoInterface.h"
#include <string>
#include <algorithm>
#include <QString>
#include <QThread>
#include <chrono>
#include "adfwritermanager.h"
using namespace ArduinoFloppyReader;

DiagnosticThread::DiagnosticThread()
{
}

void DiagnosticThread::setup(QString port)
{
    qDebug() << __func__ << "Called. Input Port =" << port;
    m_port = port;
    qDebug() << __func__ << "Constructed m_port =" << m_port;
}

void DiagnosticThread::run()
{
    // Global prompt for disk insertion
    emit diagnosticMessage("");
    emit diagnosticMessage(">>> PLEASE INSERT A WRITE-PROTECTED AMIGADOS DISK IN THE DRIVE <<<");
    emit diagnosticMessage(">>> The disk will be used for testing and must be write-protected <<<");
    emit diagnosticMessage("");
    QThread::msleep(1000); // Initial display time for the message

#if defined(SIMULATE_DIAGNOSTIC_SUCCESS) || defined(SIMULATE_DIAGNOSTIC_FAILURE)
    // ============================================
    // SIMULATED DIAGNOSTIC MODE
    // ============================================

    emit diagnosticMessage("=== SIMULATION MODE ACTIVE ===");
    emit diagnosticMessage("(No real hardware required)");
    emit diagnosticMessage("");
    QThread::msleep(800);

    if (isInterruptionRequested()) {
        emit diagnosticMessage("Diagnostic interrupted by user");
        emit diagnosticComplete(false);
        return;
    }

    emit diagnosticMessage("Attempting to open and use " + m_port + " without CTS");
    QThread::msleep(600);

    emit diagnosticMessage("Testing CTS pin");
    QThread::msleep(500);
    emit diagnosticMessage("CTS OK. Reconnecting with CTS enabled");
    QThread::msleep(600);

    emit diagnosticMessage("Board is running firmware version 1.9.5");
    QThread::msleep(500);

    emit diagnosticMessage("Fetching latest firmware version...");
    QThread::msleep(1000);
    emit diagnosticMessage("Firmware is up-to-date");
    QThread::msleep(600);

    if (isInterruptionRequested()) {
        emit diagnosticMessage("Diagnostic interrupted by user");
        emit diagnosticComplete(false);
        return;
    }

    emit diagnosticMessage("Features Set:");
    emit diagnosticMessage("      o Disk Change Pin Support");
    emit diagnosticMessage("      o DrawBridge Classic");
    emit diagnosticMessage("      o HD/DD Detect Enabled");
    QThread::msleep(800);

    emit diagnosticMessage("Testing USB->Serial transfer speed (read)");
    QThread::msleep(1500);
    emit diagnosticMessage("Read speed test passed. USB to serial converter is functioning correctly!");
    QThread::msleep(600);

    emit diagnosticMessage("");
    emit diagnosticMessage(">>> PLEASE INSERT A WRITE-PROTECTED AMIGADOS DISK IN THE DRIVE <<<");
    emit diagnosticMessage(">>> The disk will be used for testing and must be write-protected <<<");
    emit diagnosticMessage("");
    QThread::msleep(1000);
    emit diagnosticMessage("Waiting for disk insertion...");
    QThread::msleep(1500);
    emit diagnosticMessage("Please ensure the disk is write-protected...");
    QThread::msleep(1500);
    emit diagnosticMessage("Checking for disk...");
    QThread::msleep(10000);

    if (isInterruptionRequested()) {
        emit diagnosticMessage("Diagnostic interrupted by user");
        emit diagnosticComplete(false);
        return;
    }

    emit diagnosticMessage("Testing write-protect signal");
    QThread::msleep(600);
    emit diagnosticMessage(">>> Disk is correctly write-protected");
    QThread::msleep(600);

    emit diagnosticMessage("Enabling the drive");
    QThread::msleep(1000);
    emit diagnosticMessage("The drive should now be spinning and the LED should be on");
    QThread::msleep(800);

    emit diagnosticMessage("Checking for INDEX pulse from drive");
    QThread::msleep(800);

#ifdef SIMULATE_DIAGNOSTIC_FAILURE
    // SIMULATE FAILURE HERE
    emit diagnosticMessage("ERROR: No INDEX pulse detected from drive");
    emit diagnosticMessage("Please check that a disk is inserted and the following PINS on the Arduino: 2");
    QThread::msleep(1000);
    emit diagnosticMessage("\n=== DIAGNOSTIC TEST FAILED ===");
    emit diagnosticComplete(false);
    return;
#else
    emit diagnosticMessage("INDEX pulse detected");
    QThread::msleep(600);
#endif

    emit diagnosticMessage("Measuring Drive RPM...");
    QThread::msleep(1500);
    emit diagnosticMessage("Drive RPM measured as 299.85 RPM");
    QThread::msleep(700);

    emit diagnosticMessage("Checking board type...");
    QThread::msleep(800);
    emit diagnosticMessage("Board EEPROM setting matches detected board type.");
    QThread::msleep(600);

    emit diagnosticMessage("Asking the Arduino to find Track 0");
    QThread::msleep(1200);
    emit diagnosticMessage("Track 0 was found. Asking the Arduino to find Track 70");
    QThread::msleep(1500);
    emit diagnosticMessage("You should hear the drive head moving to track 70");
    QThread::msleep(800);

    emit diagnosticMessage("Starting drive, and seeking to track 40.");
    QThread::msleep(1200);
    emit diagnosticMessage("Disk inserted was detected as DOUBLE DENSITY");
    QThread::msleep(700);

    emit diagnosticMessage("Checking for DATA from drive");
    QThread::msleep(800);
    emit diagnosticMessage("DATA pulse detected");
    QThread::msleep(600);

    if (isInterruptionRequested()) {
        emit diagnosticMessage("Diagnostic interrupted by user");
        emit diagnosticComplete(false);
        return;
    }

    emit diagnosticMessage("Attempting to read a track from the UPPER side of the disk");
    QThread::msleep(2000);
    emit diagnosticMessage("Tracks found!");
    QThread::msleep(700);

    emit diagnosticMessage("Attempting to read a track from the LOWER side of the disk");
    QThread::msleep(2000);
    emit diagnosticMessage("Tracks found!");
    QThread::msleep(700);

    emit diagnosticMessage("\n=== DIAGNOSTIC TEST COMPLETED SUCCESSFULLY ===");
    emit diagnosticComplete(true);

#else
    // ============================================
    // REAL HARDWARE MODE
    // ============================================

    if (isInterruptionRequested()) {
        emit diagnosticMessage("Diagnostic interrupted by user");
        emit diagnosticComplete(false);
        return;
    }

    // Convert QString to wstring for the port name
    std::wstring portName = m_port.toStdWString();

    // Attempt to open the device
    DiagnosticResponse openResult = ADFWriterManager::getInstance().openDevice(portName);
    if (openResult != DiagnosticResponse::drOK) {
        emit diagnosticMessage(QString("ERROR: Could not open serial port for diagnostic: %1\n").arg(QString::fromStdString(ADFWriterManager::getInstance().getLastError())));
        ADFWriterManager::getInstance().closeDevice(); // Ensure port is closed
        emit diagnosticComplete(false);
        return;
    }

    // Define the messageOutput callback
    auto messageOutput = [this](bool isError, const std::string message) {
        QString qMessage = QString::fromStdString(message);
        if (isError) {
            qMessage = "ERROR: " + qMessage;
        }
        emit diagnosticMessage(qMessage);
    };

    // Define the askQuestion callback - with disk polling for real hardware
    auto askQuestion = [this, &messageOutput, &portName](bool isQuestion, const std::string question) -> bool {
        QString qQuestion = QString::fromStdString(question);

        // Log the question/message
        emit diagnosticMessage(">>> " + qQuestion);

        // If it's not a yes/no question, just return true to continue
        if (!isQuestion) {
            return true;
        }

        // For yes/no questions, check the content and respond appropriately
        std::string lowerQuestion = question;
        std::transform(lowerQuestion.begin(), lowerQuestion.end(), lowerQuestion.begin(), ::tolower);

        // Questions about hearing/seeing the drive work
        if (lowerQuestion.find("did the floppy disk start spinning") != std::string::npos) {
            emit diagnosticMessage("The drive should now be spinning and the LED should be on");
            return true;
        }

        if (lowerQuestion.find("could you hear") != std::string::npos) {
            emit diagnosticMessage("You should hear the drive head moving to the specified track");
            return true;
        }

        if (lowerQuestion.find("did you hear") != std::string::npos) {
            emit diagnosticMessage("You should hear the drive head moving");
            return true;
        }


        // Questions about continuing after warnings
        if (lowerQuestion.find("do you want to continue") != std::string::npos ||
            lowerQuestion.find("would you like") != std::string::npos) {
            emit diagnosticMessage("Continuing with diagnostic test...");
            return true;
        }

        // Default: continue the test
        return true;
    };

    // Run the diagnostics
    bool result = ADFWriterManager::getInstance().runDiagnostics(portName, messageOutput, askQuestion);

    // Emit completion signal
    if (result) {
        emit diagnosticMessage("\n=== DIAGNOSTIC TEST COMPLETED SUCCESSFULLY ===");
    } else {
        emit diagnosticMessage("\n=== DIAGNOSTIC TEST FAILED ===");
    }

    emit diagnosticComplete(result);

#endif // SIMULATE_DIAGNOSTIC_SUCCESS || SIMULATE_DIAGNOSTIC_FAILURE
}
