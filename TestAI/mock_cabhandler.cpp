// Mock implementation of CabHandler for Linux testing
// This provides stub implementations of Windows-specific functionality
// so that unit tests can run on Linux development systems

#include "CabHandler_test.h"
#include <iostream>
#include <fstream>
#include <cstring>

#ifdef LINUX_BUILD

// Mock implementations for Linux testing

CabHandler::CabHandler() : cbsEnabled(false) {
    // Initialize for testing
}

CabHandler::~CabHandler() = default;

bool CabHandler::extractCab(const std::string& cabPath, const std::string& destination) {
    return extractCabImpl(cabPath, destination);
}

bool CabHandler::extractCabImpl(const std::string& cabPath, const std::string& destination) {
    // Enhanced file signature checking with RAII
    char signature[FileSignatures::SIGNATURE_SIZE] = {0}; // Initialize to zero
    bool isStandardCab = false;
    bool isPossibleArchive = false;
    
    {
        std::ifstream cabFile(cabPath, std::ios::binary);
        if (!cabFile.is_open()) {
            setLastError("Cannot open CAB file for reading: " + cabPath);
            return false;
        }
        
        // Read and analyze file signature
        cabFile.read(signature, FileSignatures::SIGNATURE_SIZE);
        
        if (!cabFile.good() && !cabFile.eof()) {
            setLastError("Failed to read signature from CAB file: " + cabPath);
            return false;
        }
        
        // File is automatically closed when cabFile goes out of scope
    }
    
    isStandardCab = (memcmp(signature, FileSignatures::CAB_SIGNATURE, FileSignatures::CAB_SIGNATURE_SIZE) == 0);
    
    // Check for various archive signatures
    if (memcmp(signature, FileSignatures::ZIP_SIGNATURE, FileSignatures::ZIP_SIGNATURE_SIZE) == 0) {
        isPossibleArchive = true; // ZIP-based format
        SimpleLogger::info("Detected ZIP-based archive format");
    } else if (memcmp(signature, FileSignatures::SEVENZ_SIGNATURE, FileSignatures::SEVENZ_SIGNATURE_SIZE) == 0) {
        isPossibleArchive = true; // 7-Zip format
        SimpleLogger::info("Detected 7-Zip archive format");
    } else if (memcmp(signature, FileSignatures::GZIP_SIGNATURE, FileSignatures::GZIP_SIGNATURE_SIZE) == 0) {
        isPossibleArchive = true; // GZIP format
        SimpleLogger::info("Detected GZIP archive format");
    }
    
    if (!isStandardCab) {
        SimpleLogger::warning("Non-standard CAB signature detected");
    }

    // For testing purposes, return success if the file exists and destination is valid
    return std::filesystem::exists(cabPath) && std::filesystem::exists(destination);
}

bool CabHandler::listCab(const std::string& cabPath, std::vector<CabFileInfo>& files) {
    // Mock implementation - just return empty list
    files.clear();
    return std::filesystem::exists(cabPath);
}

void CabHandler::setLastError(const std::string& error) {
    lastError = error;
    SimpleLogger::error(error);
}

std::string CabHandler::getLastError() const {
    return lastError;
}

// Mock implementations for other methods that aren't needed for basic testing
std::vector<PsfPackageInfo> CabHandler::listPsfPackages(const std::string& /*psfPath*/) {
    return std::vector<PsfPackageInfo>();
}

bool CabHandler::extractPsfPackage(const std::string& /*psfPath*/, const std::string& /*destination*/) {
    return false; // Not implemented in mock
}

bool CabHandler::installPsfPackageOffline(const std::string& /*psfPath*/, const std::string& /*imagePath*/, 
                                         const std::string& /*logPath*/, bool /*quiet*/) {
    return false; // Not implemented in mock
}

bool CabHandler::installPsfPackageOnline(const std::string& /*psfPath*/, const std::string& /*logPath*/, bool /*quiet*/) {
    return false; // Not implemented in mock
}

std::vector<WimImageInfo> CabHandler::listWimImages(const std::string& /*wimPath*/) {
    return std::vector<WimImageInfo>();
}

bool CabHandler::extractWimImage(const std::string& /*wimPath*/, int /*imageIndex*/, const std::string& /*destination*/) {
    return false; // Not implemented in mock
}

bool CabHandler::installWimImageOffline(const std::string& /*wimPath*/, int /*imageIndex*/, const std::string& /*imagePath*/,
                                      const std::string& /*logPath*/, bool /*quiet*/) {
    return false; // Not implemented in mock
}

bool CabHandler::captureWimImage(const std::string& /*sourcePath*/, const std::string& /*wimPath*/, 
                                const std::string& /*imageName*/, const std::string& /*description*/, bool /*quiet*/) {
    return false; // Not implemented in mock
}

bool CabHandler::extractMsu(const std::string& /*msuPath*/, const std::string& /*destination*/) {
    return false; // Not implemented in mock
}

bool CabHandler::installMsuOffline(const std::string& /*msuPath*/, const std::string& /*imagePath*/,
                                  const std::string& /*logPath*/, bool /*quiet*/) {
    return false; // Not implemented in mock
}

bool CabHandler::installFromExtractedMsuOffline(const std::string& /*extractedDir*/, const std::string& /*imagePath*/,
                                               const std::string& /*logPath*/, bool /*quiet*/) {
    return false; // Not implemented in mock
}

bool CabHandler::installFromExtractedMsuOnline(const std::string& /*extractedDir*/, const std::string& /*logPath*/, bool /*quiet*/) {
    return false; // Not implemented in mock
}

bool CabHandler::installMsuPackageOnline(const std::string& /*msuPath*/, const std::string& /*logPath*/, bool /*quiet*/) {
    return false; // Not implemented in mock
}

bool CabHandler::installCabPackageOnline(const std::string& /*cabPath*/, const std::string& /*logPath*/, bool /*quiet*/) {
    return false; // Not implemented in mock
}

void CabHandler::enableCbsIntegration() {
    cbsEnabled = true;
}

std::shared_ptr<CbsIntegratedCabHandler> CabHandler::getCbsHandler() {
    return nullptr; // Not implemented in mock
}

#endif // LINUX_BUILD