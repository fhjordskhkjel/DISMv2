#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <chrono>
#include <regex>
#include <windows.h>
#include <comdef.h>
#include <msxml6.h>
#include <atlbase.h>

// Simplified Package Identity Structure
struct PackageIdentity {
    std::string name;
    std::string version;
    std::string architecture;
    std::string language;
    std::string publicKeyToken;
    std::string buildType;
    std::string releaseType;
    
    std::string getFullIdentity() const;
    std::string getShortIdentity() const;
    bool isCompatibleWith(const PackageIdentity& other) const;
    int compareVersion(const PackageIdentity& other) const;
};

// Simplified Component Manifest
struct ComponentManifest {
    PackageIdentity identity;
    std::string manifestPath;
    std::string componentType;
    std::string displayName;
    std::string description;
    std::string restartRequired;
    
    std::vector<PackageIdentity> dependencies;
    std::vector<PackageIdentity> supersedes;
    
    std::chrono::system_clock::time_point creationTime;
    bool isValid = false;
    std::string parseErrors;
};

// Package Installation State
enum class PackageInstallState {
    NotPresent,
    Staged,
    Installed,
    PartiallyInstalled,
    Superseded,
    Pending,
    Failed,
    Corrupted,
    Unknown
};

struct PackageState {
    PackageIdentity identity;
    PackageInstallState state;
    std::string stateDetails;
    std::chrono::system_clock::time_point lastStateChange;
    std::string installationPath;
    bool needsRestart = false;
    std::string errorCode;
    std::string errorDescription;
};

// Supersedence Information
struct SupersedenceInfo {
    PackageIdentity superseding;
    PackageIdentity superseded;
    std::string supersedenceType;
    std::string reason;
    bool isDirectSupersedence;
    std::chrono::system_clock::time_point effectiveDate;
};

/**
 * Simplified Package Supersedence Manager
 */
class PackageSupersedenceManager {
public:
    PackageSupersedenceManager();
    ~PackageSupersedenceManager();

    // Installation intelligence
    enum class InstallDecision {
        Install,
        Skip_AlreadyInstalled,
        Skip_Superseded,
        Skip_NewerExists,
        Skip_Incompatible,
        Skip_Failed,
        Repair_Corrupted,
        Update_Available,
        Downgrade_Blocked
    };
    
    struct InstallRecommendation {
        InstallDecision decision;
        PackageIdentity targetPackage;
        std::string reasoning;
        std::vector<PackageIdentity> prerequisitePackages;
        std::vector<PackageIdentity> conflictingPackages;
        bool requiresRestart;
        std::string riskAssessment;
    };

    // Core functionality
    bool initialize();
    void cleanup();
    
    // Manifest parsing
    ComponentManifest* parseManifestFile(const std::string& mumFilePath);
    std::vector<ComponentManifest> parseManifestDirectory(const std::string& directory);
    bool validateManifestIntegrity(const ComponentManifest& manifest);
    
    // Package state management
    PackageState getPackageState(const PackageIdentity& identity);
    std::vector<PackageState> getInstalledPackages();
    std::vector<PackageState> getStagedPackages();
    std::vector<PackageState> getCorruptedPackages();
    
    // Supersedence analysis
    std::vector<SupersedenceInfo> findSupersedingPackages(const PackageIdentity& package);
    bool isPackageSuperseded(const PackageIdentity& package);
    
    // Installation intelligence
    InstallRecommendation analyzePackageInstall(const PackageIdentity& package);
    std::vector<InstallRecommendation> optimizeInstallationOrder(const std::vector<PackageIdentity>& packages);
    
    // Advanced analysis features
    bool detectCircularDependencies(const std::vector<PackageIdentity>& packages);
    std::vector<PackageIdentity> resolveInstallationChain(const PackageIdentity& rootPackage);
    std::vector<PackageIdentity> findUpdateCandidates();
    std::vector<PackageIdentity> findOrphanedPackages();
    
    // System integration
    bool scanSystemForPackages();
    
