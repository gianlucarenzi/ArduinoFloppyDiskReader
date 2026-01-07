#include "diagnosticthread.h"
#include "diagnosticconfig.h"
#include "ADFWriter.h"
#include "ArduinoInterface.h"
#include <string>
#include <algorithm>
#include <QString>
#include <QThread>
#include <chrono>

DiagnosticThread::DiagnosticThread()
{
}

void DiagnosticThread::setup(QString port)
{
    m_port = port;
}

void DiagnosticThread::run()
{
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

    ArduinoFloppyReader::ADFWriter writer;

    // Convert QString to wstring for the port name
    std::wstring portName = m_port.toStdWString();

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

        // Questions about disk insertion - DO POLLING WITH 30 SECOND TIMEOUT
        if (lowerQuestion.find("please insert") != std::string::npos ||
            lowerQuestion.find("inserted disk is not write protected") != std::string::npos) {

            emit diagnosticMessage("");
            emit diagnosticMessage("Waiting for disk insertion (max 30 seconds)...");

            // Create temporary interface for polling
            ArduinoFloppyReader::ArduinoInterface pollDevice;

            // Open the port
            if (pollDevice.openPort(portName) != ArduinoFloppyReader::DiagnosticResponse::drOK) {
                emit diagnosticMessage("ERROR: Cannot open port for disk detection");
                return false;
            }

            // Enable reading
            if (pollDevice.enableReading(true, false) != ArduinoFloppyReader::DiagnosticResponse::drOK) {
                pollDevice.closePort();
                emit diagnosticMessage("ERROR: Cannot enable drive for disk detection");
                return false;
            }

            // Poll for disk with 30 second timeout
            auto startTime = std::chrono::steady_clock::now();
            const int timeoutSeconds = 30;
            bool diskFound = false;
            int lastSecond = -1;

            while (std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::steady_clock::now() - startTime).count() < timeoutSeconds) {

                // Check if thread interruption was requested
                if (isInterruptionRequested()) {
                    pollDevice.closePort();
                    emit diagnosticMessage("Diagnostic interrupted by user");
                    emit diagnosticComplete(false);
                    return false;
                }

                // Check for disk
                pollDevice.checkForDisk(true);
                if (pollDevice.isDiskInDrive()) {
                    diskFound = true;
                    break;
                }

                // Show countdown every second
                int currentSecond = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startTime).count();
                if (currentSecond != lastSecond && currentSecond % 5 == 0) {
                    int remaining = timeoutSeconds - currentSecond;
                    emit diagnosticMessage("  Still waiting... (" + QString::number(remaining) + " seconds remaining)");
                    lastSecond = currentSecond;
                }

                // Wait 500ms before next check
                QThread::msleep(500);
            }

            // Close the polling device
            pollDevice.closePort();

            if (diskFound) {
                emit diagnosticMessage("Disk detected!");
                return true;
            } else {
                emit diagnosticMessage("");
                emit diagnosticMessage("ERROR: No disk detected after 30 seconds timeout");
                emit diagnosticMessage("Please insert a disk and try again");
                return false;
            }
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
    bool result = writer.runDiagnostics(portName, messageOutput, askQuestion);

    // Emit completion signal
    if (result) {
        emit diagnosticMessage("\n=== DIAGNOSTIC TEST COMPLETED SUCCESSFULLY ===");
    } else {
        emit diagnosticMessage("\n=== DIAGNOSTIC TEST FAILED ===");
    }

    emit diagnosticComplete(result);

#endif // SIMULATE_DIAGNOSTIC_SUCCESS || SIMULATE_DIAGNOSTIC_FAILURE
}
