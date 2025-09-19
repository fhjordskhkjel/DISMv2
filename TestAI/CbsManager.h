#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <windows.h>
#include <mutex>

// COM and CBS related includes
#include <comdef.h>
#include <atlbase.h>
#include <objbase.h>

// Helper for external tool timeout override
inline DWORD ExternalTimeoutMs(DWORD defaultMs) {
    char buf[32] = {};
    DWORD n = GetEnvironmentVariableA("DISMV2_TIMEOUT_MS", buf, static_cast<DWORD>(sizeof(buf)));
    if (n > 0 && n < sizeof(buf)) {
        char* end = nullptr; unsigned long v = strtoul(buf, &end, 10);
        if (v > 0 && v < 0xFFFFFFFFul) return static_cast<DWORD>(v);
    }
    return defaultMs;
}

// Operation statistics for last run
struct OperationStats {
    size_t copied = 0;
    size_t skipped = 0;
    size_t failed = 0;
};

// CBS Integration Structures
struct CbsComponentInfo {
    std::string identity;
    std::string version;
    std::string architecture;
    std::string publicKeyToken;
    std::string processorArchitecture;
    std::vector<std::string> dependencies;
    std::string state; // "staged", "installed", "absent", etc.
    bool isApplicable;
    bool needsRestart;
};

struct CbsPackageInfo {
    std::string packageIdentity;
    std::string displayName;
    std::string description;
    std::string version;
    std::string releaseType;
    std::string restartRequired;
    std::vector<CbsComponentInfo> components;
    std::vector<std::string> applicabilityInfo;
    std::string installState;
};

struct CbsInstallResult {
    bool success;
    std::string errorDescription;
    HRESULT errorCode;
    bool needsRestart;
    std::vector<std::string> installedComponents;
    std::vector<std::string> failedComponents;
    std::string logPath;
};

// CBS Transaction States
enum class CbsTransactionState {
    None,
    Initiated,
    Staged,
    Committed,
    Aborted,
    Failed
};

/**
 * Component-Based Servicing (CBS) Integration Manager
 * 
 * This class provides proper Windows CBS integration for package installation,
 * mimicking DISM's functionality for component registration, dependency resolution,
 * and transaction management.
 */
class CbsManager {
public:
    CbsManager();
    ~CbsManager();

    // Initialization and cleanup
    bool initialize();
    void cleanup();

    // Verbose logging
    void setVerbose(bool v) { verbose = v; }

    // Optional behaviors
    void setOfflineImagePath(const std::string& imagePath) { offlineImagePath = imagePath; }
    void setAllowPowershellFallback(bool allow) { allowPowershellFallback = allow; }
    void setAllowWusaFallback(bool allow) { allowWusaFallback = allow; }
    void setAllow7zFallback(bool allow) { allow7zFallback = allow; }
    void setAllowCatalogRegistration(bool allow) { allowCatalogRegistration = allow; }

    // CBS Package Analysis
    std::optional<CbsPackageInfo> analyzePackage(const std::string& packagePath);
    std::optional<CbsPackageInfo> analyzeExtractedPackage(const std::string& extractedDir);
    
    // Dependency Management
    bool validateDependencies(const CbsPackageInfo& packageInfo);
    std::vector<std::string> resolveDependencies(const CbsPackageInfo& packageInfo);
    bool checkConflicts(const CbsPackageInfo& packageInfo);
    
    // Component Registration
    bool registerComponents(const std::vector<CbsComponentInfo>& components);
    bool unregisterComponents(const std::vector<CbsComponentInfo>& components);
    
    // CBS Transaction Management
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    CbsTransactionState getTransactionState() const { return transactionState; }
    
    // Package Installation with CBS Integration
    CbsInstallResult installPackageWithCbs(const std::string& packagePath, 
                                          const std::string& targetPath,
                                          bool isOnline = false);
    
    CbsInstallResult installExtractedPackageWithCbs(const std::string& extractedDir,
                                                   const std::string& targetPath,
                                                   bool isOnline = false);
    
    // Manifest Processing
    bool processManifestFiles(const std::vector<std::string>& manifestPaths, 
                            const std::string& targetPath);
    
    // System Integration
    bool integrateCbsStore(const std::string& targetPath);
    bool updateComponentStore(const std::string& targetPath);
    bool notifyServicingStack(const std::vector<std::string>& installedComponents);
    
    // Signature Verification
    bool verifyPackageSignature(const std::string& packagePath);
    bool verifyComponentSignature(const std::string& componentPath);
    
    // System File Protection Integration
    bool disableWrp();
    bool enableWrp();
    bool bypassWrpForInstall(const std::vector<std::string>& filePaths);
    
    // Applicability Checking
    bool checkApplicability(const CbsPackageInfo& packageInfo, const std::string& targetSystem);
    std::vector<std::string> getApplicabilityFailures(const CbsPackageInfo& packageInfo);
    
    // Error Handling and Logging
    std::optional<std::string> getLastError() const { return lastError; }
    std::string getDetailedErrorLog() const { return errorLog; }
    bool enableCbsLogging(const std::string& logPath);

