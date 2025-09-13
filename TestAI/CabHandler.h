#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <filesystem>
#include <functional>
#include <fci.h>
#include <fdi.h>
#include <windows.h>

// Forward declarations
struct CabFileInfo {
    std::string filename;
    ULONG size;
    ULONG compressedSize;
    USHORT attributes;
    DWORD timestamp;
};

struct OperationContext {
    class CabHandler* handler;
    std::string destinationPath;
    bool listOnly;
    std::vector<CabFileInfo> fileList;
};

// ?? PSF and WIM structures
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
    ULONGLONG totalBytes;
};

// Simple forward declaration for CBS integration
class CbsIntegratedCabHandler;

// Forward declare CBS integration classes
class CbsManager;
struct CbsInstallResult;

class CabHandler {
public:
    CabHandler();
    ~CabHandler();

    // Core CAB operations
    bool extractCab(const std::string& cabPath, const std::string& destination);
    bool createCab(const std::string& cabPath, const std::string& source);
    bool addToCab(const std::string& cabPath, const std::string& source);
    std::vector<CabFileInfo> listCabContents(const std::string& cabPath);
    bool verifyCab(const std::string& cabPath);

    // ?? PSF (Package Store Format) operations
    bool extractPsf(const std::string& psfPath, const std::string& destination);
    bool listPsfContents(const std::string& psfPath, std::vector<PsfPackageInfo>& packages);
    bool installPsfPackage(const std::string& psfPath, const std::string& targetPath,
                          const std::string& logPath = "", bool quiet = false);
    bool installPsfPackageOnline(const std::string& psfPath, const std::string& logPath = "", bool quiet = false);
    bool verifyPsfPackage(const std::string& psfPath);
    
    // ?? WIM (Windows Imaging Format) operations
    bool extractWim(const std::string& wimPath, int imageIndex, const std::string& destination);
    bool listWimImages(const std::string& wimPath, std::vector<WimImageInfo>& images);
    bool installWimPackage(const std::string& wimPath, int imageIndex, const std::string& targetPath,
                          const std::string& logPath = "", bool quiet = false);
    bool applyWimImage(const std::string& wimPath, int imageIndex, const std::string& targetPath,
                      bool preserveAcl = true, bool quiet = false);
    bool captureWimImage(const std::string& sourcePath, const std::string& wimPath,
                        const std::string& imageName, const std::string& description = "", bool quiet = false);
    bool verifyWimFile(const std::string& wimPath);

    // DISM-like package installation functions
    bool installMsuPackage(const std::string& msuPath, const std::string& imagePath, 
                          const std::string& logPath = "", bool quiet = false);
    bool installCabPackage(const std::string& cabPath, const std::string& imagePath, 
                          const std::string& logPath = "", bool quiet = false);
    
    // NEW: Install from already extracted MSU directory
    bool installFromExtractedMsu(const std::string& extractedDir, const std::string& imagePath,
                                 const std::string& logPath = "", bool quiet = false);
    bool installFromExtractedMsuOnline(const std::string& extractedDir, const std::string& logPath = "", bool quiet = false);
    
    // Online system package installation (DISM /Online equivalent)
    bool installMsuPackageOnline(const std::string& msuPath, const std::string& logPath = "", bool quiet = false);
    bool installCabPackageOnline(const std::string& cabPath, const std::string& logPath = "", bool quiet = false);
    
    // ?? NEW: CBS-INTEGRATED INSTALLATION METHODS
    bool installPackageWithCbs(const std::string& packagePath, const std::string& targetPath,
                              const std::string& logPath = "", bool quiet = false);
    bool installExtractedPackageWithCbs(const std::string& extractedDir, const std::string& targetPath,
                                       const std::string& logPath = "", bool quiet = false);
    bool installPackageOnlineWithCbs(const std::string& packagePath, const std::string& logPath = "", 
                                    bool quiet = false);
    
    // ?? PSF/WIM CBS-INTEGRATED INSTALLATION
    bool installPsfPackageWithCbs(const std::string& psfPath, const std::string& targetPath,
                                 const std::string& logPath = "", bool quiet = false);
    bool installWimPackageWithCbs(const std::string& wimPath, int imageIndex, const std::string& targetPath,
                                 const std::string& logPath = "", bool quiet = false);
    
    // ?? CBS TRANSACTION MANAGEMENT
    bool beginCbsTransaction();
    bool commitCbsTransaction();
    bool rollbackCbsTransaction();
    
    // ?? CBS VALIDATION AND ANALYSIS
    bool validatePackageWithCbs(const std::string& packagePath, const std::string& targetPath = "");
    bool checkPackageApplicability(const std::string& packagePath, const std::string& targetPath = "");
    bool verifyPackageSignature(const std::string& packagePath);
    
