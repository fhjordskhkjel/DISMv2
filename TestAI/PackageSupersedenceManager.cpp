#include "PackageSupersedenceManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <regex>
#include <comutil.h>
#include <shlwapi.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "msxml6.lib")
#pragma comment(lib, "shlwapi.lib")

// PackageIdentity Implementation
std::string PackageIdentity::getFullIdentity() const {
    std::stringstream ss;
    ss << name << "_" << version << "_" << architecture << "_" << language << "_" << publicKeyToken;
    return ss.str();
}

std::string PackageIdentity::getShortIdentity() const {
    return name + "_" + version + "_" + architecture;
}

bool PackageIdentity::isCompatibleWith(const PackageIdentity& other) const {
    if (architecture != "neutral" && other.architecture != "neutral" && 
        architecture != other.architecture) {
        return false;
    }
    
    if (language != "neutral" && other.language != "neutral" && 
        language != other.language) {
        return false;
    }
    
    return true;
}

int PackageIdentity::compareVersion(const PackageIdentity& other) const {
    return PackageUtils::compareSemanticVersions(version, other.version);
}

// PackageSupersedenceManager Implementation
PackageSupersedenceManager::PackageSupersedenceManager() 
    : verboseLogging(false)
    , initialized(false)
    , supersedenceCheckingEnabled(true)
{
}

PackageSupersedenceManager::~PackageSupersedenceManager() {
    cleanup();
}

bool PackageSupersedenceManager::initialize() {
    if (initialized) {
        return true;
    }
    
    try {
        logMessage("Initializing Package Supersedence Manager");
        
        // Initialize COM for XML processing
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) {
            setLastError("Failed to initialize COM: " + std::to_string(hr));
            return false;
        }
        
        // Initialize XML processor
        if (!initializeXmlProcessor()) {
            setLastError("Failed to initialize XML processor");
            CoUninitialize();
            return false;
        }
        
        // Set default system path
        systemPath = "C:";
        
        logMessage("System path: " + systemPath);
        
        // Scan system for existing packages
        if (!scanSystemForPackages()) {
            logError("Warning: Failed to scan system for packages during initialization");
        }
        
        // Build supersedence database
        buildSupersedenceDatabase();
        
        initialized = true;
        logMessage("Package Supersedence Manager initialized successfully");
        
        return true;
        
    } catch (const std::exception& ex) {
        setLastError("Exception during initialization: " + std::string(ex.what()));
        cleanup();
        return false;
    }
}

void PackageSupersedenceManager::cleanup() {
    if (!initialized) {
        return;
    }
    
    try {
        // Clear caches
        packageStates.clear();
        manifestCache.clear();
        supersedenceDatabase.clear();
        processedManifests.clear();
        
        // Cleanup XML processor
        cleanupXmlProcessor();
        
        // Cleanup COM
        CoUninitialize();
        
        initialized = false;
        logMessage("Package Supersedence Manager cleanup completed");
        
    } catch (...) {
        // Suppress exceptions during cleanup
    }
}

ComponentManifest* PackageSupersedenceManager::parseManifestFile(const std::string& mumFilePath) {
    if (!initialized && !initialize()) {
        setLastError("Manager not initialized");
        return nullptr;
    }
    
    try {
        logVerbose("Parsing manifest file: " + mumFilePath);
        
        // Check if already processed
        std::string manifestHash = getManifestHash(mumFilePath);
        auto fileName = mumFilePath.substr(mumFilePath.find_last_of("\\") + 1);
        
        if (processedManifests.find(manifestHash) != processedManifests.end()) {
            if (manifestCache.find(fileName) != manifestCache.end()) {
                logVerbose("Using cached manifest for: " + mumFilePath);
                return &manifestCache[fileName];
            }
        }
        
        ComponentManifest manifest;
        manifest.manifestPath = mumFilePath;
        manifest.creationTime = std::chrono::system_clock::now();
        
        // Parse XML content
        if (!parseManifestXml(mumFilePath, manifest)) {
            setLastError("Failed to parse XML content from manifest: " + mumFilePath);
            return nullptr;
        }
        
        // Validate manifest integrity
        if (!validateManifestIntegrity(manifest)) {
            setLastError("Manifest integrity validation failed: " + mumFilePath);
            return nullptr;
        }
        
        // Cache the manifest
        manifestCache[fileName] = manifest;
        processedManifests.insert(manifestHash);
        
        manifest.isValid = true;
        logVerbose("Successfully parsed manifest: " + manifest.identity.getShortIdentity());
        
        return &manifestCache[fileName];
        
    } catch (const std::exception& ex) {
        setLastError("Exception parsing manifest file: " + std::string(ex.what()));
        return nullptr;
    }
}

