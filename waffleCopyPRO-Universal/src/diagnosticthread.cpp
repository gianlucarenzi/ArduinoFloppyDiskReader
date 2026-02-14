#include "inc/debugmsg.h"

#include "diagnosticthread.h"
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
    DebugMsg::print(__func__, "Called. Input Port =" + port);
    m_port = port;
    DebugMsg::print(__func__, "Constructed m_port =" + m_port);
}

void DiagnosticThread::run()
{
    // Initialize state machine
    m_currentState = DiagnosticState::dsInitialize;
    m_overallSuccess = true;

    // Main state machine loop
    while (m_currentState != DiagnosticState::dsCleaning) {
        // Check for user interruption (except during cleanup)
        if (checkInterruption() && m_currentState != DiagnosticState::dsError) {
            emit diagnosticMessage(tr("Diagnostic interrupted by user."));
            transitionToError();
            continue;
        }

        // State machine dispatcher
        switch (m_currentState) {
            case DiagnosticState::dsInitialize:
                m_currentState = stateInitialize();
                break;

            case DiagnosticState::dsWaitForDisk:
                m_currentState = stateWaitForDisk();
                break;

            case DiagnosticState::dsFirmwareVersion:
                m_currentState = stateFirmwareVersion();
                break;

            case DiagnosticState::dsTestCTS:
                m_currentState = stateTestCTS();
                break;

            case DiagnosticState::dsTestTransferSpeed:
                m_currentState = stateTestTransferSpeed();
                break;

            case DiagnosticState::dsMeasureRPM:
                m_currentState = stateMeasureRPM();
                break;

            case DiagnosticState::dsTrackSeek:
                m_currentState = stateTrackSeek();
                break;

            case DiagnosticState::dsReadTrack40:
                m_currentState = stateReadTrack40();
                break;

            case DiagnosticState::dsComplete:
                m_currentState = stateComplete();
                break;

            case DiagnosticState::dsError:
                m_currentState = stateCleaning();
                break;

            case DiagnosticState::dsCleaning:
                // Should not reach here due to loop condition
                break;
        }
    }

    // Final cleanup state
    stateCleaning();
}

// ============================================================================
// Helper Methods
// ============================================================================

void DiagnosticThread::transitionToError()
{
    m_overallSuccess = false;
    m_currentState = DiagnosticState::dsError;
}

bool DiagnosticThread::checkInterruption()
{
    return isInterruptionRequested();
}

QString DiagnosticThread::translateError(const QString &error)
{
    // Translate known error messages from the Arduino library
    if (error.contains("An unknown error occurred attempting to open access to the specified COM port"))
        return tr("An unknown error occurred attempting to open access to the specified COM port.");
    if (error.contains("An unknown response was was received from the Arduino"))
        return tr("An unknown response was received from the Arduino.");
    
    // Return original error if no translation found
    return error;
}

// ============================================================================
// State Handler Methods
// ============================================================================

DiagnosticState DiagnosticThread::stateInitialize()
{
    // Display initial prompt
    emit diagnosticMessage("");
    emit diagnosticMessage(tr(">>> PLEASE INSERT AN AMIGADOS DISK IN THE DRIVE <<<"));
    emit diagnosticMessage(tr(">>> The disk will be used for testing <<<"));
    emit diagnosticMessage("");
    QThread::msleep(INITIAL_PROMPT_DELAY);

    // Convert QString to wstring for the port name
    std::wstring portName = m_port.toStdWString();

    // Attempt to open the device
    emit diagnosticMessage(tr("Attempting to open device: %1").arg(m_port));
    DiagnosticResponse openResult = ADFWriterManager::getInstance().openDevice(portName);
    if (openResult != DiagnosticResponse::drOK) {
        emit diagnosticMessage(tr("ERROR: Could not open serial port for diagnostic: %1\n")
            .arg(translateError(QString::fromStdString(ADFWriterManager::getInstance().getLastError()))));
        m_overallSuccess = false;
        return DiagnosticState::dsError;
    }
    emit diagnosticMessage(tr("Device opened successfully."));

    return DiagnosticState::dsWaitForDisk;
}

