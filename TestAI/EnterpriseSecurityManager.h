#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <array>

// Advanced Security Manager for enhanced package validation
class SecurityManager {
private:
    struct CertificateInfo {
        std::string subject;
        std::string issuer;
        std::string thumbprint;
        std::chrono::system_clock::time_point notBefore;
        std::chrono::system_clock::time_point notAfter;
        bool isValid;
        bool isTrusted;
    };

    struct SecurityContext {
        bool hasAdminRights;
        bool hasTrustedInstallerRights;
        bool hasSeBackupPrivilege;
        bool hasSeRestorePrivilege;
        bool hasSeSecurityPrivilege;
        std::string currentUser;
        std::string currentDomain;
    };

public:
    SecurityManager();
    ~SecurityManager();

    // Enhanced certificate validation
    bool validateCertificateChain(const std::string& packagePath);
    bool checkPackageIntegrity(const std::string& packagePath);
    bool validateTrustedPublisher(const std::string& packagePath);
    bool checkRevocationStatus(const std::string& packagePath);
    
    // Advanced signature verification
    bool verifyAuthenticodeSignature(const std::string& filePath);
    bool validateCatalogSignature(const std::string& filePath);
    bool checkSignatureTimestamp(const std::string& filePath);
    
    // Security context management
    SecurityContext getCurrentSecurityContext();
    bool elevateSecurityContext();
    bool runWithElevatedPrivileges(const std::function<bool()>& operation);
    
    // Risk assessment
    enum class RiskLevel { LOW, MEDIUM, HIGH, CRITICAL };
    struct RiskAssessment {
        RiskLevel level;
        std::vector<std::string> riskFactors;
        std::vector<std::string> mitigations;
        bool requiresApproval;
        double confidenceScore;
    };
    
    RiskAssessment analyzePackageRisk(const std::string& packagePath);
    bool performHeuristicAnalysis(const std::string& packagePath);
    
    // Trust management
    bool addTrustedPublisher(const std::string& publisherName);
    bool removeTrustedPublisher(const std::string& publisherName);
    bool isTrustedPublisher(const std::string& publisherName);
    
    // Security policy enforcement
    bool checkGroupPolicyCompliance(const std::string& packagePath);
    bool enforceInstallationPolicy(const std::string& packagePath);
    bool validateDomainPolicy(const std::string& packagePath);

private:
    bool initialized;
    SecurityContext currentContext;
    std::vector<std::string> trustedPublishers;
    std::string lastError;
    
    // Internal certificate handling
    CertificateInfo extractCertificateInfo(const std::string& filePath);
    bool validateCertificateChainInternal(PCCERT_CONTEXT certContext);
    bool checkCertificateRevocation(PCCERT_CONTEXT certContext);
    
    // Internal risk analysis
    double calculateEntropyScore(const std::string& filePath);
    bool checkSuspiciousAPIs(const std::string& filePath);
    bool analyzeResourceSection(const std::string& filePath);
    
    // Internal policy checking
    bool loadGroupPolicies();
    bool checkRegistryPolicies(const std::string& packagePath);
    
    // Privilege management
    bool checkPrivilege(const std::string& privilegeName);
    
    void setLastError(const std::string& error);
};

// Windows Resource Protection (WRP) Manager
class WrpManager {
public:
    WrpManager();
    ~WrpManager();
    
    // WRP bypass operations
    bool createWrpBypass(const std::vector<std::string>& filePaths);
    bool removeWrpBypass(const std::vector<std::string>& filePaths);
    bool isWrpProtected(const std::string& filePath);
    
    // Safe WRP operations
    bool installWithWrpBypass(const std::string& packagePath, 
                             const std::vector<std::string>& protectedFiles,
                             const std::function<bool()>& installOperation);
    
    // WRP status management
    bool disableWrpTemporarily();
    bool enableWrp();
    bool getWrpStatus();
    
private:
    bool wrpDisabled;
    std::vector<std::string> bypassedFiles;
    std::string lastError;
    