std::vector<ComponentManifest> PackageSupersedenceManager::parseManifestDirectory(const std::string& directory) {
    std::vector<ComponentManifest> manifests;
    
    if (!initialized && !initialize()) {
        setLastError("Manager not initialized");
        return manifests;
    }
    
    try {
        logMessage("Parsing manifest directory: " + directory);
        
        // Find all manifest files
        auto manifestFiles = findManifestFiles(directory);
        logMessage("Found " + std::to_string(manifestFiles.size()) + " manifest files");
        
        // Parse each manifest file
        for (const auto& manifestFile : manifestFiles) {
            try {
                auto manifest = parseManifestFile(manifestFile);
                if (manifest) {
                    manifests.push_back(*manifest);
                    logVerbose("Parsed manifest: " + manifest->identity.getShortIdentity());
                } else {
                    logError("Failed to parse manifest: " + manifestFile);
                }
            } catch (const std::exception& ex) {
                logError("Exception parsing manifest " + manifestFile + ": " + ex.what());
            }
        }
        
        logMessage("Successfully parsed " + std::to_string(manifests.size()) + " manifests");
        
    } catch (const std::exception& ex) {
        setLastError("Exception parsing manifest directory: " + std::string(ex.what()));
    }
    
    return manifests;
}

bool PackageSupersedenceManager::validateManifestIntegrity(const ComponentManifest& manifest) {
    try {
        if (manifest.identity.name.empty()) {
            logError("Manifest validation failed: Empty package name");
            return false;
        }
        
        if (manifest.identity.version.empty()) {
            logError("Manifest validation failed: Empty version");
            return false;
        }
        
        if (manifest.identity.architecture.empty()) {
            logError("Manifest validation failed: Empty architecture");
            return false;
        }
        
        if (!PackageUtils::isVersionStringValid(manifest.identity.version)) {
            logError("Manifest validation failed: Invalid version format: " + manifest.identity.version);
            return false;
        }
        
        auto supportedArchs = PackageUtils::getSupportedArchitectures();
        if (std::find(supportedArchs.begin(), supportedArchs.end(), 
                     manifest.identity.architecture) == supportedArchs.end()) {
            logError("Manifest validation failed: Unsupported architecture: " + manifest.identity.architecture);
            return false;
        }
        
        logVerbose("Manifest validation passed for: " + manifest.identity.getShortIdentity());
        return true;
        
    } catch (const std::exception& ex) {
        logError("Exception during manifest validation: " + std::string(ex.what()));
        return false;
    }
}

PackageState PackageSupersedenceManager::getPackageState(const PackageIdentity& identity) {
    PackageState state;
    state.identity = identity;
    state.state = PackageInstallState::Unknown;
    state.lastStateChange = std::chrono::system_clock::now();
    
    try {
        std::string packageKey = identity.getFullIdentity();
        
        // Check cache first
        auto cacheIt = packageStates.find(packageKey);
        if (cacheIt != packageStates.end()) {
            logVerbose("Using cached state for package: " + identity.getShortIdentity());
            return cacheIt->second;
        }
        
        // Query package state from system
        state = queryPackageStateFromSystem(identity);
        
        // Cache the result
        packageStates[packageKey] = state;
        
        logVerbose("Retrieved package state: " + identity.getShortIdentity() + 
                  " -> " + packageStateToString(state.state));
        
    } catch (const std::exception& ex) {
        logError("Exception getting package state: " + std::string(ex.what()));
        state.state = PackageInstallState::Unknown;
        state.errorDescription = ex.what();
    }
    
    return state;
}

std::vector<SupersedenceInfo> PackageSupersedenceManager::findSupersedingPackages(const PackageIdentity& package) {
    std::vector<SupersedenceInfo> superseding;
    
    if (!supersedenceCheckingEnabled) {
        return superseding;
    }
    
    try {
        logVerbose("Finding packages that supersede: " + package.getShortIdentity());
        
        for (const auto& supersedenceInfo : supersedenceDatabase) {
            if (supersedenceInfo.superseded.getFullIdentity() == package.getFullIdentity()) {
                superseding.push_back(supersedenceInfo);
                logVerbose("Found superseding package: " + 
                          supersedenceInfo.superseding.getShortIdentity());
            }
        }
        
        // Sort by version (newest first)
        std::sort(superseding.begin(), superseding.end(),
                 [](const SupersedenceInfo& a, const SupersedenceInfo& b) {
                     return a.superseding.compareVersion(b.superseding) > 0;
                 });
        
        logVerbose("Found " + std::to_string(superseding.size()) + 
                  " packages that supersede " + package.getShortIdentity());
        
    } catch (const std::exception& ex) {
        logError("Exception finding superseding packages: " + std::string(ex.what()));
    }
    
    return superseding;
}

