// src/adfwritermanager.h
#pragma once

#include <string>
#include <functional>
#include <vector>
#include <QString> // For diagnostic messages if any are generated internally

// Forward declarations for types used in ADFWriter
namespace ArduinoFloppyReader {
    enum class DiagnosticResponse;
    enum class ADFResult;
    enum class DiskSurface;
    enum class CallbackOperation;
    enum class TrackSearchSpeed; // Added for selectTrack overload
    struct FirmwareVersion;
    // class ArduinoInterface; // Not directly needed by the manager interface, but ADFWriter uses it.
}

// Include the ADFWriter header from lib
#include "../lib/ADFWriter.h"

namespace ArduinoFloppyReader {

// Forward declaration of ADFWriterManager itself for the getInstance() return type
class ADFWriterManager;

// Manager class to provide a single instance of ADFWriter
class ADFWriterManager {
private:
    // The single instance of ADFWriter
    ArduinoFloppyReader::ADFWriter m_adfWriter;

    // Singleton instance of the manager
    static ADFWriterManager* instance;

    // Private constructor, destructor, copy constructor, and assignment operator
    ADFWriterManager();
    ~ADFWriterManager();
    ADFWriterManager(const ADFWriterManager&);
    ADFWriterManager& operator=(const ADFWriterManager&);

public:
    // Singleton access method
    static ADFWriterManager& getInstance();

    // --- Methods to forward from ADFWriter ---
    // These methods are based on the analysis of usage in diagnosticthread.cpp and drawbridge.cpp

    // From openDevice
    ArduinoFloppyReader::DiagnosticResponse openDevice(const std::wstring& portName);
    // From closeDevice
    void closeDevice();
    // From getLastError
    std::string getLastError();
    DiagnosticResponse getLastErrorCode();
    // From getFirwareVersion
    const ArduinoFloppyReader::FirmwareVersion getFirwareVersion() const;

    // From runDiagnostics
    bool runDiagnostics(const std::wstring& portName, std::function<void(bool isError, const std::string message)> messageOutput, std::function<bool(bool isQuestion, const std::string question)> askQuestion);



    // From ADFToDisk
    ArduinoFloppyReader::ADFResult ADFToDisk(const std::wstring& inputFile, bool mediaIsHD, bool verify, bool usePrecompMode, bool eraseFirst, bool writeFromIndex, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, bool isVerifyError, const ArduinoFloppyReader::CallbackOperation operation) > callback);
    // From SCPToDisk
    ArduinoFloppyReader::ADFResult SCPToDisk(const std::wstring& inputFile, bool extraErases, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, bool isVerifyError, const ArduinoFloppyReader::CallbackOperation operation) > callback);
    // From IPFToDisk
    ArduinoFloppyReader::ADFResult IPFToDisk(const std::wstring& inputFile, bool extraErases, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, bool isVerifyError, const ArduinoFloppyReader::CallbackOperation operation) > callback);
    // From DiskToADF
    ArduinoFloppyReader::ADFResult DiskToADF(const std::wstring& outputFile, bool hdMode, int numTracks, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, const int retryCounter, const int sectorsFound, const int badSectorsFound, const int totalSectors, const ArduinoFloppyReader::CallbackOperation operation)> callback);
    // From DiskToSCP
    ArduinoFloppyReader::ADFResult DiskToSCP(const std::wstring& outputFile, bool hdMode, int numTracks, unsigned char revolutions, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, const int retryCounter, const int sectorsFound, const int badSectorsFound, const int totalSectors, const ArduinoFloppyReader::CallbackOperation)> callback, bool useNewFluxReader = false);
    // From sectorFileToDisk (writes IMG/IMA/ST to floppy)
    ArduinoFloppyReader::ADFResult sectorFileToDisk(const std::wstring& inputFile, bool inHDMode, bool verify, bool usePrecompMode, bool eraseFirst, bool useAtariSTTiming, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, bool isVerifyError, const ArduinoFloppyReader::CallbackOperation operation) > callback);
    // From diskToIBMST (reads PC/Atari ST floppy to IMG/IMA/ST file)
    ArduinoFloppyReader::ADFResult diskToIBMST(const std::wstring& outputFile, bool inHDMode, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, const int retryCounter, const int sectorsFound, const int badSectorsFound, const int maxSectors, const ArduinoFloppyReader::CallbackOperation operation)> callback);
    // From GuessDiskDensity
    ArduinoFloppyReader::ADFResult GuessDiskDensity(bool& isHD);

    // Exposing ArduinoInterface diagnostic methods
    DiagnosticResponse checkIfDiskIsWriteProtected(bool forceCheck);
    bool isDiskInDrive();
    DiagnosticResponse testCTS();
    DiagnosticResponse testTransferSpeed();
    DiagnosticResponse testIndexPulse();
    DiagnosticResponse measureDriveRPM(float& rpm);
    DiagnosticResponse findTrack0();
    DiagnosticResponse selectTrack(unsigned char trackIndex, ArduinoFloppyReader::TrackSearchSpeed searchSpeed = ArduinoFloppyReader::TrackSearchSpeed::tssNormal);
    DiagnosticResponse selectSurface(ArduinoFloppyReader::DiskSurface side);
    DiagnosticResponse readCurrentTrack(void* trackData, const int dataLength, const bool readFromIndexPulse);
    DiagnosticResponse enableReading(bool enable, bool reset = true, bool dontWait = false);
};

} // namespace ArduinoFloppyReader
