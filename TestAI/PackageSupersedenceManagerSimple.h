#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>

/**
 * @file PackageSupersedenceManagerSimple.h
 * @brief Simplified Package Supersedence Manager - Phase 2 Enhancement
 * 
 * This simplified version provides essential package supersedence functionality
 * with reduced complexity for basic use cases and performance-critical scenarios.
 * 
 * Key Features:
 * - Lightweight package identity management
 * - Basic supersedence detection
 * - Simple installation recommendations
 * - Fast package validation
 * - Memory-efficient operations
 */

namespace WindowsInstallationEnhancement {
namespace Simple {

/**
 * @brief Simplified Package Identity Structure
 */
struct SimplePackageIdentity {
    std::string name;
    std::string version;
    std::string architecture;
    
    std::string getIdentityString() const;
    bool isNewer(const SimplePackageIdentity& other) const;
    bool isCompatible(const SimplePackageIdentity& other) const;
    bool operator==(const SimplePackageIdentity& other) const;
};

/**
 * @brief Simple Package State Information
 */
enum class SimplePackageState {
    Unknown = 0,
    NotInstalled = 1,
    Installed = 2,
    Superseded = 3,
    Pending = 4
};

/**
 * @brief Simple Installation Decision
 */
enum class SimpleInstallDecision {
    Install = 0,
    Skip_AlreadyInstalled = 1,
    Skip_Superseded = 2,
    Skip_Incompatible = 3,
    Update_Available = 4
};

/**
 * @brief Simple Package Information
 */
struct SimplePackageInfo {
    SimplePackageIdentity identity;
    SimplePackageState state;
    std::string filePath;
    std::chrono::system_clock::time_point lastModified;
    bool isValid = false;
};

/**
 * @brief Simple Installation Recommendation
 */
struct SimpleInstallRecommendation {
    SimpleInstallDecision decision;
    SimplePackageIdentity package;
    std::string reason;
    bool requiresRestart = false;
};

/**
 * @brief Simplified Package Supersedence Manager
 * 
 * Provides essential package supersedence functionality with:
 * - Fast package identity parsing
 * - Basic supersedence detection
 * - Simple installation recommendations
 * - Lightweight memory footprint
 */
class PackageSupersedenceManagerSimple {
public:
    PackageSupersedenceManagerSimple();
    ~PackageSupersedenceManagerSimple();

    // Core functionality
    bool initialize();
    void cleanup();
    
    // Package identity operations
    SimplePackageIdentity parsePackageIdentity(const std::string& packagePath);
    bool isPackageIdentityValid(const SimplePackageIdentity& identity);
    
    // Package state management
    SimplePackageState getPackageState(const SimplePackageIdentity& identity);
    std::vector<SimplePackageInfo> getInstalledPackages();
    
    // Supersedence detection
    bool isPackageSuperseded(const SimplePackageIdentity& identity);
    std::vector<SimplePackageIdentity> findSupersedingPackages(const SimplePackageIdentity& identity);
    
    // Installation recommendations
    SimpleInstallRecommendation analyzePackageInstall(const SimplePackageIdentity& identity);
    std::vector<SimpleInstallRecommendation> analyzeMultiplePackages(const std::vector<SimplePackageIdentity>& packages);
    
    // Package scanning
    std::vector<SimplePackageInfo> scanDirectoryForPackages(const std::string& directory);
    bool addPackageToDatabase(const SimplePackageInfo& packageInfo);
    
    // Configuration
    void setVerboseLogging(bool enable) { verboseLogging_ = enable; }
    void setArchitectureFilter(const std::string& architecture) { architectureFilter_ = architecture; }
    
    // Status and diagnostics
    std::string getLastError() const { return lastError_; }
    size_t getPackageCount() const { return packageDatabase_.size(); }
    bool isInitialized() const { return initialized_; }

private:
    // Internal state
    bool initialized_ = false;
    bool verboseLogging_ = false;
    std::string lastError_;
    std::string architectureFilter_;
    
    // Package database
    std::vector<SimplePackageInfo> packageDatabase_;
    
    // Internal methods
    bool loadSystemPackages();
    SimplePackageIdentity parseFromFilename(const std::string& filename);
    SimplePackageIdentity parseFromManifest(const std::string& manifestPath);
    bool updatePackageDatabase();
    
    // Supersedence logic
    bool checkSupersedence(const SimplePackageIdentity& package1, const SimplePackageIdentity& package2);
    int compareVersions(const std::string& version1, const std::string& version2);
    
    // Utility methods
    std::vector<std::string> findPackageFiles(const std::string& directory);
    bool isArchitectureCompatible(const std::string& packageArch, const std::string& systemArch);
    std::string getCurrentSystemArchitecture();
    
    // Logging
    void logMessage(const std::string& message);
    void logError(const std::string& error);
    void setLastError(const std::string& error);
};

/**
 * @brief Simple Package Utilities
 */
namespace SimplePackageUtils {
    // Version utilities
    int compareSemanticVersions(const std::string& v1, const std::string& v2);
    bool isVersionValid(const std::string& version);
    std::string normalizeVersion(const std::string& version);
    
    // Architecture utilities
    bool isArchitectureSupported(const std::string& architecture);
    std::string normalizeArchitecture(const std::string& architecture);
    
    // File utilities
    bool isPackageFile(const std::string& filePath);
    std::string extractPackageExtension(const std::string& filePath);
    
    // String utilities
    std::vector<std::string> splitString(const std::string& str, char delimiter);
    std::string toLower(const std::string& str);
    std::string trim(const std::string& str);
}

} // namespace Simple
} // namespace WindowsInstallationEnhancement