bool PackageSupersedenceManager::isPackageSuperseded(const PackageIdentity& package) {
    try {
        auto supersedingPackages = findSupersedingPackages(package);
        
        for (const auto& supersedingInfo : supersedingPackages) {
            auto supersedingState = getPackageState(supersedingInfo.superseding);
            if (supersedingState.state == PackageInstallState::Installed ||
                supersedingState.state == PackageInstallState::Staged) {
                logVerbose("Package " + package.getShortIdentity() + 
                          " is superseded by " + supersedingInfo.superseding.getShortIdentity());
                return true;
            }
        }
        
        return false;
        
    } catch (const std::exception& ex) {
        logError("Exception checking if package is superseded: " + std::string(ex.what()));
        return false;
    }
}

PackageSupersedenceManager::InstallRecommendation 
PackageSupersedenceManager::analyzePackageInstall(const PackageIdentity& package) {
    InstallRecommendation recommendation;
    recommendation.targetPackage = package;
    recommendation.decision = InstallDecision::Install;
    recommendation.requiresRestart = false;
    
    try {
        logMessage("Analyzing package installation: " + package.getShortIdentity());
        
        auto currentState = getPackageState(package);
        
        switch (currentState.state) {
            case PackageInstallState::Installed:
                recommendation.decision = InstallDecision::Skip_AlreadyInstalled;
                recommendation.reasoning = "Package is already installed";
                return recommendation;
                
            case PackageInstallState::Corrupted:
                recommendation.decision = InstallDecision::Repair_Corrupted;
                recommendation.reasoning = "Package is corrupted and needs repair";
                return recommendation;
                
            case PackageInstallState::PartiallyInstalled:
                recommendation.decision = InstallDecision::Install;
                recommendation.reasoning = "Package is partially installed, completing installation";
                break;
                
            default:
                break;
        }
        
        if (isPackageSuperseded(package)) {
            auto supersedingPackages = findSupersedingPackages(package);
            if (!supersedingPackages.empty()) {
                recommendation.decision = InstallDecision::Skip_Superseded;
                recommendation.reasoning = "Package is superseded by " + 
                                        supersedingPackages[0].superseding.getShortIdentity();
                return recommendation;
            }
        }
        
        auto newerVersions = findNewerVersionsOfPackage(package);
        if (!newerVersions.empty()) {
            recommendation.decision = InstallDecision::Update_Available;
            recommendation.targetPackage = newerVersions[0];
            recommendation.reasoning = "Newer version available: " + newerVersions[0].getShortIdentity();
        }
        
        if (!PackageUtils::isSystemCompatibleWithPackage(package)) {
            recommendation.decision = InstallDecision::Skip_Incompatible;
            recommendation.reasoning = "Package is not compatible with current system";
            return recommendation;
        }
        
        recommendation.prerequisitePackages = resolveInstallationChain(package);
        recommendation.conflictingPackages = findConflictingPackages(package);
        recommendation.riskAssessment = assessInstallationRisk(package);
        
        logMessage("Installation analysis completed: " + 
                  installDecisionToString(recommendation.decision));
        
    } catch (const std::exception& ex) {
        logError("Exception analyzing package installation: " + std::string(ex.what()));
        recommendation.decision = InstallDecision::Skip_Failed;
        recommendation.reasoning = "Analysis failed: " + std::string(ex.what());
    }
    
    return recommendation;
}

std::vector<PackageSupersedenceManager::InstallRecommendation> 
PackageSupersedenceManager::optimizeInstallationOrder(const std::vector<PackageIdentity>& packages) {
    std::vector<InstallRecommendation> recommendations;
    
    try {
        logMessage("Optimizing installation order for " + std::to_string(packages.size()) + " packages");
        
        for (const auto& package : packages) {
            auto recommendation = analyzePackageInstall(package);
            recommendations.push_back(recommendation);
        }
        
        logMessage("Installation order optimized: " + std::to_string(recommendations.size()) + 
                  " packages analyzed");
        
    } catch (const std::exception& ex) {
        logError("Exception optimizing installation order: " + std::string(ex.what()));
    }
    
    return recommendations;
}

// Private implementation methods

