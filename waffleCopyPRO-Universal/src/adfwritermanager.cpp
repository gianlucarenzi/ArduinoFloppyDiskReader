// src/adfwritermanager.cpp
#include "adfwritermanager.h"
#include "lib/ADFWriter.h"
#include "lib/ArduinoInterface.h"
#include "debugmsg.h"
#include <string>
#include <functional>
#include <vector>
#include <QString>

// Initialize the singleton instance pointer
ArduinoFloppyReader::ADFWriterManager* ArduinoFloppyReader::ADFWriterManager::instance = nullptr;

namespace ArduinoFloppyReader {

// Private constructor
ADFWriterManager::ADFWriterManager() {
    // The ADFWriter instance m_adfWriter is default-constructed.
    // If ADFWriter has specific initialization needs upon creation, they should be handled here or within ADFWriter itself.
}

// Protected destructor
ADFWriterManager::~ADFWriterManager() {
    // Ensure the ADFWriter is closed if it was opened.
    // This might be tricky if openDevice was called and the manager is destroyed before closeDevice.
    // For now, we assume the user of the manager will call closeDevice appropriately if needed.
    // If m_adfWriter was dynamically allocated, it would be deleted here.
}

// Private copy constructor and assignment operator
ADFWriterManager::ADFWriterManager(const ADFWriterManager&) {
    // Prevent copying
}

ADFWriterManager& ADFWriterManager::operator=(const ADFWriterManager&) {
    // Prevent assignment
    return *this;
}

// Singleton access method
ADFWriterManager& ADFWriterManager::getInstance() {
    if (instance == nullptr) {
        instance = new ADFWriterManager();
    }
    return *instance;
}

// --- Implementations of forwarded methods ---

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::openDevice(const std::wstring& portName) {
    DEBUGMSG("ENTER");
    if (m_adfWriter.openDevice(portName)) {
        DEBUGMSG("OK");
        return ArduinoFloppyReader::DiagnosticResponse::drOK;
    }
    DEBUGMSG("ERROR");
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

void ADFWriterManager::closeDevice() {
    DEBUGMSG("ENTER");
    m_adfWriter.closeDevice();
}

std::string ADFWriterManager::getLastError() {
    std::string err = m_adfWriter.getLastError();
    DEBUGMSG("error=%s", err.c_str());
    return err;
}

const ArduinoFloppyReader::FirmwareVersion ADFWriterManager::getFirwareVersion() const {
    DEBUGMSG("ENTER");
    return m_adfWriter.getFirwareVersion();
}

DiagnosticResponse ADFWriterManager::getLastErrorCode() {
    DiagnosticResponse r = m_adfWriter.getLastErrorCode();
    DEBUGMSG("result=%d", (int)r);
    return r;
}

bool ADFWriterManager::runDiagnostics(const std::wstring& portName, std::function<void(bool isError, const std::string message)> messageOutput, std::function<bool(bool isQuestion, const std::string question)> askQuestion) {
    DEBUGMSG("ENTER");
    bool r = m_adfWriter.runDiagnostics(portName, messageOutput, askQuestion);
    DEBUGMSG("result=%d", (int)r);
    return r;
}

ADFResult ADFWriterManager::ADFToDisk(const std::wstring& inputFile, bool mediaIsHD, bool verify, bool usePrecompMode, bool eraseFirst, bool writeFromIndex, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, bool isVerifyError, const ArduinoFloppyReader::CallbackOperation operation) > callback) {
    DEBUGMSG("ENTER hdMode=%d verify=%d preComp=%d eraseFirst=%d", mediaIsHD, verify, usePrecompMode, eraseFirst);
    ADFResult r = m_adfWriter.ADFToDisk(inputFile, mediaIsHD, verify, usePrecompMode, eraseFirst, writeFromIndex, callback);
    DEBUGMSG("result=%d", (int)r);
    return r;
}

ADFResult ADFWriterManager::SCPToDisk(const std::wstring& inputFile, bool extraErases, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, bool isVerifyError, const ArduinoFloppyReader::CallbackOperation operation) > callback) {
    DEBUGMSG("ENTER extraErases=%d", extraErases);
    ADFResult r = m_adfWriter.SCPToDisk(inputFile, extraErases, callback);
    DEBUGMSG("result=%d", (int)r);
    return r;
}

ADFResult ADFWriterManager::IPFToDisk(const std::wstring& inputFile, bool extraErases, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, bool isVerifyError, const ArduinoFloppyReader::CallbackOperation operation) > callback) {
    DEBUGMSG("ENTER extraErases=%d", extraErases);
    ADFResult r = m_adfWriter.IPFToDisk(inputFile, extraErases, callback);
    DEBUGMSG("result=%d", (int)r);
    return r;
}

ADFResult ADFWriterManager::DiskToADF(const std::wstring& outputFile, bool hdMode, int numTracks, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, const int retryCounter, const int sectorsFound, const int badSectorsFound, const int totalSectors, const ArduinoFloppyReader::CallbackOperation operation)> callback) {
    DEBUGMSG("ENTER hdMode=%d numTracks=%d", hdMode, numTracks);
    ADFResult r = m_adfWriter.DiskToADF(outputFile, hdMode, numTracks, callback);
    DEBUGMSG("result=%d", (int)r);
    return r;
}