    // ?? CBS INFORMATION RETRIEVAL
    std::vector<std::string> getPackageDependencies(const std::string& packagePath);
    std::vector<std::string> getInstalledComponents(const std::string& targetPath = "");
    bool checkDependencySatisfaction(const std::string& packagePath, const std::string& targetPath = "");
    
    // Enhanced MSU extraction methods
    bool extractMsuPackage(const std::string& msuPath, const std::string& destination, bool quiet = false);
    
    // ?? Enhanced extraction methods with PSF/WIM support
    bool extractPackageAdvanced(const std::string& packagePath, const std::string& destination, bool quiet = false);
    bool detectPackageType(const std::string& packagePath, std::string& detectedType);
    
    // ?? Enhanced Command Integration
    void printUniversalPackageInfo();
    
    // Advanced extraction methods
    bool extractMsuWithWusa(const std::string& msuPath, const std::string& destination);
    bool extractMsuDirect(const std::string& msuPath, const std::string& destination);
    bool extractMsuAsZip(const std::string& msuPath, const std::string& destination);
    bool extractMsuWithBinaryParsing(const std::string& msuPath, const std::string& destination);
    bool extractMsuWithPowerShell(const std::string& msuPath, const std::string& destination);
    bool extractMsuAlternative(const std::string& msuPath, const std::string& destination);
    
    // Additional enhanced extraction methods
    bool extractMsuWithModernAPI(const std::string& msuPath, const std::string& destination);
    bool extractMsuWithWinRAR(const std::string& msuPath, const std::string& destination);
    bool extractMsuWith7Zip(const std::string& msuPath, const std::string& destination);
    bool extractMsuWithPkUnzip(const std::string& msuPath, const std::string& destination);
    bool extractMsuWithUrlMon(const std::string& msuPath, const std::string& destination);
    bool extractMsuWithStreaming(const std::string& msuPath, const std::string& destination);
    
    // ?? PSF extraction methods
    bool extractPsfWithPowerShell(const std::string& psfPath, const std::string& destination);
    bool extractPsfWithDism(const std::string& psfPath, const std::string& destination);
    bool extractPsfDirect(const std::string& psfPath, const std::string& destination);
    
    // ?? WIM extraction methods
    bool extractWimWithDism(const std::string& wimPath, int imageIndex, const std::string& destination);
    bool extractWimWithWimApi(const std::string& wimPath, int imageIndex, const std::string& destination);
    bool extractWimWith7Zip(const std::string& wimPath, const std::string& destination);
    
    // File operations
    auto getFilesInDirectoryRange(const std::filesystem::path& directory, bool recursive = true) 
        -> std::vector<std::filesystem::path>;
    std::vector<std::string> getFilesInDirectory(const std::string& directory, bool recursive = false);
    
    // Modern optional-based error handling
    std::optional<std::string> getLastError() const noexcept { return lastError; }
    
    // Helper functions for online operations
    bool checkAdminPrivileges();
    bool enablePrivilege(const std::string& privilegeName);
    std::string getWindowsDirectory();
    
    // ?? CBS INTEGRATION GETTERS
    std::shared_ptr<CbsIntegratedCabHandler> getCbsHandler();
    bool isCbsEnabled() const { return cbsEnabled; }
    void enableCbsIntegration(bool enable = true) { cbsEnabled = enable; }
    
    // Additional helper methods for enhanced extraction
    bool executePowerShellScript(const std::string& script);
    bool executeCommand(const std::string& command, DWORD timeoutMs);
    void searchForCabSignatures(const std::vector<uint8_t>& buffer, size_t offset, std::vector<size_t>& cabOffsets);
    bool extractEmbeddedCabs(const std::string& msuPath, const std::string& destination, const std::vector<size_t>& cabOffsets);
    bool attemptBinaryExtraction(const std::string& filePath, const std::string& destination);

    // ?? PSF/WIM helper methods
    bool isPsfFile(const std::string& filePath);
    bool isWimFile(const std::string& filePath);
    bool analyzePsfPackage(const std::string& psfPath, PsfPackageInfo& packageInfo);
    bool analyzeWimFile(const std::string& wimPath, std::vector<WimImageInfo>& images);

private:
    std::optional<std::string> lastError;
    
    // ?? CBS INTEGRATION MEMBERS
    bool cbsEnabled;
    std::shared_ptr<CbsIntegratedCabHandler> cbsHandler;
    