bool PackageSupersedenceManager::initializeXmlProcessor() {
    try {
        HRESULT hr = CoCreateInstance(CLSID_DOMDocument60, NULL, CLSCTX_INPROC_SERVER,
                                     IID_IXMLDOMDocument, (void**)&xmlDocument);
        
        if (FAILED(hr)) {
            logError("Failed to create XML DOM document: " + std::to_string(hr));
            return false;
        }
        
        xmlDocument->put_async(VARIANT_FALSE);
        xmlDocument->put_validateOnParse(VARIANT_FALSE);
        xmlDocument->put_resolveExternals(VARIANT_FALSE);
        
        logVerbose("XML processor initialized successfully");
        return true;
        
    } catch (const std::exception& ex) {
        logError("Exception initializing XML processor: " + std::string(ex.what()));
        return false;
    }
}

void PackageSupersedenceManager::cleanupXmlProcessor() {
    if (xmlDocument) {
        xmlDocument.Release();
    }
}

bool PackageSupersedenceManager::parseManifestXml(const std::string& xmlPath, ComponentManifest& manifest) {
    try {
        logVerbose("Parsing XML manifest: " + xmlPath);
        
        _variant_t xmlFilePath(xmlPath.c_str());
        VARIANT_BOOL success;
        HRESULT hr = xmlDocument->load(xmlFilePath, &success);
        
        if (FAILED(hr) || success != VARIANT_TRUE) {
            logError("XML parse error for file: " + xmlPath);
            return false;
        }
        
        CComPtr<IXMLDOMElement> rootElement;
        hr = xmlDocument->get_documentElement(&rootElement);
        if (FAILED(hr) || !rootElement) {
            logError("Failed to get XML root element");
            return false;
        }
        
        // Enhanced parsing for Windows .mum manifests
        // Try multiple XPath expressions to find assembly identity
        std::vector<std::wstring> identityPaths = {
            L"//assemblyIdentity",
            L"//package/assemblyIdentity", 
            L"//assembly/assemblyIdentity",
            L"//package/update/package/assemblyIdentity",
            L"//*[@name and @version and @processorArchitecture]"
        };
        
        CComPtr<IXMLDOMNode> identityNode;
        for (const auto& path : identityPaths) {
            hr = rootElement->selectSingleNode(_bstr_t(path.c_str()), &identityNode);
            if (SUCCEEDED(hr) && identityNode) {
                if (parsePackageIdentityFromXml(identityNode, manifest.identity)) {
                    break;
                }
                identityNode.Release();
            }
        }
        
        // If we couldn't find assemblyIdentity, try to parse from filename
        if (manifest.identity.name.empty()) {
            std::string filename = xmlPath.substr(xmlPath.find_last_of("\\") + 1);
            if (parseIdentityFromFilename(filename, manifest.identity)) {
                logVerbose("Parsed identity from filename: " + manifest.identity.getShortIdentity());
            }
        }
        
        // Parse additional manifest properties
        parseManifestProperties(rootElement, manifest);
        
        // Parse dependencies
        parseDependencies(rootElement, manifest);
        
        // Parse supersedence information
        parseSupersedence(rootElement, manifest);
        
        if (!manifest.identity.name.empty()) {
            logVerbose("Successfully parsed XML manifest for: " + manifest.identity.getShortIdentity());
            return true;
        } else {
            logError("Could not extract package identity from manifest: " + xmlPath);
            return false;
        }
        
    } catch (const std::exception& ex) {
        logError("Exception parsing manifest XML: " + std::string(ex.what()));
        return false;
    }
}

bool PackageSupersedenceManager::parsePackageIdentityFromXml(IXMLDOMNode* node, PackageIdentity& identity) {
    try {
        CComPtr<IXMLDOMNamedNodeMap> attributes;
        HRESULT hr = node->get_attributes(&attributes);
        if (FAILED(hr)) {
            return false;
        }
        
        identity.name = getXmlAttributeValue(attributes, L"name");
        identity.version = getXmlAttributeValue(attributes, L"version");
        identity.architecture = getXmlAttributeValue(attributes, L"processorArchitecture");
        identity.language = getXmlAttributeValue(attributes, L"language");
        identity.publicKeyToken = getXmlAttributeValue(attributes, L"publicKeyToken");
        identity.buildType = getXmlAttributeValue(attributes, L"buildType");
        
        return !identity.name.empty() && !identity.version.empty();
        
    } catch (const std::exception& ex) {
        logError("Exception parsing package identity: " + std::string(ex.what()));
        return false;
    }
}