ADFResult ADFWriterManager::DiskToSCP(const std::wstring& outputFile, bool hdMode, int numTracks, unsigned char revolutions, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, const int retryCounter, const int sectorsFound, const int badSectorsFound, const int totalSectors, const ArduinoFloppyReader::CallbackOperation)> callback, bool useNewFluxReader) {
    DEBUGMSG("ENTER hdMode=%d numTracks=%d revolutions=%d newFlux=%d", hdMode, numTracks, revolutions, useNewFluxReader);
    ADFResult r = m_adfWriter.DiskToSCP(outputFile, hdMode, numTracks, revolutions, callback, useNewFluxReader);
    DEBUGMSG("result=%d", (int)r);
    return r;
}

ADFResult ADFWriterManager::sectorFileToDisk(const std::wstring& inputFile, bool inHDMode, bool verify, bool usePrecompMode, bool eraseFirst, bool useAtariSTTiming, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, bool isVerifyError, const ArduinoFloppyReader::CallbackOperation operation) > callback) {
    DEBUGMSG("ENTER hdMode=%d verify=%d preComp=%d eraseFirst=%d atariST=%d", inHDMode, verify, usePrecompMode, eraseFirst, useAtariSTTiming);
    ADFResult r = m_adfWriter.sectorFileToDisk(inputFile, inHDMode, verify, usePrecompMode, eraseFirst, useAtariSTTiming, callback);
    DEBUGMSG("result=%d", (int)r);
    return r;
}

ADFResult ADFWriterManager::diskToIBMST(const std::wstring& outputFile, bool inHDMode, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, const int retryCounter, const int sectorsFound, const int badSectorsFound, const int maxSectors, const ArduinoFloppyReader::CallbackOperation operation)> callback) {
    DEBUGMSG("ENTER hdMode=%d", inHDMode);
    ADFResult r = m_adfWriter.diskToIBMST(outputFile, inHDMode, callback);
    DEBUGMSG("result=%d", (int)r);
    return r;
}

ADFResult ADFWriterManager::GuessDiskDensity(bool& isHD) {
    DEBUGMSG("ENTER");
    ADFResult r = m_adfWriter.GuessDiskDensity(isHD);
    DEBUGMSG("result=%d isHD=%d", (int)r, isHD);
    return r;
}

// Implementations for exposed ArduinoInterface diagnostic methods
DiagnosticResponse ADFWriterManager::checkIfDiskIsWriteProtected(bool forceCheck) {
    DEBUGMSG("ENTER forceCheck=%d", forceCheck);
    DiagnosticResponse r = m_adfWriter.checkIfDiskIsWriteProtected(forceCheck);
    DEBUGMSG("result=%d", (int)r);
    return r;
}

bool ADFWriterManager::isDiskInDrive() {
    bool r = m_adfWriter.isDiskInDrive();
    DEBUGMSG("result=%d", r);
    return r;
}

DiagnosticResponse ADFWriterManager::testCTS() {
    DEBUGMSG("ENTER");
    DiagnosticResponse r = m_adfWriter.testCTS();
    DEBUGMSG("result=%d", (int)r);
    return r;
}

DiagnosticResponse ADFWriterManager::testTransferSpeed() {
    DEBUGMSG("ENTER");
    DiagnosticResponse r = m_adfWriter.testTransferSpeed();
    DEBUGMSG("result=%d", (int)r);
    return r;
}

DiagnosticResponse ADFWriterManager::testIndexPulse() {
    DEBUGMSG("ENTER");
    DiagnosticResponse r = m_adfWriter.testIndexPulse();
    DEBUGMSG("result=%d", (int)r);
    return r;
}

DiagnosticResponse ADFWriterManager::measureDriveRPM(float& rpm) {
    DEBUGMSG("ENTER");
    DiagnosticResponse r = m_adfWriter.measureDriveRPM(rpm);
    DEBUGMSG("result=%d rpm=%.2f", (int)r, rpm);
    return r;
}

DiagnosticResponse ADFWriterManager::findTrack0() {
    DEBUGMSG("ENTER");
    DiagnosticResponse r = m_adfWriter.findTrack0();
    DEBUGMSG("result=%d", (int)r);
    return r;
}

DiagnosticResponse ADFWriterManager::selectTrack(unsigned char trackIndex, ArduinoFloppyReader::TrackSearchSpeed searchSpeed) {
    DEBUGMSG("track=%d speed=%d", trackIndex, (int)searchSpeed);
    DiagnosticResponse r = m_adfWriter.selectTrack(trackIndex, searchSpeed);
    DEBUGMSG("result=%d", (int)r);
    return r;
}

DiagnosticResponse ADFWriterManager::selectSurface(ArduinoFloppyReader::DiskSurface side) {
    DEBUGMSG("side=%d", (int)side);
    DiagnosticResponse r = m_adfWriter.selectSurface(side);
    DEBUGMSG("result=%d", (int)r);
    return r;
}

DiagnosticResponse ADFWriterManager::readCurrentTrack(void* trackData, const int dataLength, const bool readFromIndexPulse) {
    DEBUGMSG("dataLength=%d fromIndex=%d", dataLength, readFromIndexPulse);
    DiagnosticResponse r = m_adfWriter.readCurrentTrack(trackData, dataLength, readFromIndexPulse);
    DEBUGMSG("result=%d", (int)r);
    return r;
}

DiagnosticResponse ADFWriterManager::enableReading(bool enable, bool reset, bool dontWait) {
    DEBUGMSG("enable=%d reset=%d dontWait=%d", enable, reset, dontWait);
    DiagnosticResponse r = m_adfWriter.enableReading(enable, reset, dontWait);
    DEBUGMSG("result=%d", (int)r);
    return r;
}

} // namespace ArduinoFloppyReader
