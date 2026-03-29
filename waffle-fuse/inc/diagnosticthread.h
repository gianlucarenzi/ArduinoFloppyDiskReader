#ifndef DIAGNOSTICTHREAD_H
#define DIAGNOSTICTHREAD_H

#include <QThread>
#include <QString>

// Diagnostic state machine states
enum class DiagnosticState {
    dsInitialize,          // Display prompt + open device
    dsWaitForDisk,         // Wait for write-protected disk (30s timeout)
    dsFirmwareVersion,     // Read firmware version
    dsTestCTS,             // Test CTS pin
    dsTestTransferSpeed,   // Test USB->Serial speed
    dsMeasureRPM,          // Measure RPM (290-310 range)
    dsTrackSeek,           // Track 0<->79 seek tests (2 iterations)
    dsReadTrack40,         // Read track 40 (upper + lower surfaces)
    dsComplete,            // Display completion message
    dsError,               // Error state
    dsCleaning             // Cleanup and close device
};

class DiagnosticThread : public QThread
{
    Q_OBJECT
public:
    DiagnosticThread();
    virtual void run();
    void setup(QString port);

signals:
    void diagnosticMessage(QString message);
    void diagnosticComplete(bool success);

private:
    // Port configuration
    QString m_port;

    // State machine variables
    DiagnosticState m_currentState;
    bool m_overallSuccess;

    // Configurable timing constants (in milliseconds)
    static constexpr int INITIAL_PROMPT_DELAY = 1000;        // Initial message display time
    static constexpr int DISK_WAIT_POLL_INTERVAL = 500;      // Poll every 0.5 seconds
    static constexpr int DISK_WAIT_TIMEOUT = 30000;          // 30 seconds timeout
    static constexpr int DISK_WAIT_STATUS_INTERVAL = 5000;   // Status message every 5 seconds
    static constexpr int INTER_STATE_DELAY = 500;            // Delay between states
    static constexpr int TRACK_SEEK_SHORT_DELAY = 1000;      // Delay after finding Track 0
    static constexpr int TRACK_SEEK_LONG_DELAY = 3000;       // Delay after selecting Track 79
    static constexpr int TRACK_SEEK_ITERATIONS = 2;          // Number of track seek iterations
    static constexpr int SURFACE_READ_DELAY = 500;           // Delay between reading surfaces

    // RPM validation range
    static constexpr float RPM_MIN_RANGE = 290.0f;
    static constexpr float RPM_MAX_RANGE = 310.0f;

    // Track parameters for seek tests
    static constexpr unsigned char TRACK_MIN = 0;
    static constexpr unsigned char TRACK_MAX = 79;
    static constexpr unsigned char TRACK_READ_TEST = 40;

    // State handler methods (each returns next state)
    DiagnosticState stateInitialize();
    DiagnosticState stateWaitForDisk();
    DiagnosticState stateFirmwareVersion();
    DiagnosticState stateTestCTS();
    DiagnosticState stateTestTransferSpeed();
    DiagnosticState stateMeasureRPM();
    DiagnosticState stateTrackSeek();
    DiagnosticState stateReadTrack40();
    DiagnosticState stateComplete();
    DiagnosticState stateCleaning();

    // Helper methods
    void transitionToError();                                // Transition to error state
    bool checkInterruption();                                // Check if user requested interruption
    QString translateError(const QString &error);            // Translate known error messages
};

#endif // DIAGNOSTICTHREAD_H