std::string PackageSupersedenceManager::getXmlAttributeValue(IXMLDOMNamedNodeMap* attributes, const std::wstring& attributeName) {
    CComPtr<IXMLDOMNode> attrNode;
    HRESULT hr = attributes->getNamedItem(_bstr_t(attributeName.c_str()), &attrNode);
    if (SUCCEEDED(hr) && attrNode) {
        _bstr_t value;
        attrNode->get_text(value.GetAddress());
        return std::string((char*)value);
    }
    return "";
}

std::vector<std::string> PackageSupersedenceManager::findManifestFiles(const std::string& directory) {
    std::vector<std::string> manifestFiles;
    
    try {
        WIN32_FIND_DATAA findData;
        std::string searchPath = directory + "\\*.mum";
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    manifestFiles.push_back(directory + "\\" + findData.cFileName);
                }
            } while (FindNextFileA(hFind, &findData));
            
            FindClose(hFind);
        }
        
    } catch (const std::exception& ex) {
        logError("Exception finding manifest files: " + std::string(ex.what()));
    }
    
    return manifestFiles;
}

void PackageSupersedenceManager::logMessage(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    std::tm timeinfo;
    if (localtime_s(&timeinfo, &time_t) == 0) {
        ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    }
    ss << " [INFO] " << message << std::endl;
    
    detailedLog += ss.str();
    
    if (verboseLogging) {
        std::cout << ss.str();
    }
}

void PackageSupersedenceManager::logError(const std::string& error) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    std::tm timeinfo;
    if (localtime_s(&timeinfo, &time_t) == 0) {
        ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    }
    ss << " [ERROR] " << error << std::endl;
    
    detailedLog += ss.str();
    std::cerr << ss.str();
}

void PackageSupersedenceManager::logVerbose(const std::string& message) {
    if (verboseLogging) {
        logMessage("[VERBOSE] " + message);
    }
}

void PackageSupersedenceManager::setLastError(const std::string& error) {
    lastError = error;
    logError(error);
}

std::string PackageSupersedenceManager::getLastError() const {
    return lastError;
}

// PackageUtils namespace implementation
namespace PackageUtils {
    std::string normalizeVersionString(const std::string& version) {
        std::regex versionRegex(R"((\d+(?:\.\d+)*))");
        std::smatch match;
        
        if (std::regex_search(version, match, versionRegex)) {
            return match[1].str();
        }
        
        return version;
    }
    
    bool isVersionStringValid(const std::string& version) {
        std::regex versionPattern(R"(^\d+(?:\.\d+){0,3}$)");
        return std::regex_match(version, versionPattern);
    }
    
    int compareSemanticVersions(const std::string& v1, const std::string& v2) {
        auto parseVersion = [](const std::string& version) {
            std::vector<int> parts;
            std::stringstream ss(version);
            std::string part;
            
            while (std::getline(ss, part, '.')) {
                try {
                    parts.push_back(std::stoi(part));
                } catch (...) {
                    parts.push_back(0);
                }
            }
            
            while (parts.size() < 4) {
                parts.push_back(0);
            }
            
            return parts;
        };
        
        auto parts1 = parseVersion(v1);
        auto parts2 = parseVersion(v2);
        
        for (size_t i = 0; i < parts1.size() && i < parts2.size(); ++i) {
            if (parts1[i] != parts2[i]) {
                return (parts1[i] > parts2[i]) ? 1 : -1;
            }
        }
        
        return 0;
    }
    
    std::vector<std::string> getSupportedArchitectures() {
        return {"x86", "amd64", "arm", "arm64", "ia64", "neutral", "wow64"};
    }
    
    std::string normalizeArchitecture(const std::string& arch) {
        std::string normalized = arch;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
        
        if (normalized == "x64") normalized = "amd64";
        if (normalized == "any cpu") normalized = "neutral";
        
        return normalized;
    }
    
    bool isArchitectureCompatible(const std::string& packageArch, const std::string& systemArch) {
        std::string normalizedPackageArch = normalizeArchitecture(packageArch);
        std::string normalizedSystemArch = normalizeArchitecture(systemArch);
        
        if (normalizedPackageArch == "neutral") {
            return true;
        }
        
        if (normalizedPackageArch == normalizedSystemArch) {
            return true;
        }
        
        if (normalizedPackageArch == "x86" && normalizedSystemArch == "amd64") {
            return true;
        }
        
        return false;
    }
    
