#pragma once

// Test-only header for Linux builds
// Provides minimal definitions needed for unit testing

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <filesystem>
#include <functional>
#include <iostream>

// File signature constants
namespace FileSignatures {
    constexpr char CAB_SIGNATURE[] = "MSCF";
    constexpr char ZIP_SIGNATURE[] = "PK";  
    constexpr char SEVENZ_SIGNATURE[] = "7z";
    constexpr char GZIP_SIGNATURE[] = "\x1F\x8B";
    constexpr size_t SIGNATURE_SIZE = 8;
    constexpr size_t CAB_SIGNATURE_SIZE = 4;
    constexpr size_t ZIP_SIGNATURE_SIZE = 2;
    constexpr size_t SEVENZ_SIGNATURE_SIZE = 2;
    constexpr size_t GZIP_SIGNATURE_SIZE = 2;
}

// Simple logging utilities
namespace LogLevel {
    enum Level { INFO, WARNING, ERROR, DEBUG };
}

class SimpleLogger {
public:
    static void log(LogLevel::Level level, const std::string& message) {
        const char* levelStr = getLevelString(level);
        auto stream = (level == LogLevel::ERROR) ? &std::cerr : &std::cout;
        *stream << "[" << levelStr << "] " << message << std::endl;
    }
    
    static void info(const std::string& message) { log(LogLevel::INFO, message); }
    static void warning(const std::string& message) { log(LogLevel::WARNING, message); }
    static void error(const std::string& message) { log(LogLevel::ERROR, message); }
    static void debug(const std::string& message) { log(LogLevel::DEBUG, message); }

private:
    static const char* getLevelString(LogLevel::Level level) {
        switch (level) {
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::DEBUG: return "DEBUG";
            default: return "UNKNOWN";
        }
    }
};

// Minimal type definitions for testing
struct CabFileInfo {
    std::string filename;
    unsigned long size;
    unsigned long compressedSize;
    unsigned short attributes;
    unsigned long timestamp;
};

struct PsfPackageInfo {
    std::string packageName;
    std::string version;
    std::string architecture;
    std::string displayName;
    std::vector<std::string> dependencies;
    std::vector<std::string> components;
    bool isApplicable;
    std::string storeLocation;
};

struct WimImageInfo {
    int imageIndex;
    std::string imageName;
    std::string description;
    std::string architecture;
    std::string version;
    std::string displayName;
    std::string installationType;
    std::string defaultLanguage;
    bool bootable;
};

// Forward declarations for classes that aren't testable on Linux
class CbsIntegratedCabHandler;

// Minimal CabHandler class for testing
class CabHandler {
public:
    CabHandler();
    ~CabHandler();
    
    // Core functionality that can be tested
    bool extractCab(const std::string& cabPath, const std::string& destination);
    bool listCab(const std::string& cabPath, std::vector<CabFileInfo>& files);
    void setLastError(const std::string& error);
    std::string getLastError() const;
    
    // Stub methods for interface compatibility
    std::vector<PsfPackageInfo> listPsfPackages(const std::string& psfPath);
    bool extractPsfPackage(const std::string& psfPath, const std::string& destination);
    bool installPsfPackageOffline(const std::string& psfPath, const std::string& imagePath, 
                                 const std::string& logPath = "", bool quiet = false);
    bool installPsfPackageOnline(const std::string& psfPath, const std::string& logPath = "", bool quiet = false);
    std::vector<WimImageInfo> listWimImages(const std::string& wimPath);
    bool extractWimImage(const std::string& wimPath, int imageIndex, const std::string& destination);
    bool installWimImageOffline(const std::string& wimPath, int imageIndex, const std::string& imagePath,
                              const std::string& logPath = "", bool quiet = false);
    bool captureWimImage(const std::string& sourcePath, const std::string& wimPath, 
                        const std::string& imageName, const std::string& description = "", bool quiet = false);
    bool extractMsu(const std::string& msuPath, const std::string& destination);
    bool installMsuOffline(const std::string& msuPath, const std::string& imagePath,
                          const std::string& logPath = "", bool quiet = false);
    bool installFromExtractedMsuOffline(const std::string& extractedDir, const std::string& imagePath,
                                       const std::string& logPath = "", bool quiet = false);
    bool installFromExtractedMsuOnline(const std::string& extractedDir, const std::string& logPath = "", bool quiet = false);
    bool installMsuPackageOnline(const std::string& msuPath, const std::string& logPath = "", bool quiet = false);
    bool installCabPackageOnline(const std::string& cabPath, const std::string& logPath = "", bool quiet = false);
    void enableCbsIntegration();
    std::shared_ptr<CbsIntegratedCabHandler> getCbsHandler();

protected:
    bool extractCabImpl(const std::string& cabPath, const std::string& destination);
    
private:
    std::string lastError;
    bool cbsEnabled;
};