    // Operation stats
    OperationStats getLastOperationStats() const { return lastStats; }
    
    // Utility Functions
    bool isSystemOnline() const { return systemOnline; }
    void setSystemOnline(bool online) { systemOnline = online; }
    std::string getCbsLogPath() const;
    
    // Component Enumeration
    std::vector<CbsComponentInfo> enumerateInstalledComponents(const std::string& targetPath);
    std::vector<std::string> getComponentDependencies(const std::string& componentIdentity);

    // Public utilities
    bool extractMsuTo(const std::string& msuPath, const std::string& destination) { return extractMsuForAnalysis(msuPath, destination); }
    bool extractCabTo(const std::string& cabPath, const std::string& destination) { return extractCabForAnalysis(cabPath, destination); }

private:
    // Internal state
    std::optional<std::string> lastError;
    std::string errorLog;
    bool initialized;
    bool systemOnline;
    bool verbose = false;
    bool allowPowershellFallback = true;
    bool allowWusaFallback = true;
    bool allow7zFallback = true;
    bool allowCatalogRegistration = true;
    std::string offlineImagePath;
    CbsTransactionState transactionState;
    std::optional<std::string> logFilePath;
    mutable std::mutex errorLogMutex;
    OperationStats lastStats{};

    // Track reboot requirement when scheduling delayed replacements
    bool rebootRequired = false;

    // COM interfaces for CBS
    CComPtr<IUnknown> cbsSession;
    CComPtr<IUnknown> cbsStore;
    
    // Private helper methods
    bool initializeCom();
    void cleanupCom();
    
    bool loadCbsApi();
    void unloadCbsApi();
    
    // Enhanced package extraction methods
    bool extractCabForAnalysis(const std::string& cabPath, const std::string& destination);
    bool extractMsuForAnalysis(const std::string& msuPath, const std::string& destination);
    bool extractGenericPackageForAnalysis(const std::string& packagePath, const std::string& destination);
    bool installExtractedFiles(const std::string& extractedDir, const std::string& targetPath, bool isOnline);

    // Manifest parsing
    std::optional<CbsComponentInfo> parseComponentManifest(const std::string& manifestPath);
    std::optional<CbsPackageInfo> parsePackageManifest(const std::string& manifestPath);
    
    // XML processing helpers
    bool parseXmlManifest(const std::string& xmlPath, CbsPackageInfo& packageInfo);
    bool parseMumManifest(const std::string& mumPath, CbsComponentInfo& componentInfo);
    
    // CBS Store operations
    bool createCbsSession(const std::string& targetPath);
    void closeCbsSession();
    
    // File system integration
    bool createStagingDirectory(const std::string& basePath, std::string& stagingPath);
    bool cleanupStagingDirectory(const std::string& stagingPath);
    
    // Registry operations for component registration
    bool updateComponentRegistry(const std::vector<CbsComponentInfo>& components);
    bool removeComponentRegistry(const std::vector<CbsComponentInfo>& components);
    
    // Error handling helpers
    void setLastError(const std::string& error);
    void appendToErrorLog(const std::string& logEntry);

    // Path helpers
    std::string toAbsolutePath(const std::string& path);
    std::wstring getSystemToolPath(const wchar_t* toolName);
    bool isUncPath(const std::string& path) const { return path.rfind("\\\\", 0) == 0; }
    
    // System interaction
    bool notifyTrustedInstaller(const std::vector<std::string>& operations);
    bool schedulePostInstallTasks(const std::vector<std::string>& tasks);

    // Privilege management
    bool enableRequiredPrivileges();
    bool enableTrustedInstallerPrivileges();

    // Diagnostics
    void tailServicingLogs(int lastLines);

    // Constants for CBS operations
    static constexpr const char* CBS_STORE_PATH = "\\Windows\\servicing\\Packages";
    static constexpr const char* CBS_LOG_PATH = "\\Windows\\Logs\\CBS";
    static constexpr const char* COMPONENT_STORE_PATH = "\\Windows\\WinSxS";
    static constexpr const char* MANIFEST_CACHE_PATH = "\\Windows\\servicing\\Packages";
};

// Forward declaration only - actual definition is in CbsIntegratedHandler.h
class CbsIntegratedCabHandler;

// Utility Functions for CBS Integration
namespace CbsUtils {
    // Manifest utilities
    std::vector<std::string> findManifestFiles(const std::string& directory);
    bool isValidManifestFile(const std::string& filePath);
    std::string extractComponentIdentity(const std::string& manifestPath);
    
    // System utilities
    bool isRunningOnline();
    std::string getSystemArchitecture();
    std::string getWindowsVersion();
    
    // Path utilities
    std::string constructComponentPath(const std::string& componentIdentity, 
                                     const std::string& basePath);
    std::string constructManifestPath(const std::string& componentIdentity,
                                    const std::string& basePath);
    
    // Logging utilities
    void logCbsOperation(const std::string& operation, 
                        const std::string& details,
                        const std::string& logPath);
}