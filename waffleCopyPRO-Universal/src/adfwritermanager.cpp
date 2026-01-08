#include <iostream>

// src/adfwritermanager.cpp
#include "adfwritermanager.h"
#include "lib/ADFWriter.h" // Include ADFWriter header
#include "lib/ArduinoInterface.h" // Include ArduinoInterface header if needed by ADFWriter methods
#include <string>
#include <functional>
#include <vector>
#include <QString> // For diagnostic messages if any are generated internally

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
    std::wcout << L"ADFWriterManager::openDevice: Attempting to open port = " << portName << std::endl;
    if (m_adfWriter.openDevice(portName)) {
        return ArduinoFloppyReader::DiagnosticResponse::drOK;
    } else {
        return ArduinoFloppyReader::DiagnosticResponse::drError;
    }
}

void ADFWriterManager::closeDevice() {
    m_adfWriter.closeDevice();
}

std::string ADFWriterManager::getLastError() {
    return m_adfWriter.getLastError();
}

const ArduinoFloppyReader::FirmwareVersion ADFWriterManager::getFirwareVersion() const {
    return m_adfWriter.getFirwareVersion();
}

DiagnosticResponse ADFWriterManager::getLastErrorCode() {
    return m_adfWriter.getLastErrorCode();
}

bool ADFWriterManager::runDiagnostics(const std::wstring& portName, std::function<void(bool isError, const std::string message)> messageOutput, std::function<bool(bool isQuestion, const std::string question)> askQuestion) {
    return m_adfWriter.runDiagnostics(portName, messageOutput, askQuestion);
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::checkDiskCapacity(bool& /*isHD*/) {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::setDiskCapacity(bool /*isHDMode*/) {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::checkForDisk(bool /*checkOnly*/) {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::testCTS() {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::testTransferSpeed() {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::checkIfDiskIsWriteProtected(bool /*checkOnly*/) {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::enableReading(bool /*enable*/, bool /*reset*/) {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::testIndexPulse() {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::measureDriveRPM(float& /*rpm*/) {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::guessPlusMode(bool& /*isPlus*/) {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::eeprom_IsDrawbridgePlusMode(bool& /*isPlusReally*/) {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::eeprom_SetDrawbridgePlusMode(bool /*setPlusMode*/) {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::findTrack0() {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::selectTrack(uint32_t /*track*/, ArduinoFloppyReader::TrackSearchSpeed /*speed*/) {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::testDataPulse() {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::selectSurface(ArduinoFloppyReader::DiskSurface /*surface*/) {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::readCurrentTrack(void* /*data*/, unsigned short /*dataLength*/, bool /*readRaw*/) {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::enableWriting(bool /*enable*/, bool /*reset*/) {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::eraseCurrentTrack() {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::DiagnosticResponse ADFWriterManager::eraseFluxOnTrack() {
    // Functionality not implemented in ADFWriterManager and cannot be delegated to ADFWriter.
    return ArduinoFloppyReader::DiagnosticResponse::drError;
}

ArduinoFloppyReader::ADFResult ADFWriterManager::ADFToDisk(const std::wstring& inputFile, bool mediaIsHD, bool verify, bool usePrecompMode, bool eraseFirst, bool writeFromIndex, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, bool isVerifyError, const ArduinoFloppyReader::CallbackOperation operation) > callback) {
    return m_adfWriter.ADFToDisk(inputFile, mediaIsHD, verify, usePrecompMode, eraseFirst, writeFromIndex, callback);
}

ArduinoFloppyReader::ADFResult ADFWriterManager::SCPToDisk(const std::wstring& inputFile, bool extraErases, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, bool isVerifyError, const ArduinoFloppyReader::CallbackOperation operation) > callback) {
    return m_adfWriter.SCPToDisk(inputFile, extraErases, callback);
}

ArduinoFloppyReader::ADFResult ADFWriterManager::IPFToDisk(const std::wstring& inputFile, bool extraErases, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, bool isVerifyError, const ArduinoFloppyReader::CallbackOperation operation) > callback) {
    return m_adfWriter.IPFToDisk(inputFile, extraErases, callback);
}

ArduinoFloppyReader::ADFResult ADFWriterManager::DiskToADF(const std::wstring& outputFile, bool hdMode, int numTracks, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, const int retryCounter, const int sectorsFound, const int badSectorsFound, const int totalSectors, const ArduinoFloppyReader::CallbackOperation operation)> callback) {
    return m_adfWriter.DiskToADF(outputFile, hdMode, numTracks, callback);
}

ArduinoFloppyReader::ADFResult ADFWriterManager::DiskToSCP(const std::wstring& outputFile, bool hdMode, int numTracks, unsigned char revolutions, std::function < ArduinoFloppyReader::WriteResponse(const int currentTrack, const ArduinoFloppyReader::DiskSurface currentSide, const int retryCounter, const int sectorsFound, const int badSectorsFound, const int totalSectors, const ArduinoFloppyReader::CallbackOperation)> callback, bool useNewFluxReader) {
    return m_adfWriter.DiskToSCP(outputFile, hdMode, numTracks, revolutions, callback, useNewFluxReader);
}

ArduinoFloppyReader::ADFResult ADFWriterManager::GuessDiskDensity(bool& isHD) {
    return m_adfWriter.GuessDiskDensity(isHD);
}

} // namespace ArduinoFloppyReader