DiagnosticState DiagnosticThread::stateWaitForDisk()
{
    emit diagnosticMessage(tr("Waiting for a disk to be inserted..."));
    //emit diagnosticMessage("  Note: Using INDEX pulse detection - disk must be spinning");

    int diskWaitAttempts = 0;
    const int statusMessageInterval = DISK_WAIT_STATUS_INTERVAL / DISK_WAIT_POLL_INTERVAL;

    while (!isInterruptionRequested()) {
        // Use INDEX pulse test to detect disk presence
        // The INDEX pulse comes from a hole in the disk hub - if detected, disk is present
        DiagnosticResponse indexPulseStatus = ADFWriterManager::getInstance().testIndexPulse();

        // Success condition: INDEX pulse detected means disk is present and spinning
        if (indexPulseStatus == DiagnosticResponse::drOK) {
            emit diagnosticMessage(tr("Disk detected!"));
            return DiagnosticState::dsFirmwareVersion;
        }

        // Periodic status updates (every 5 seconds)
        //if (diskWaitAttempts % statusMessageInterval == 0) {
        //    emit diagnosticMessage("  Still waiting... (No INDEX pulse detected - insert disk)");
        //}

        // Wait before next check
        QThread::msleep(DISK_WAIT_POLL_INTERVAL);
        diskWaitAttempts++;

        // Timeout check
        if (diskWaitAttempts * DISK_WAIT_POLL_INTERVAL >= DISK_WAIT_TIMEOUT) {
            emit diagnosticMessage(tr("ERROR: Timeout waiting for disk."));
            m_overallSuccess = false;
            return DiagnosticState::dsError;
        }
    }

    // User interruption
    emit diagnosticMessage(tr("Diagnostic interrupted by user during disk wait."));
    m_overallSuccess = false;
    return DiagnosticState::dsError;
}

DiagnosticState DiagnosticThread::stateFirmwareVersion()
{
    FirmwareVersion version = ADFWriterManager::getInstance().getFirwareVersion();
    if (version.major == 0 && version.minor == 0) {
        emit diagnosticMessage(tr("ERROR: Could not read firmware version: %1")
            .arg(translateError(QString::fromStdString(ADFWriterManager::getInstance().getLastError()))));
        m_overallSuccess = false;
    } else {
        emit diagnosticMessage(tr("Firmware Version: %1.%2.%3")
            .arg(version.major).arg(version.minor).arg(version.buildNumber));
    }

    QThread::msleep(INTER_STATE_DELAY);
    return DiagnosticState::dsMeasureRPM;  // Skip CTS and transfer speed tests
}

DiagnosticState DiagnosticThread::stateTestCTS()
{
    emit diagnosticMessage(tr("Testing CTS pin..."));

    if (ADFWriterManager::getInstance().testCTS() != DiagnosticResponse::drOK) {
        emit diagnosticMessage(tr("ERROR: CTS test failed: %1")
            .arg(translateError(QString::fromStdString(ADFWriterManager::getInstance().getLastError()))));
        m_overallSuccess = false;
        return DiagnosticState::dsError;
    }

    emit diagnosticMessage(tr("CTS test passed."));
    QThread::msleep(INTER_STATE_DELAY);
    return DiagnosticState::dsTestTransferSpeed;
}

DiagnosticState DiagnosticThread::stateTestTransferSpeed()
{
    emit diagnosticMessage(tr("Testing USB->Serial transfer speed..."));

    if (ADFWriterManager::getInstance().testTransferSpeed() != DiagnosticResponse::drOK) {
        emit diagnosticMessage(tr("ERROR: Transfer speed test failed: %1")
            .arg(translateError(QString::fromStdString(ADFWriterManager::getInstance().getLastError()))));
        m_overallSuccess = false;
        return DiagnosticState::dsError;
    }

    emit diagnosticMessage(tr("Transfer speed test passed."));
    QThread::msleep(INTER_STATE_DELAY);
    return DiagnosticState::dsMeasureRPM;
}