    bool modifyWrpRegistry(bool disable);
    bool backupWrpSettings();
    bool restoreWrpSettings();
};

// TrustedInstaller Service Manager
class TrustedInstallerManager {
public:
    TrustedInstallerManager();
    ~TrustedInstallerManager();
    
    // TrustedInstaller operations
    bool runAsTrustedInstaller(const std::function<bool()>& operation);
    bool elevateToTrustedInstaller();
    bool impersonateTrustedInstaller();
    bool revertFromTrustedInstaller();
    
    // Service management
    bool startTrustedInstallerService();
    bool stopTrustedInstallerService();
    bool getTrustedInstallerServiceStatus();
    
    // Process token management
    bool acquireTrustedInstallerToken();
    bool releaseTrustedInstallerToken();
    
private:
    HANDLE trustedInstallerToken;
    HANDLE originalToken;
    bool impersonating;
    std::string lastError;
    
    bool openTrustedInstallerProcess();
    bool duplicateProcessToken(HANDLE processHandle);
    bool setTokenPrivileges(HANDLE token);
};

// Performance Monitor for installation metrics
class PerformanceMonitor {
public:
    struct InstallMetrics {
        std::chrono::milliseconds totalTime;
        std::chrono::milliseconds extractionTime;
        std::chrono::milliseconds installationTime;
        std::chrono::milliseconds validationTime;
        size_t bytesProcessed;
        size_t filesExtracted;
        size_t filesInstalled;
        int registryKeysModified;
        std::vector<std::string> performanceBottlenecks;
        double cpuUsagePercent;
        size_t memoryUsageMB;
        size_t diskIOBytes;
    };

    PerformanceMonitor();
    ~PerformanceMonitor();
    
    // Monitoring operations
    void startMonitoring(const std::string& operationName);
    void stopMonitoring();
    InstallMetrics getMetrics();
    
    // Performance analysis
    std::vector<std::string> identifyBottlenecks();
    double calculateEfficiencyScore();
    std::string generatePerformanceReport();
    
    // Real-time monitoring
    void enableRealTimeMonitoring(bool enable);
    bool isMonitoring();
    
private:
    bool monitoring;
    std::string currentOperation;
    std::chrono::high_resolution_clock::time_point startTime;
    InstallMetrics currentMetrics;
    
    // Performance counters
    HANDLE performanceQuery;
    HANDLE cpuCounter;
    HANDLE memoryCounter;
    HANDLE diskCounter;
    
    void initializeCounters();
    void updateCounters();
    void cleanupCounters();
};

// Advanced Error Analyzer
class ErrorAnalyzer {
public:
    enum class ErrorCategory {
        PERMISSION_DENIED,
        DEPENDENCY_MISSING,
        CORRUPTION_DETECTED,
        SIGNATURE_INVALID,
        POLICY_VIOLATION,
        RESOURCE_EXHAUSTED,
        NETWORK_ERROR,
        UNKNOWN
    };
    
    struct ErrorAnalysis {
        ErrorCategory category;
        std::string description;
        std::vector<std::string> possibleCauses;
        std::vector<std::string> suggestedSolutions;
        std::string automaticResolution;
        bool canAutoResolve;
        double confidenceLevel;
    };

    ErrorAnalyzer();
    ~ErrorAnalyzer();
    
    // Error analysis
    ErrorAnalysis analyzeError(DWORD errorCode, const std::string& context);
    ErrorAnalysis analyzeDependencyFailures(const std::vector<std::string>& failures);
    ErrorAnalysis analyzePermissionIssues(const std::vector<std::string>& failures);
    
    // Automatic resolution
    bool attemptAutomaticResolution(const ErrorAnalysis& analysis);
    std::string suggestManualResolution(const ErrorAnalysis& analysis);
    
    // Diagnostic capabilities
    bool performAutomaticDiagnostics(const std::string& packagePath);
    std::string generateDiagnosticReport(const std::string& packagePath);
    
private:
    std::vector<ErrorAnalysis> knownErrors;
    std::string lastError;
    
