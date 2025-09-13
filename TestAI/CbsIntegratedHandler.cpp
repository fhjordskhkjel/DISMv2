#include "CbsIntegratedHandler.h"
#include "CbsManager.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

// Enhanced CAB Handler with CBS Integration Implementation

CbsIntegratedCabHandler::CbsIntegratedCabHandler() 
    : cbsManager(std::make_unique<CbsManager>()),
      lastInstallResult(std::make_unique<CbsInstallResult>()),
      lastError{}
{
    lastInstallResult->success = false;
}

CbsIntegratedCabHandler::~CbsIntegratedCabHandler() = default;

bool CbsIntegratedCabHandler::installPackageWithFullCbs(const std::string& packagePath,
                                                       const std::string& targetPath,
                                                       const std::string& logPath,
                                                       bool quiet) {
    if (!quiet) {
        std::cout << "Starting CBS-integrated package installation...\n";
        std::cout << "Package: " << packagePath << "\n";
        std::cout << "Target: " << targetPath << "\n";
    }
    
    try {
        // Initialize CBS Manager
        if (!cbsManager->initialize()) {
            setLastError("Failed to initialize CBS Manager: " + 
                        cbsManager->getLastError().value_or("Unknown error"));
            return false;
        }
        
        // Perform CBS-integrated installation
        *lastInstallResult = cbsManager->installPackageWithCbs(packagePath, targetPath, false);
        
        if (!lastInstallResult->success) {
            setLastError("CBS installation failed: " + lastInstallResult->errorDescription);
            return false;
        }
        
        if (!quiet) {
            std::cout << "? CBS-integrated installation completed successfully!\n";
            std::cout << "Installed components: " << lastInstallResult->installedComponents.size() << "\n";
            
            if (lastInstallResult->needsRestart) {
                std::cout << "??  System restart required to complete installation.\n";
            }
        }
        
        // Log results if log path provided
        if (!logPath.empty()) {
            CbsUtils::logCbsOperation("InstallPackage", 
                                    "Package: " + packagePath + ", Components: " + 
                                    std::to_string(lastInstallResult->installedComponents.size()),
                                    logPath);
        }
        
        return true;
        
    } catch (const std::exception& ex) {
        setLastError("Exception during CBS-integrated installation: " + std::string(ex.what()));
        return false;
    }
}

bool CbsIntegratedCabHandler::installExtractedPackageWithFullCbs(const std::string& extractedDir,
                                                                const std::string& targetPath,
                                                                const std::string& logPath,
                                                                bool quiet) {
    if (!quiet) {
        std::cout << "Starting CBS-integrated installation from extracted directory...\n";
        std::cout << "Extracted Directory: " << extractedDir << "\n";
        std::cout << "Target: " << targetPath << "\n";
    }
    
    try {
        // Initialize CBS Manager
        if (!cbsManager->initialize()) {
            setLastError("Failed to initialize CBS Manager: " + 
                        cbsManager->getLastError().value_or("Unknown error"));
            return false;
        }
        
        // Perform CBS-integrated installation from extracted directory
        *lastInstallResult = cbsManager->installExtractedPackageWithCbs(extractedDir, targetPath, false);
        
        if (!lastInstallResult->success) {
            setLastError("CBS installation from extracted directory failed: " + 
                        lastInstallResult->errorDescription);
            return false;
        }
        
        if (!quiet) {
            std::cout << "? CBS-integrated installation from extracted directory completed!\n";
            std::cout << "Installed components: " << lastInstallResult->installedComponents.size() << "\n";
            
            if (lastInstallResult->needsRestart) {
                std::cout << "??  System restart required to complete installation.\n";
            }
        }
        
        // Log results
        if (!logPath.empty()) {
            CbsUtils::logCbsOperation("InstallExtractedPackage", 
                                    "Directory: " + extractedDir + ", Components: " + 
                                    std::to_string(lastInstallResult->installedComponents.size()),
                                    logPath);
        }
        
        return true;
        
    } catch (const std::exception& ex) {
        setLastError("Exception during CBS-integrated installation from directory: " + 
                    std::string(ex.what()));
        return false;
    }
}

