#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <windows.h>

// Forward declarations to avoid circular dependencies
class CbsManager;

// Forward declaration - CbsInstallResult is defined in CbsManager.h
struct CbsInstallResult;

/**
 * Enhanced CAB Handler with CBS Integration
 * 
 * This class extends the existing CabHandler with proper CBS functionality
 * for enterprise-grade Windows component management
 */
class CbsIntegratedCabHandler {
public:
    CbsIntegratedCabHandler();
    ~CbsIntegratedCabHandler();
    
    // Enhanced installation methods with CBS integration
    bool installPackageWithFullCbs(const std::string& packagePath,
                                 const std::string& targetPath,
                                 const std::string& logPath = "",
                                 bool quiet = false);
    
    bool installExtractedPackageWithFullCbs(const std::string& extractedDir,
                                          const std::string& targetPath,
                                          const std::string& logPath = "",
                                          bool quiet = false);
    
    // Online installation with CBS (DISM-equivalent)
    bool installPackageOnlineWithCbs(const std::string& packagePath,
                                   const std::string& logPath = "",
                                   bool quiet = false);
    
    // Pre-installation validation
    bool validatePackageForInstallation(const std::string& packagePath,
                                       const std::string& targetPath = "");
    
    // Transaction-based installation
    bool beginPackageInstallation();
    bool commitPackageInstallation();
    bool rollbackPackageInstallation();
    
    // Dependency-aware installation
    bool installWithDependencyResolution(const std::string& packagePath,
                                        const std::string& targetPath,
                                        bool installDependencies = true);
    
    // Error handling
    std::optional<std::string> getLastError() const;
    CbsInstallResult getLastInstallResult() const;

private:
    std::unique_ptr<CbsManager> cbsManager;
    std::unique_ptr<CbsInstallResult> lastInstallResult;  // Use pointer to avoid duplicate definition
    std::optional<std::string> lastError;
    
    void setLastError(const std::string& error);
};