DiagnosticState DiagnosticThread::stateMeasureRPM()
{
    emit diagnosticMessage(tr("Measuring Drive RPM..."));

    float rpm = 0.0f;
    DiagnosticResponse result = ADFWriterManager::getInstance().measureDriveRPM(rpm);

    // Check if firmware is too old for RPM measurement
    if (result == DiagnosticResponse::drOldFirmware) {
        emit diagnosticMessage(tr("  RPM test skipped (requires firmware v1.9+)"));
        QThread::msleep(INTER_STATE_DELAY);
        return DiagnosticState::dsTrackSeek;
    }

    if (result != DiagnosticResponse::drOK) {
        emit diagnosticMessage(tr("ERROR: RPM measurement failed: %1")
            .arg(translateError(QString::fromStdString(ADFWriterManager::getInstance().getLastError()))));
        m_overallSuccess = false;
        return DiagnosticState::dsError;
    }

    // Display result
    emit diagnosticMessage(tr("Drive RPM: %1").arg(rpm));

    // Validate RPM range
    if (rpm < RPM_MIN_RANGE || rpm > RPM_MAX_RANGE) {
        emit diagnosticMessage(tr("WARNING: RPM is outside optimal range (%1-%2).")
            .arg(RPM_MIN_RANGE).arg(RPM_MAX_RANGE));
    }

    QThread::msleep(INTER_STATE_DELAY);
    return DiagnosticState::dsTrackSeek;
}

DiagnosticState DiagnosticThread::stateTrackSeek()
{
    emit diagnosticMessage(tr("Performing track seek tests (Track 0 <-> Track 79)..."));

    for (int i = 0; i < TRACK_SEEK_ITERATIONS; ++i) {
        // Check for interruption
        if (isInterruptionRequested()) {
            emit diagnosticMessage(tr("Diagnostic interrupted during track seek test."));
            m_overallSuccess = false;
            return DiagnosticState::dsError;
        }

        // Seek to Track 0
        emit diagnosticMessage(tr("  Seek iteration %1: Finding Track 0").arg(i + 1));
        if (ADFWriterManager::getInstance().findTrack0() != DiagnosticResponse::drOK) {
            emit diagnosticMessage(tr("ERROR: Failed to find Track 0: %1")
                .arg(translateError(QString::fromStdString(ADFWriterManager::getInstance().getLastError()))));
            m_overallSuccess = false;
            return DiagnosticState::dsError;
        }
        QThread::msleep(TRACK_SEEK_SHORT_DELAY);

        // Check for interruption again
        if (isInterruptionRequested()) {
            emit diagnosticMessage(tr("Diagnostic interrupted during track seek test."));
            m_overallSuccess = false;
            return DiagnosticState::dsError;
        }

        // Seek to Track 79
        emit diagnosticMessage(tr("  Seek iteration %1: Selecting Track 79").arg(i + 1));
        if (ADFWriterManager::getInstance().selectTrack(TRACK_MAX) != DiagnosticResponse::drOK) {
            emit diagnosticMessage(tr("ERROR: Failed to select Track 79: %1")
                .arg(translateError(QString::fromStdString(ADFWriterManager::getInstance().getLastError()))));
            m_overallSuccess = false;
            return DiagnosticState::dsError;
        }
        QThread::msleep(TRACK_SEEK_LONG_DELAY);
    }

    // All iterations completed successfully
    return DiagnosticState::dsReadTrack40;
}