    void initializeKnownErrors();
    ErrorCategory categorizeError(DWORD errorCode);
    std::vector<std::string> generateSolutions(ErrorCategory category, const std::string& context);
    bool tryCommonFixes(ErrorCategory category);
};

// System State Manager for advanced backup/restore
class SystemStateManager {
public:
    struct SystemSnapshot {
        std::string snapshotId;
        std::chrono::system_clock::time_point creationTime;
        std::string description;
        std::vector<std::string> backedUpFiles;
        std::vector<std::string> registryKeys;
        size_t totalSize;
        bool isValid;
    };

    SystemStateManager();
    ~SystemStateManager();
    
    // Snapshot operations
    std::string createSystemSnapshot(const std::string& description);
    bool restoreFromSnapshot(const std::string& snapshotId);
    bool deleteSnapshot(const std::string& snapshotId);
    std::vector<SystemSnapshot> listSnapshots();
    
    // Package-specific backup
    std::string createPackageSpecificBackup(const std::string& packagePath);
    bool restorePackageBackup(const std::string& backupId);
    
    // Registry state management
    bool backupRegistryState(const std::vector<std::string>& keyPaths);
    bool restoreRegistryState(const std::string& backupId);
    
    // File system state management
    bool backupFileSystemState(const std::vector<std::string>& filePaths);
    bool restoreFileSystemState(const std::string& backupId);
    
    // Integration with Windows System Restore
    bool createRestorePoint(const std::string& description);
    bool enableSystemRestore();
    
private:
    std::vector<SystemSnapshot> snapshots;
    std::string backupBasePath;
    std::string lastError;
    
    bool initializeBackupSystem();
    std::string generateSnapshotId();
    bool copyFilesWithMetadata(const std::vector<std::string>& files, const std::string& destination);
    bool restoreFilesWithMetadata(const std::vector<std::string>& files, const std::string& source);
};

// Installation Intelligence Engine
class InstallIntelligence {
public:
    struct SystemInfo {
        std::string osVersion;
        std::string architecture;
        size_t totalMemoryMB;
        size_t availableMemoryMB;
        size_t totalDiskSpaceMB;
        size_t availableDiskSpaceMB;
        std::vector<std::string> installedPackages;
        std::vector<std::string> runningServices;
    };
    
    struct PredictionResult {
        std::chrono::milliseconds estimatedTime;
        double successProbability;
        std::vector<std::string> potentialIssues;
        std::vector<std::string> recommendations;
        bool shouldProceed;
    };

    InstallIntelligence();
    ~InstallIntelligence();
    
    // Predictive analytics
    PredictionResult predictInstallationOutcome(const std::string& packagePath, const SystemInfo& system);
    std::chrono::milliseconds estimateInstallationTime(const std::string& packagePath);
    double calculateSuccessProbability(const std::string& packagePath, const SystemInfo& system);
    
    // Optimization
    std::vector<std::string> optimizeInstallationOrder(const std::vector<std::string>& packages);
    std::string recommendOptimalSettings(const SystemInfo& system);
    
    // Anomaly detection
    bool detectAnomalousPackage(const std::string& packagePath);
    std::vector<std::string> identifyAnomalies(const std::string& packagePath);
    
    // Learning and adaptation
    void recordInstallationResult(const std::string& packagePath, const SystemInfo& system, 
                                 bool success, std::chrono::milliseconds actualTime);
    void updatePredictionModel();
    
private:
    struct InstallationRecord {
        std::string packageHash;
        SystemInfo system;
        bool success;
        std::chrono::milliseconds duration;
        std::chrono::system_clock::time_point timestamp;
    };
    
    std::vector<InstallationRecord> installationHistory;
    SystemInfo currentSystem;
    std::string lastError;
    
    SystemInfo gatherSystemInfo();
    double calculateSimilarity(const SystemInfo& sys1, const SystemInfo& sys2);
    std::vector<InstallationRecord> findSimilarInstallations(const std::string& packagePath, const SystemInfo& system);
    bool analyzePackageComplexity(const std::string& packagePath);
};