    // Error handling and diagnostics
    std::string getLastError() const;
    std::string getDetailedLog() const { return detailedLog; }
    void enableVerboseLogging(bool enable) { verboseLogging = enable; }

private:
    // Internal state
    std::string lastError;
    std::string detailedLog;
    bool verboseLogging = false;
    bool initialized = false;
    std::string systemPath;
    bool supersedenceCheckingEnabled = true;
    
    // Package databases
    std::unordered_map<std::string, PackageState> packageStates;
    std::unordered_map<std::string, ComponentManifest> manifestCache;
    std::vector<SupersedenceInfo> supersedenceDatabase;
    std::unordered_set<std::string> processedManifests;
    
    // XML processing
    CComPtr<IXMLDOMDocument> xmlDocument;
    bool initializeXmlProcessor();
    void cleanupXmlProcessor();
    
    // Manifest parsing helpers
    bool parseManifestXml(const std::string& xmlPath, ComponentManifest& manifest);
    bool parsePackageIdentityFromXml(IXMLDOMNode* node, PackageIdentity& identity);
    std::string getXmlAttributeValue(IXMLDOMNamedNodeMap* attributes, const std::wstring& attributeName);
    
    // Enhanced XML processing for .mum manifests
    bool parseIdentityFromFilename(const std::string& filename, PackageIdentity& identity);
    bool parseManifestProperties(IXMLDOMElement* rootElement, ComponentManifest& manifest);
    bool parseDependencies(IXMLDOMElement* rootElement, ComponentManifest& manifest);
    bool parseSupersedence(IXMLDOMElement* rootElement, ComponentManifest& manifest);
    std::string extractXmlNodeText(IXMLDOMNode* node);
    std::vector<CComPtr<IXMLDOMNode>> selectXmlNodes(IXMLDOMElement* element, const std::wstring& xpath);
    
    // System integration helpers
    bool queryInstalledPackagesFromRegistry();
    bool queryInstalledPackagesFromCBS();
    bool queryStagedPackagesFromStore();
    PackageState queryPackageStateFromSystem(const PackageIdentity& identity);
    
    // Supersedence detection
    void buildSupersedenceDatabase();
    void detectImplicitSupersedence();
    
    // String conversion utilities
    std::string packageStateToString(PackageInstallState state);
    std::string installDecisionToString(InstallDecision decision);
    
    // Logging and error handling
    void logMessage(const std::string& message);
    void logError(const std::string& error);
    void logVerbose(const std::string& message);
    void setLastError(const std::string& error);
    
    // File system utilities
    std::vector<std::string> findManifestFiles(const std::string& directory);
    std::string getManifestHash(const std::string& filePath);
    
    // Helper methods
    std::vector<PackageIdentity> findNewerVersionsOfPackage(const PackageIdentity& package);
    std::vector<PackageIdentity> findConflictingPackages(const PackageIdentity& package);
    ComponentManifest* findManifestForPackage(const PackageIdentity& package);
    std::string assessInstallationRisk(const PackageIdentity& package);
    bool hasTransitiveDependency(const PackageIdentity& package1, const PackageIdentity& package2);
    
    // Constants
    static constexpr const char* MANIFEST_EXTENSION = ".mum";
};

/**
 * Utility Functions for Package Analysis
 */
namespace PackageUtils {
    // Version utilities
    std::string normalizeVersionString(const std::string& version);
    bool isVersionStringValid(const std::string& version);
    int compareSemanticVersions(const std::string& v1, const std::string& v2);
    
    // Architecture utilities
    std::string normalizeArchitecture(const std::string& arch);
    bool isArchitectureCompatible(const std::string& packageArch, const std::string& systemArch);
    std::vector<std::string> getSupportedArchitectures();
    
    // System utilities
    std::string getCurrentSystemArchitecture();
    std::string getCurrentWindowsVersion();
    bool isSystemCompatibleWithPackage(const PackageIdentity& package);
    
    // Logging utilities
    void logPackageOperation(const std::string& operation, const PackageIdentity& package,
                           const std::string& result, const std::string& logPath);
}