    // Internal implementation methods
    bool extractCabImpl(const std::string& cabPath, const std::string& destination);
    bool createCabImpl(const std::string& cabPath, const std::string& source);
    bool addToCabImpl(const std::string& cabPath, const std::string& source);
    std::vector<CabFileInfo> listCabContentsImpl(const std::string& cabPath);
    bool verifyCabImpl(const std::string& cabPath);
    bool installMsuPackageImpl(const std::string& msuPath, const std::string& imagePath, 
                              const std::string& logPath, bool quiet);
    bool installCabPackageImpl(const std::string& cabPath, const std::string& imagePath, 
                              const std::string& logPath, bool quiet);
    bool installMsuPackageOnlineImpl(const std::string& msuPath, const std::string& logPath, bool quiet);
    bool installCabPackageOnlineImpl(const std::string& cabPath, const std::string& logPath, bool quiet);
    bool extractMsuPackageImpl(const std::string& msuPath, const std::string& destination, bool quiet);

    // ?? PSF/WIM internal implementation methods
    bool extractPsfImpl(const std::string& psfPath, const std::string& destination, bool quiet = false);
    bool extractWimImpl(const std::string& wimPath, int imageIndex, const std::string& destination, bool quiet = false);
    bool installPsfPackageImpl(const std::string& psfPath, const std::string& targetPath, 
                              const std::string& logPath, bool quiet);
    bool installWimPackageImpl(const std::string& wimPath, int imageIndex, const std::string& targetPath,
                              const std::string& logPath, bool quiet);
    bool installPsfPackageOnlineImpl(const std::string& psfPath, const std::string& logPath, bool quiet);
    
    // ?? DISM-like integration methods
    bool postProcessExtractedUpdate(const std::string& extractedDir, bool quiet = false);
    bool integrateExtractedComponents(const std::string& stagingDir, const std::string& targetDir, bool quiet = false);
    
    // Error handling
    void setLastError(const std::string& error);
    
    template<typename... Args>
    void setLastErrorFmt(const std::string& fmt, Args&&... args);
    
    // Helper functions with std::filesystem
    bool createDirectoryRecursive(const std::filesystem::path& path);
    bool createDirectoryRecursive(const std::string& path);
    bool removeDirectoryRecursive(const std::filesystem::path& path);
    bool removeDirectoryRecursive(const std::string& path);
    bool copyDirectoryRecursive(const std::filesystem::path& source, const std::filesystem::path& destination);
    bool copyDirectoryRecursive(const std::string& source, const std::string& destination);
    
    std::filesystem::path getDirectoryFromPath(const std::filesystem::path& path);
    std::string getFilenameFromPath(const std::filesystem::path& path);
    std::filesystem::path getRelativePath(const std::filesystem::path& fullPath, const std::filesystem::path& basePath);
    
    // Legacy string-based helpers for compatibility
    std::string getDirectoryFromPathStr(const std::string& path);
    std::string getRelativePathStr(const std::string& fullPath, const std::string& basePath);

    // Static callback functions for FDI/FCI
    static INT_PTR DIAMONDAPI fdiNotify(FDINOTIFICATIONTYPE fdint, PFDINOTIFICATION pfdin);
    
    // FCI callback implementations
    static BOOL DIAMONDAPI fciGetNextCab(PCCAB pccab, ULONG cbPrevCab, void* pv);
    static int DIAMONDAPI fciFilePlaced(PCCAB pccab, char* pszFile, LONG cbFile, BOOL fContinuation, void* pv);
    static void* DIAMONDAPI fciAlloc(ULONG cb);
    static void DIAMONDAPI fciFree(void* memory);
    static INT_PTR DIAMONDAPI fciOpen(char* pszFile, int oflag, int pmode, int* err, void* pv);
    static UINT DIAMONDAPI fciRead(INT_PTR hf, void* memory, UINT cb, int* err, void* pv);
    static UINT DIAMONDAPI fciWrite(INT_PTR hf, void* memory, UINT cb, int* err, void* pv);
    static int DIAMONDAPI fciClose(INT_PTR hf, int* err, void* pv);
    static LONG DIAMONDAPI fciSeek(INT_PTR hf, LONG dist, int seektype, int* err, void* pv);
    static int DIAMONDAPI fciDelete(char* pszFile, int* err, void* pv);
    static BOOL DIAMONDAPI fciGetTempFile(char* pszTempName, int cbTempName, void* pv);

    // FDI callback implementations
    static void* DIAMONDAPI fdiAlloc(ULONG cb);
    static void DIAMONDAPI fdiFree(void* pv);
    static INT_PTR DIAMONDAPI fdiOpen(char* pszFile, int oflag, int pmode);
    static UINT DIAMONDAPI fdiRead(INT_PTR hf, void* pv, UINT cb);
    static UINT DIAMONDAPI fdiWrite(INT_PTR hf, void* pv, UINT cb);
    static int DIAMONDAPI fdiClose(INT_PTR hf);
    static LONG DIAMONDAPI fdiSeek(INT_PTR hf, LONG dist, int seektype);
};