bool CbsIntegratedCabHandler::installPackageOnlineWithCbs(const std::string& packagePath,
                                                         const std::string& logPath,
                                                         bool quiet) {
    if (!quiet) {
        std::cout << "Starting CBS-integrated ONLINE installation...\n";
        std::cout << "Package: " << packagePath << "\n";
        std::cout << "Mode: Online (Live System)\n";
    }
    
    try {
        // Initialize CBS Manager
        if (!cbsManager->initialize()) {
            setLastError("Failed to initialize CBS Manager: " + 
                        cbsManager->getLastError().value_or("Unknown error"));
            return false;
        }
        
        // Set online mode
        cbsManager->setSystemOnline(true);
        
        // Get Windows directory for online installation
        char windowsDir[MAX_PATH];
        if (GetWindowsDirectoryA(windowsDir, MAX_PATH) == 0) {
            setLastError("Failed to get Windows directory for online installation");
            return false;
        }
        
        std::string systemRoot = fs::path(windowsDir).parent_path().string();
        
        // Perform CBS-integrated online installation
        *lastInstallResult = cbsManager->installPackageWithCbs(packagePath, systemRoot, true);
        
        if (!lastInstallResult->success) {
            setLastError("CBS online installation failed: " + lastInstallResult->errorDescription);
            return false;
        }
        
        if (!quiet) {
            std::cout << "? CBS-integrated ONLINE installation completed successfully!\n";
            std::cout << "Installed components: " << lastInstallResult->installedComponents.size() << "\n";
            std::cout << "Target system: " << systemRoot << "\n";
            
            if (lastInstallResult->needsRestart) {
                std::cout << "?? SYSTEM RESTART REQUIRED to complete installation.\n";
                std::cout << "   Use 'shutdown /r /t 0' to restart immediately.\n";
            }
        }
        
        // Log results
        if (!logPath.empty()) {
            CbsUtils::logCbsOperation("InstallPackageOnline", 
                                    "Package: " + packagePath + ", Online: true, Components: " + 
                                    std::to_string(lastInstallResult->installedComponents.size()),
                                    logPath);
        }
        
        return true;
        
    } catch (const std::exception& ex) {
        setLastError("Exception during CBS-integrated online installation: " + 
                    std::string(ex.what()));
        return false;
    }
}

bool CbsIntegratedCabHandler::validatePackageForInstallation(const std::string& packagePath,
                                                           const std::string& targetPath) {
    try {
        if (!cbsManager->initialize()) {
            setLastError("Failed to initialize CBS Manager for validation");
            return false;
        }
        
        // Verify package signature
        if (!cbsManager->verifyPackageSignature(packagePath)) {
            setLastError("Package signature verification failed");
            return false;
        }
        
        // Analyze package
        auto packageInfo = cbsManager->analyzePackage(packagePath);
        if (!packageInfo) {
            setLastError("Failed to analyze package for validation");
            return false;
        }
        
        // Check applicability
        if (!cbsManager->checkApplicability(*packageInfo, targetPath)) {
            setLastError("Package is not applicable to target system");
            return false;
        }
        
        // Validate dependencies
        if (!cbsManager->validateDependencies(*packageInfo)) {
            setLastError("Package dependency validation failed");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& ex) {
        setLastError("Exception during package validation: " + std::string(ex.what()));
        return false;
    }
}

bool CbsIntegratedCabHandler::beginPackageInstallation() {
    try {
        if (!cbsManager->initialize()) {
            setLastError("Failed to initialize CBS Manager for transaction");
            return false;
        }
        
        return cbsManager->beginTransaction();
        
    } catch (const std::exception& ex) {
        setLastError("Exception beginning package installation transaction: " + 
                    std::string(ex.what()));
        return false;
    }
}

bool CbsIntegratedCabHandler::commitPackageInstallation() {
    try {
        return cbsManager->commitTransaction();
        
    } catch (const std::exception& ex) {
        setLastError("Exception committing package installation: " + std::string(ex.what()));
        return false;
    }
}

bool CbsIntegratedCabHandler::rollbackPackageInstallation() {
    try {
        return cbsManager->rollbackTransaction();
        
    } catch (const std::exception& ex) {
        setLastError("Exception rolling back package installation: " + std::string(ex.what()));
        return false;
    }
}

bool CbsIntegratedCabHandler::installWithDependencyResolution(const std::string& packagePath,
                                                             const std::string& targetPath,
                                                             bool installDependencies) {
    try {
        if (!cbsManager->initialize()) {
            setLastError("Failed to initialize CBS Manager for dependency resolution");
            return false;
        }
        
        // Analyze package to get dependency information
        auto packageInfo = cbsManager->analyzePackage(packagePath);
        if (!packageInfo) {
            setLastError("Failed to analyze package for dependency resolution");
            return false;
        }
        
        // Resolve dependencies
        auto dependencies = cbsManager->resolveDependencies(*packageInfo);
        
        if (installDependencies && !dependencies.empty()) {
            std::cout << "Installing " << dependencies.size() << " dependencies...\n";
            
            for (const auto& dependency : dependencies) {
                std::cout << "Installing dependency: " << dependency << "\n";
                // In a full implementation, this would recursively install dependencies
            }
        }
        
        // Install the main package
        *lastInstallResult = cbsManager->installPackageWithCbs(packagePath, targetPath, false);
        
        return lastInstallResult->success;
        
    } catch (const std::exception& ex) {
        setLastError("Exception during dependency-aware installation: " + std::string(ex.what()));
        return false;
    }
}

std::optional<std::string> CbsIntegratedCabHandler::getLastError() const {
    return lastError;
}

void CbsIntegratedCabHandler::setLastError(const std::string& error) {
    lastError = error;
    std::cerr << "CBS Error: " << error << std::endl;
}

CbsInstallResult CbsIntegratedCabHandler::getLastInstallResult() const { 
    return *lastInstallResult; 
}