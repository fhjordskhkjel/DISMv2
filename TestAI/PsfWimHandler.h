#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

// Forward declarations to avoid including Windows headers in the interface
class PsfWimHandlerImpl;
class WimgApiWrapper;

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

// WIM compression selection (kept for backward compatibility)
enum class WimCompression {
    None,
    Xpress,
    LZX,
    LZMS
};

// Progress callback for WIM operations
using WimProgressCallback = std::function<void(int messageType, uint64_t processedBytes, uint64_t totalBytes, const std::string& message)>;

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

    // WIM operations using wimgapi.dll (with progress callbacks and integrity verification)
    bool listWimImages(const std::string& wimPath, std::vector<WimImageInfo>& images);
    bool extractWimImage(const std::string& wimPath, int imageIndex, const std::string& destination, 
                        bool verifyIntegrity = false, bool preserveAcls = true, bool preserveTimestamps = true, 
                        bool preserveReparsePoints = true);
    bool applyWimImage(const std::string& wimPath, int imageIndex, const std::string& destination,
                      bool verifyIntegrity = false, bool preserveAcls = true, bool preserveTimestamps = true, 
                      bool preserveReparsePoints = true);
    bool captureWimImage(const std::string& sourcePath, const std::string& wimPath, 
                        const std::string& imageName, const std::string& description,
                        WimCompression compression = WimCompression::LZX, bool verifyIntegrity = false);

    // Progress callback support
    void setProgressCallback(WimProgressCallback callback);

    // Integrity verification
    bool verifyWimIntegrity(const std::string& wimPath);

    // Compression validation
    bool validateCompressionType(const std::string& wimPath, WimCompression compression);

    // Error handling
    std::string getLastError() const;

    // Static utility functions
    static bool detectPackageType(const std::string& packagePath, PackageType& type);
    
    // Check if WIMGAPI is available for native operations
    static bool isWimgapiAvailable();
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