    std::string getCurrentSystemArchitecture() {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        
        switch (sysInfo.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64:
                return "amd64";
            case PROCESSOR_ARCHITECTURE_ARM:
                return "arm";
            case PROCESSOR_ARCHITECTURE_ARM64:
                return "arm64";
            case PROCESSOR_ARCHITECTURE_INTEL:
                return "x86";
            case PROCESSOR_ARCHITECTURE_IA64:
                return "ia64";
            default:
                return "unknown";
        }
    }
    
    std::string getCurrentWindowsVersion() {
        return "10.0.19041"; // Simplified version
    }
    
    bool isSystemCompatibleWithPackage(const PackageIdentity& package) {
        std::string systemArch = getCurrentSystemArchitecture();
        return isArchitectureCompatible(package.architecture, systemArch);
    }
    
    void logPackageOperation(const std::string& operation, const PackageIdentity& package,
                           const std::string& result, const std::string& logPath) {
        try {
            std::ofstream logFile(logPath, std::ios::app);
            if (logFile.is_open()) {
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);
                
                std::tm timeinfo;
                if (localtime_s(&timeinfo, &time_t) == 0) {
                    logFile << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
                }
                logFile << " - Operation: " << operation 
                       << " - Package: " << package.getShortIdentity()
                       << " - Result: " << result << std::endl;
            }
        } catch (...) {
            // Suppress logging exceptions
        }
    }
}

// Stub implementations for missing methods
bool PackageSupersedenceManager::queryInstalledPackagesFromRegistry() { return true; }
bool PackageSupersedenceManager::queryInstalledPackagesFromCBS() { return true; }
bool PackageSupersedenceManager::queryStagedPackagesFromStore() { return true; }

PackageState PackageSupersedenceManager::queryPackageStateFromSystem(const PackageIdentity& identity) {
    PackageState state;
    state.identity = identity;
    state.state = PackageInstallState::NotPresent;
    state.lastStateChange = std::chrono::system_clock::now();
    return state;
}

void PackageSupersedenceManager::buildSupersedenceDatabase() {
    // Implementation for building supersedence relationships
}

void PackageSupersedenceManager::detectImplicitSupersedence() {
    // Implementation for detecting implicit supersedence
}

std::string PackageSupersedenceManager::packageStateToString(PackageInstallState state) {
    switch (state) {
        case PackageInstallState::NotPresent: return "NotPresent";
        case PackageInstallState::Staged: return "Staged";
        case PackageInstallState::Installed: return "Installed";
        case PackageInstallState::PartiallyInstalled: return "PartiallyInstalled";
        case PackageInstallState::Superseded: return "Superseded";
        case PackageInstallState::Pending: return "Pending";
        case PackageInstallState::Failed: return "Failed";
        case PackageInstallState::Corrupted: return "Corrupted";
        default: return "Unknown";
    }
}

std::string PackageSupersedenceManager::installDecisionToString(InstallDecision decision) {
    switch (decision) {
        case InstallDecision::Install: return "Install";
        case InstallDecision::Skip_AlreadyInstalled: return "Skip_AlreadyInstalled";
        case InstallDecision::Skip_Superseded: return "Skip_Superseded";
        case InstallDecision::Skip_NewerExists: return "Skip_NewerExists";
        case InstallDecision::Skip_Incompatible: return "Skip_Incompatible";
        case InstallDecision::Skip_Failed: return "Skip_Failed";
        case InstallDecision::Repair_Corrupted: return "Repair_Corrupted";
        case InstallDecision::Update_Available: return "Update_Available";
        case InstallDecision::Downgrade_Blocked: return "Downgrade_Blocked";
        default: return "Unknown";
    }
}

std::string PackageSupersedenceManager::getManifestHash(const std::string& filePath) {
    return std::to_string(std::hash<std::string>{}(filePath));
}

bool PackageSupersedenceManager::scanSystemForPackages() {
    packageStates.clear();
    return true;
}

std::vector<PackageState> PackageSupersedenceManager::getInstalledPackages() {
    std::vector<PackageState> installed;
    for (const auto& pair : packageStates) {
        if (pair.second.state == PackageInstallState::Installed) {
            installed.push_back(pair.second);
        }
    }
    return installed;
}

std::vector<PackageState> PackageSupersedenceManager::getStagedPackages() {
    std::vector<PackageState> staged;
    for (const auto& pair : packageStates) {
        if (pair.second.state == PackageInstallState::Staged) {
            staged.push_back(pair.second);
        }
    }
    return staged;
}

std::vector<PackageState> PackageSupersedenceManager::getCorruptedPackages() {
    std::vector<PackageState> corrupted;
    for (const auto& pair : packageStates) {
        if (pair.second.state == PackageInstallState::Corrupted) {
            corrupted.push_back(pair.second);
        }
    }
    return corrupted;
}