DiagnosticState DiagnosticThread::stateReadTrack40()
{
    emit diagnosticMessage(tr("Reading Track 40 (Upper and Lower surfaces)..."));

    // Step 1: Select Track 40
    if (ADFWriterManager::getInstance().selectTrack(TRACK_READ_TEST) != DiagnosticResponse::drOK) {
        emit diagnosticMessage(tr("ERROR: Failed to select Track 40: %1")
            .arg(translateError(QString::fromStdString(ADFWriterManager::getInstance().getLastError()))));
        m_overallSuccess = false;
        return DiagnosticState::dsError;
    }

    // Step 2: Read Upper Surface
    emit diagnosticMessage(tr("  Selecting Upper surface..."));
    if (ADFWriterManager::getInstance().selectSurface(DiskSurface::dsUpper) != DiagnosticResponse::drOK) {
        emit diagnosticMessage(tr("ERROR: Failed to select Upper surface: %1")
            .arg(translateError(QString::fromStdString(ADFWriterManager::getInstance().getLastError()))));
        m_overallSuccess = false;
        return DiagnosticState::dsError;
    }

    unsigned char trackData[RAW_TRACKDATA_LENGTH_DD];
    emit diagnosticMessage(tr("  Reading Upper track data..."));
    if (ADFWriterManager::getInstance().readCurrentTrack(trackData, RAW_TRACKDATA_LENGTH_DD, true)
        != DiagnosticResponse::drOK) {
        emit diagnosticMessage(tr("ERROR: Failed to read Upper track: %1")
            .arg(translateError(QString::fromStdString(ADFWriterManager::getInstance().getLastError()))));
        m_overallSuccess = false;
        return DiagnosticState::dsError;
    }
    emit diagnosticMessage(tr("  Upper track read successfully."));

    // Check for interruption before continuing
    if (isInterruptionRequested()) {
        emit diagnosticMessage(tr("Diagnostic interrupted during track read."));
        m_overallSuccess = false;
        return DiagnosticState::dsError;
    }

    QThread::msleep(SURFACE_READ_DELAY);

    // Step 3: Read Lower Surface
    emit diagnosticMessage(tr("  Selecting Lower surface..."));
    if (ADFWriterManager::getInstance().selectSurface(DiskSurface::dsLower) != DiagnosticResponse::drOK) {
        emit diagnosticMessage(tr("ERROR: Failed to select Lower surface: %1")
            .arg(translateError(QString::fromStdString(ADFWriterManager::getInstance().getLastError()))));
        m_overallSuccess = false;
        return DiagnosticState::dsError;
    }

    emit diagnosticMessage(tr("  Reading Lower track data..."));
    if (ADFWriterManager::getInstance().readCurrentTrack(trackData, RAW_TRACKDATA_LENGTH_DD, true)
        != DiagnosticResponse::drOK) {
        emit diagnosticMessage(tr("ERROR: Failed to read Lower track: %1")
            .arg(translateError(QString::fromStdString(ADFWriterManager::getInstance().getLastError()))));
        m_overallSuccess = false;
        return DiagnosticState::dsError;
    }
    emit diagnosticMessage(tr("  Lower track read successfully."));

    QThread::msleep(SURFACE_READ_DELAY);

    // Both surfaces read successfully
    return DiagnosticState::dsComplete;
}

DiagnosticState DiagnosticThread::stateComplete()
{
    emit diagnosticMessage(tr("Diagnostic tests completed."));

    if (m_overallSuccess) {
        emit diagnosticMessage(tr("\n=== DIAGNOSTIC TEST COMPLETED SUCCESSFULLY ==="));
    } else {
        emit diagnosticMessage(tr("\n=== DIAGNOSTIC TEST FAILED ==="));
    }

    // Transition to cleaning
    return DiagnosticState::dsCleaning;
}

DiagnosticState DiagnosticThread::stateCleaning()
{
    // Close device connection
    ADFWriterManager::getInstance().closeDevice();

    // Emit final completion signal
    emit diagnosticComplete(m_overallSuccess);

    // Return dsCleaning to signal loop termination
    return DiagnosticState::dsCleaning;
}
