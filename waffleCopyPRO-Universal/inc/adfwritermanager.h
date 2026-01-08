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
    // From getFirwareVersion
    const ArduinoFloppyReader::FirmwareVersion getFirwareVersion() const;

    // From runDiagnostics
    bool runDiagnostics(const std::wstring& portName, std::function<void(bool isError, const std::string message)> messageOutput, std::function<bool(bool isQuestion, const std::string question)> askQuestion);

    // From checkDiskCapacity
    ArduinoFloppyReader::DiagnosticResponse checkDiskCapacity(bool& isHD);
    // From setDiskCapacity
    ArduinoFloppyReader::DiagnosticResponse setDiskCapacity(bool isHDMode);
    // From checkForDisk
    ArduinoFloppyReader::DiagnosticResponse checkForDisk(bool checkOnly);
    // From testCTS
    ArduinoFloppyReader::DiagnosticResponse testCTS();
    // From testTransferSpeed
    ArduinoFloppyReader::DiagnosticResponse testTransferSpeed();
    // From checkIfDiskIsWriteProtected
    ArduinoFloppyReader::DiagnosticResponse checkIfDiskIsWriteProtected(bool checkOnly);
    // From enableReading
    ArduinoFloppyReader::DiagnosticResponse enableReading(bool enable, bool reset);
    // From testIndexPulse
    ArduinoFloppyReader::DiagnosticResponse testIndexPulse();
    // From measureDriveRPM
    ArduinoFloppyReader::DiagnosticResponse measureDriveRPM(float& rpm);
    // From guessPlusMode
    ArduinoFloppyReader::DiagnosticResponse guessPlusMode(bool& isPlus);
    // From eeprom_IsDrawbridgePlusMode
    ArduinoFloppyReader::DiagnosticResponse eeprom_IsDrawbridgePlusMode(bool& isPlusReally);
    // From eeprom_SetDrawbridgePlusMode
    ArduinoFloppyReader::DiagnosticResponse eeprom_SetDrawbridgePlusMode(bool setPlusMode);
    // From findTrack0
    ArduinoFloppyReader::DiagnosticResponse findTrack0();
    // From selectTrack
    ArduinoFloppyReader::DiagnosticResponse selectTrack(uint32_t track, ArduinoFloppyReader::TrackSearchSpeed speed = ArduinoFloppyReader::TrackSearchSpeed::tssNormal);
    // From testDataPulse
    ArduinoFloppyReader::DiagnosticResponse testDataPulse();
    // From selectSurface
    ArduinoFloppyReader::DiagnosticResponse selectSurface(ArduinoFloppyReader::DiskSurface surface);
    // From readCurrentTrack
    ArduinoFloppyReader::DiagnosticResponse readCurrentTrack(void* data, unsigned short dataLength, bool readRaw);
    // From enableWriting
    ArduinoFloppyReader::DiagnosticResponse enableWriting(bool enable, bool reset);
    // From eraseCurrentTrack
    ArduinoFloppyReader::DiagnosticResponse eraseCurrentTrack();
    // From eraseFluxOnTrack
    ArduinoFloppyReader::DiagnosticResponse eraseFluxOnTrack();

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
    // From GuessDiskDensity
    ArduinoFloppyReader::ADFResult GuessDiskDensity(bool& isHD);
};

} // namespace ArduinoFloppyReader