std::vector<PackageIdentity> PackageSupersedenceManager::findNewerVersionsOfPackage(const PackageIdentity& package) {
    std::vector<PackageIdentity> newerVersions;
    return newerVersions;
}

std::vector<PackageIdentity> PackageSupersedenceManager::findConflictingPackages(const PackageIdentity& package) {
    std::vector<PackageIdentity> conflicts;
    return conflicts;
}

ComponentManifest* PackageSupersedenceManager::findManifestForPackage(const PackageIdentity& package) {
    return nullptr;
}

std::string PackageSupersedenceManager::assessInstallationRisk(const PackageIdentity& package) {
    return "Low";
}

bool PackageSupersedenceManager::hasTransitiveDependency(const PackageIdentity& package1, const PackageIdentity& package2) {
    return false;
}

bool PackageSupersedenceManager::detectCircularDependencies(const std::vector<PackageIdentity>& packages) {
    return false;
}

std::vector<PackageIdentity> PackageSupersedenceManager::resolveInstallationChain(const PackageIdentity& rootPackage) {
    std::vector<PackageIdentity> chain;
    return chain;
}

std::vector<PackageIdentity> PackageSupersedenceManager::findUpdateCandidates() {
    std::vector<PackageIdentity> candidates;
    return candidates;
}

std::vector<PackageIdentity> PackageSupersedenceManager::findOrphanedPackages() {
    std::vector<PackageIdentity> orphaned;
    return orphaned;
}

bool PackageSupersedenceManager::parseIdentityFromFilename(const std::string& filename, PackageIdentity& identity) {
    try {
        // Windows .mum files often have the identity encoded in the filename
        // Format: ComponentName~PublicKeyToken~Architecture~Language~Version.mum
        // or: Package_for_KB123456~31bf3856ad364e35~amd64~~10.0.26100.1.mum
        
        std::string nameWithoutExt = filename;
        size_t dotPos = nameWithoutExt.find_last_of('.');
        if (dotPos != std::string::npos) {
            nameWithoutExt = nameWithoutExt.substr(0, dotPos);
        }
        
        // Split by ~ character
        std::vector<std::string> parts;
        std::stringstream ss(nameWithoutExt);
        std::string part;
        
        while (std::getline(ss, part, '~')) {
            parts.push_back(part);
        }
        
        if (parts.size() >= 5) {
            identity.name = parts[0];
            identity.publicKeyToken = parts[1];
            identity.architecture = parts[2];
            identity.language = parts[3];
            identity.version = parts[4];
            
            // Clean up name if it contains wrapper prefixes
            if (identity.name.find("Package_for_") == 0) {
                identity.name = identity.name.substr(12); // Remove "Package_for_"
            } else if (identity.name.find("Wrapper-") == 0) {
                // For wrapper packages, extract meaningful name from hash or use simplified name
                identity.name = "Windows-Update-Wrapper";
            }
            
            // Set defaults for empty fields
            if (identity.language.empty()) {
                identity.language = "neutral";
            }
            
            if (identity.architecture.empty()) {
                identity.architecture = "neutral";
            }
            
            return !identity.name.empty() && !identity.version.empty();
        }
        
        // Fallback: try to extract at least name and version from different patterns
        std::regex kbPattern(R"(KB(\d+))");
        std::smatch kbMatch;
        if (std::regex_search(nameWithoutExt, kbMatch, kbPattern)) {
            identity.name = "KB" + kbMatch[1].str();
            identity.version = "1.0.0.0"; // Default version for KB updates
            identity.architecture = "neutral";
            identity.language = "neutral";
            return true;
        }
        
        return false;
        
    } catch (const std::exception& ex) {
        logError("Exception parsing identity from filename: " + std::string(ex.what()));
        return false;
    }
}

