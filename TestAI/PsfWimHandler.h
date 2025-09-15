#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

// Forward declarations to avoid including Windows headers in the interface
class PsfWimHandlerImpl;

// Package type enumeration
enum class PackageType {
    UNKNOWN,
    CAB,
    MSU,
    APPX_MSIX,
    WIM
};

// Forward declaration of WimImageInfo (defined in CabHandler.h)
struct WimImageInfo;

// WIM compression selection
enum class WimCompression {
    None,
    Xpress,
    LZX,
    LZMS
};

// Main PSF/WIM Handler class using proper Windows APIs
class PsfWimHandler {
private:
    std::unique_ptr<PsfWimHandlerImpl> impl;

public:
    PsfWimHandler();
    ~PsfWimHandler();

    // Disable copy construction and assignment
    PsfWimHandler(const PsfWimHandler&) = delete;
    PsfWimHandler& operator=(const PsfWimHandler&) = delete;

    // Enable move construction and assignment
    PsfWimHandler(PsfWimHandler&&) = default;
    PsfWimHandler& operator=(PsfWimHandler&&) = default;

    // Initialization and cleanup
    bool initialize();
    void cleanup();

    // PSF/APPX/MSIX operations using Windows AppxPackaging APIs
    bool extractPsfPackage(const std::string& packagePath, const std::string& destination);
    bool getPsfPackageInfo(const std::string& packagePath, std::string& packageName, 
                          std::string& version, std::string& architecture);

    // Optional online install/uninstall using Deployment API (fallback to PowerShell if unavailable)
    bool installAppxOnline(const std::string& packagePath, bool allUsers = false);
    bool uninstallAppxOnline(const std::string& packageFullName, bool allUsers = false);

    // WIM operations using wimgapi.dll (with progress callbacks)
    bool listWimImages(const std::string& wimPath, std::vector<WimImageInfo>& images);
    bool extractWimImage(const std::string& wimPath, int imageIndex, const std::string& destination);
    bool applyWimImage(const std::string& wimPath, int imageIndex, const std::string& destination);
    bool captureWimImage(const std::string& sourcePath, const std::string& wimPath, 
                        const std::string& imageName, const std::string& description,
                        WimCompression compression = WimCompression::LZX);

    // Error handling
    std::string getLastError() const;

    // Static utility functions
    static bool detectPackageType(const std::string& packagePath, PackageType& type);
};

// Utility namespace for PSF/WIM operations  
namespace PsfWimUtils {
    // Package type detection
    PackageType detectPackageType(const std::string& packagePath);
    
    // File signature checking
    bool isAppxPackage(const std::string& packagePath);
    bool isWimImage(const std::string& packagePath);
    
    // Logging utilities
    void logOperation(const std::string& operation, const std::string& details, 
                     const std::string& logPath = "");
}