bool PackageSupersedenceManager::parseManifestProperties(IXMLDOMElement* rootElement, ComponentManifest& manifest) {
    try {
        // Parse display name
        CComPtr<IXMLDOMNode> displayNameNode;
        HRESULT hr = rootElement->selectSingleNode(_bstr_t(L"//displayName"), &displayNameNode);
        if (SUCCEEDED(hr) && displayNameNode) {
            manifest.displayName = extractXmlNodeText(displayNameNode);
        }
        
        // Parse description
        CComPtr<IXMLDOMNode> descriptionNode;
        hr = rootElement->selectSingleNode(_bstr_t(L"//description"), &descriptionNode);
        if (SUCCEEDED(hr) && descriptionNode) {
            manifest.description = extractXmlNodeText(descriptionNode);
        }
        
        // Parse restart required
        CComPtr<IXMLDOMNode> restartNode;
        hr = rootElement->selectSingleNode(_bstr_t(L"//*[@restartRequired]"), &restartNode);
        if (SUCCEEDED(hr) && restartNode) {
            CComPtr<IXMLDOMNamedNodeMap> attributes;
            hr = restartNode->get_attributes(&attributes);
            if (SUCCEEDED(hr)) {
                manifest.restartRequired = getXmlAttributeValue(attributes, L"restartRequired");
            }
        }
        
        // Determine component type based on manifest content
        CComPtr<IXMLDOMNode> packageNode;
        hr = rootElement->selectSingleNode(_bstr_t(L"//package"), &packageNode);
        if (SUCCEEDED(hr) && packageNode) {
            manifest.componentType = "Package";
        } else {
            hr = rootElement->selectSingleNode(_bstr_t(L"//assembly"), &packageNode);
            if (SUCCEEDED(hr) && packageNode) {
                manifest.componentType = "Assembly";
            } else {
                manifest.componentType = "Component";
            }
        }
        
        return true;
        
    } catch (const std::exception& ex) {
        logError("Exception parsing manifest properties: " + std::string(ex.what()));
        return false;
    }
}

bool PackageSupersedenceManager::parseDependencies(IXMLDOMElement* rootElement, ComponentManifest& manifest) {
    try {
        // Parse dependencies from various possible locations
        std::vector<std::wstring> dependencyPaths = {
            L"//dependency/dependentAssembly/assemblyIdentity",
            L"//dependencies/dependency/assemblyIdentity",
            L"//package/update/package/dependencies/dependency/assemblyIdentity"
        };
        
        for (const auto& path : dependencyPaths) {
            auto dependencyNodes = selectXmlNodes(rootElement, path);
            
            for (auto& depNode : dependencyNodes) {
                PackageIdentity dependency;
                if (parsePackageIdentityFromXml(depNode, dependency)) {
                    manifest.dependencies.push_back(dependency);
                    logVerbose("Found dependency: " + dependency.getShortIdentity());
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& ex) {
        logError("Exception parsing dependencies: " + std::string(ex.what()));
        return false;
    }
}

bool PackageSupersedenceManager::parseSupersedence(IXMLDOMElement* rootElement, ComponentManifest& manifest) {
    try {
        // Parse supersedence information from update metadata
        std::vector<std::wstring> supersedencePaths = {
            L"//update/applicable/package/assemblyIdentity",
            L"//supersedes/package/assemblyIdentity",
            L"//replaces/package/assemblyIdentity"
        };
        
        for (const auto& path : supersedencePaths) {
            auto supersededNodes = selectXmlNodes(rootElement, path);
            
            for (auto& supNode : supersededNodes) {
                PackageIdentity superseded;
                if (parsePackageIdentityFromXml(supNode, superseded)) {
                    manifest.supersedes.push_back(superseded);
                    logVerbose("Found superseded package: " + superseded.getShortIdentity());
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& ex) {
        logError("Exception parsing supersedence: " + std::string(ex.what()));
        return false;
    }
}

std::string PackageSupersedenceManager::extractXmlNodeText(IXMLDOMNode* node) {
    if (!node) return "";
    
    try {
        _bstr_t nodeText;
        HRESULT hr = node->get_text(nodeText.GetAddress());
        if (SUCCEEDED(hr)) {
            return std::string((char*)nodeText);
        }
    } catch (...) {
        // Ignore extraction errors
    }
    
    return "";
}

std::vector<CComPtr<IXMLDOMNode>> PackageSupersedenceManager::selectXmlNodes(IXMLDOMElement* element, const std::wstring& xpath) {
    std::vector<CComPtr<IXMLDOMNode>> nodes;
    
    try {
        CComPtr<IXMLDOMNodeList> nodeList;
        HRESULT hr = element->selectNodes(_bstr_t(xpath.c_str()), &nodeList);
        
        if (SUCCEEDED(hr) && nodeList) {
            long length = 0;
            nodeList->get_length(&length);
            
            for (long i = 0; i < length; i++) {
                CComPtr<IXMLDOMNode> node;
                hr = nodeList->get_item(i, &node);
                if (SUCCEEDED(hr) && node) {
                    nodes.push_back(node);
                }
            }
        }
    } catch (...) {
        // Ignore selection errors
    }
    
    return nodes;
}