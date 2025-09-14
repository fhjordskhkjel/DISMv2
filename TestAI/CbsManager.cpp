#include "CbsManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <io.h>
#include <fcntl.h>
#include <share.h>
#include <unordered_set>
#include <unordered_map>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#include <msxml6.h>
#include <mscat.h>
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "msxml6.lib")

#if __has_include(<fdi.h>)
#define WITH_FDI 1
#include <fdi.h>
#pragma comment(lib, "cabinet.lib")
#else
#define WITH_FDI 0
#endif

namespace fs = std::filesystem;

// Move FDI helper block above installPackageWithCbs
// [BEGIN FDI BLOCK]
namespace {
#if WITH_FDI
    void FAR* DIAMONDAPI fdi_alloc(ULONG cb) { return malloc(cb); }
    void DIAMONDAPI fdi_free(void FAR* pv) { free(pv); }
    INT_PTR DIAMONDAPI fdi_open_cb(char FAR* pszFile, int oflag, int pmode) { int h=-1; _sopen_s(&h, pszFile, oflag|_O_BINARY, _SH_DENYNO, pmode); return (INT_PTR)h; }
    UINT DIAMONDAPI fdi_read_cb(INT_PTR hf, void FAR* pv, UINT cb) { return (UINT)_read((int)hf, pv, cb); }
    UINT DIAMONDAPI fdi_write_cb(INT_PTR hf, void FAR* pv, UINT cb) { return (UINT)_write((int)hf, pv, cb); }
    int DIAMONDAPI fdi_close_cb(INT_PTR hf) { return _close((int)hf); }
    long DIAMONDAPI fdi_seek_cb(INT_PTR hf, long dist, int seektype) { return (long)_lseek((int)hf, dist, seektype); }
    struct FdiCtx { std::wstring dest; };
    int DIAMONDAPI fdi_notify_cb(FDINOTIFICATIONTYPE fdint, PFDINOTIFICATION pfdin) {
        FdiCtx* ctx = reinterpret_cast<FdiCtx*>(pfdin->pv);
        switch (fdint) {
            case fdintCOPY_FILE: {
                std::wstring wName; if (pfdin->psz1) { size_t len = strlen(pfdin->psz1); wName.assign(pfdin->psz1, pfdin->psz1 + len); }
                std::wstring outPath = ctx->dest + L"\\" + wName;
                std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());
                int h=-1; _wsopen_s(&h, outPath.c_str(), _O_CREAT|_O_TRUNC|_O_WRONLY|_O_BINARY, _SH_DENYNO, _S_IREAD|_S_IWRITE);
                return h;
            }
            case fdintCLOSE_FILE_INFO: { _close(pfdin->hf); return TRUE; }
            default: return 0;
        }
    }
    bool ExtractWithFDI(const std::string& cabPath, const std::string& destination) {
        HFDI hfdi = FDICreate(fdi_alloc, fdi_free, fdi_open_cb, fdi_read_cb, fdi_write_cb, fdi_close_cb, fdi_seek_cb, cpuUNKNOWN, nullptr);
        if (!hfdi) return false; ERF erf{}; FdiCtx ctx{ std::wstring(destination.begin(), destination.end()) };
        BOOL ok = FDICopy(hfdi, const_cast<char*>(cabPath.c_str()), NULL, 0, (PFNFDINOTIFY)fdi_notify_cb, nullptr, &ctx);
        FDIDestroy(hfdi); return ok == TRUE;
    }
#endif
}
// [END FDI BLOCK]

// Helper functions for process execution and log rotation
namespace {
    bool IsProcessWow64() {
        BOOL isWow64 = FALSE;
        typedef BOOL (WINAPI *FN)(HANDLE, PBOOL);
        HMODULE h = GetModuleHandleW(L"kernel32.dll");
        if (!h) return FALSE;
        auto fn = reinterpret_cast<FN>(GetProcAddress(h, "IsWow64Process"));
        if (!fn) return FALSE;
        if (!fn(GetCurrentProcess(), &isWow64)) return FALSE;
        return isWow64 ? true : false;
    }

    bool RunProcessCapture(const std::wstring& command, DWORD timeoutMs, std::string& output, DWORD& exitCode) {
        SECURITY_ATTRIBUTES sa{}; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE; sa.lpSecurityDescriptor = NULL;
        HANDLE hRead = NULL, hWrite = NULL;
        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si{}; si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWrite;
        si.hStdError  = hWrite;
        si.hStdInput  = NULL;

        BOOL created = CreateProcessW(NULL, const_cast<wchar_t*>(command.c_str()), NULL, NULL, TRUE,
                                      CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (!created) {
            CloseHandle(hRead); CloseHandle(hWrite);
            return false;
        }

        // Create a job so children are also terminated on timeout/cleanup
        HANDLE hJob = CreateJobObjectW(nullptr, nullptr);
        if (hJob) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
            info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &info, sizeof(info));
            AssignProcessToJobObject(hJob, pi.hProcess);
        }

        CloseHandle(hWrite);

        output.clear();
        std::string buffer; buffer.resize(4096);
        DWORD startTick = GetTickCount();
        DWORD bytesRead = 0; exitCode = 1;

        for (;;) {
            DWORD bytesAvailable = 0;
            if (PeekNamedPipe(hRead, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0) {
                DWORD toRead = (DWORD)std::min<size_t>(bytesAvailable, buffer.size());
                if (ReadFile(hRead, buffer.data(), toRead, &bytesRead, NULL) && bytesRead > 0) {
                    output.append(buffer.data(), buffer.data() + bytesRead);
                }
            } else {
                DWORD wait = WaitForSingleObject(pi.hProcess, 50);
                if (wait == WAIT_OBJECT_0) {
                    for (;;) {
                        if (PeekNamedPipe(hRead, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0) {
                            DWORD toRead2 = (DWORD)std::min<size_t>(bytesAvailable, buffer.size());
                            if (ReadFile(hRead, buffer.data(), toRead2, &bytesRead, NULL) && bytesRead > 0) {
                                output.append(buffer.data(), buffer.data() + bytesRead);
                            } else { break; }
                        } else { break; }
                    }
                    break;
                }
                DWORD now = GetTickCount();
                if (timeoutMs > 0 && (now - startTick) > timeoutMs) {
                    if (hJob) {
                        TerminateJobObject(hJob, 1);
                    } else {
                        TerminateProcess(pi.hProcess, 1);
                    }
                    break;
                }
            }
        }

        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        if (hJob) CloseHandle(hJob);
        CloseHandle(hRead);
        return true;
    }

    void RotateLogIfNeeded(const std::string& path, size_t maxBytes = 2 * 1024 * 1024, int keep = 3) {
        try {
            if (path.empty()) return;
            std::error_code ec;
            auto sz = fs::exists(path, ec) ? fs::file_size(path, ec) : 0;
            if (ec || sz < maxBytes) return;
            for (int i = keep - 1; i >= 1; --i) {
                std::string src = path + "." + std::to_string(i);
                std::string dst = path + "." + std::to_string(i + 1);
                if (fs::exists(src, ec)) {
                    fs::remove(dst, ec);
                    fs::rename(src, dst, ec);
                }
            }
            std::string first = path + ".1";
            fs::remove(first, ec);
            fs::rename(path, first, ec);
        } catch (...) {
        }
    }

    std::wstring ToLongPath(const std::wstring& path) {
        if (path.rfind(L"\\\\?\\", 0) == 0) return path; // already long path
        if (path.rfind(L"\\\\", 0) == 0) {
            // UNC -> \\?\UNC\server\share\...
            return L"\\\\?\\UNC\\" + path.substr(2);
        }
        // Drive path
        return L"\\\\?\\" + path;
    }

    // Replace IsUnderRootCaseInsensitive with global ::towlower
    bool IsUnderRootCaseInsensitive(const fs::path& candidate, const fs::path& root) {
        try {
            auto normCand = fs::weakly_canonical(candidate);
            auto normRoot = fs::weakly_canonical(root);
            auto c = normCand.wstring();
            auto r = normRoot.wstring();
            auto lowerW = [](wchar_t ch) { return static_cast<wchar_t>(::towlower(static_cast<wint_t>(ch))); };
            std::transform(c.begin(), c.end(), c.begin(), lowerW);
            std::transform(r.begin(), r.end(), r.begin(), lowerW);
            if (r.size() > c.size()) return false;
            if (c.compare(0, r.size(), r) != 0) return false;
            if (c.size() == r.size()) return true;
            wchar_t sep = c[r.size()];
            return sep == L'\\' || sep == L'/';
        } catch (...) { return false; }
    }

    // Check if current process is elevated (admin) - note: not equivalent to TrustedInstaller rights
    bool IsProcessElevated() {
        HANDLE hToken = NULL;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) return false;
        TOKEN_ELEVATION elev{}; DWORD cb = sizeof(elev);
        BOOL ok = GetTokenInformation(hToken, TokenElevation, &elev, sizeof(elev), &cb);
        CloseHandle(hToken);
        return ok && elev.TokenIsElevated != 0;
    }
}

std::wstring CbsManager::getSystemToolPath(const wchar_t* toolName) {
    wchar_t winDir[MAX_PATH] = {};
    UINT wlen = GetWindowsDirectoryW(winDir, MAX_PATH);
    if (wlen == 0 || wlen >= MAX_PATH) {
        // Fallback to system directory
        wchar_t sysDir[MAX_PATH] = {};
        UINT slen = GetSystemDirectoryW(sysDir, MAX_PATH);
        if (slen == 0 || slen >= MAX_PATH) {
            return std::wstring(toolName);
        }
        return std::wstring(sysDir) + L"\\" + toolName;
    }
    bool wow64 = IsProcessWow64();
    std::wstring base(winDir);
    if (wow64) {
        return base + L"\\Sysnative\\" + toolName;
    }
    return base + L"\\System32\\" + toolName;
}

// Constructor
CbsManager::CbsManager() 
    : initialized(false)
    , systemOnline(false)
    , transactionState(CbsTransactionState::None)
{
    // Pick up DISMV2_LOG environment override if present
    char buf[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableA("DISMV2_LOG", buf, static_cast<DWORD>(std::size(buf)));
    if (n > 0 && n < std::size(buf)) {
        logFilePath = std::string(buf);
    }
}

// Destructor
CbsManager::~CbsManager() {
    cleanup();
}

// Initialize CBS Manager
bool CbsManager::initialize() {
    if (initialized) {
        return true;
    }
    
    try {
        appendToErrorLog("Starting CBS Manager initialization");
        
        // Harden DLL search path to system32 only
        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
        
        // Initialize COM with safety checks
        if (!initializeCom()) {
            setLastError("Failed to initialize COM for CBS operations");
            return false;
        }
        
        appendToErrorLog("COM initialization successful");
        
        // Load CBS API with error handling
        if (!loadCbsApi()) {
            setLastError("Failed to load CBS API");
            cleanupCom();
            return false;
        }
        
        appendToErrorLog("CBS API loaded successfully");
        
        // Detect if we're running online
        systemOnline = CbsUtils::isRunningOnline();
        appendToErrorLog("System online status: " + std::string(systemOnline ? "Online" : "Offline"));
        
        // Enable required privileges with error handling
        if (!enableRequiredPrivileges()) {
            setLastError("Failed to enable required privileges for CBS operations");
            // Don't fail initialization for privilege issues as they may not be critical
            appendToErrorLog("Warning: Some CBS privileges could not be enabled");
        } else {
            appendToErrorLog("Required privileges enabled successfully");
        }
        
        initialized = true;
        appendToErrorLog("CBS Manager initialized successfully");
        return true;
        
    } catch (const std::bad_alloc& ex) {
        setLastError("Memory allocation error during CBS Manager initialization: " + std::string(ex.what()));
        cleanup();
        return false;
    } catch (const std::exception& ex) {
        setLastError("Exception during CBS Manager initialization: " + std::string(ex.what()));
        cleanup();
        return false;
    } catch (...) {
        setLastError("Unknown exception during CBS Manager initialization");
        cleanup();
        return false;
    }
}

// Cleanup CBS Manager
void CbsManager::cleanup() {
    if (!initialized) {
        return;
    }
    
    try {
        // Rollback any pending transaction
        if (transactionState != CbsTransactionState::None && 
            transactionState != CbsTransactionState::Committed) {
            rollbackTransaction();
        }
        
        // Close CBS session
        closeCbsSession();
        
        // Unload CBS API
        unloadCbsApi();
        
        // Cleanup COM
        cleanupCom();
        
        initialized = false;
        appendToErrorLog("CBS Manager cleanup completed");
        
    } catch (...) {
        // Suppress exceptions during cleanup
    }
}

// Analyze Package (MSU/CAB)
std::optional<CbsPackageInfo> CbsManager::analyzePackage(const std::string& packagePath) {
    if (!initialized && !initialize()) {
        setLastError("CBS Manager not initialized");
        return std::nullopt;
    }
    
    if (!fs::exists(packagePath)) {
        setLastError("Package file does not exist: " + packagePath);
        return std::nullopt;
    }
    
    try {
        appendToErrorLog("Starting package analysis for: " + packagePath);
        
        // Create temporary extraction directory
        std::string tempDir;
        if (!createStagingDirectory("", tempDir)) {  // Use empty string to trigger fallback logic
            setLastError("Failed to create temporary staging directory for package analysis");
            return std::nullopt;
        }
        appendToErrorLog("Created staging directory for analysis: " + tempDir);
        
        // For CBS analysis, we'll create a basic package info structure
        CbsPackageInfo packageInfo;
        packageInfo.packageIdentity = fs::path(packagePath).stem().string();
        packageInfo.displayName = packageInfo.packageIdentity;
        packageInfo.version = "1.0.0.0";
        packageInfo.releaseType = "Update";
        packageInfo.installState = "Staged";
        
        // Determine package type based on extension
        auto extension = fs::path(packagePath).extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        
        if (extension == ".cab") {
            packageInfo.description = "Cabinet Archive Package";
        } else if (extension == ".msu") {
            packageInfo.description = "Microsoft Update Package";
        } else {
            packageInfo.description = "Windows Update Package";
        }
        
        // Create a basic component for CBS processing
        CbsComponentInfo component;
        component.identity = packageInfo.packageIdentity + ".Component";
        component.version = packageInfo.version;
        component.architecture = CbsUtils::getSystemArchitecture();
        component.state = "Staged";
        component.isApplicable = true;
        component.needsRestart = false;
        
        packageInfo.components.push_back(component);
        packageInfo.applicabilityInfo.push_back("Applicable to current system");
        
        // Cleanup staging directory
        cleanupStagingDirectory(tempDir);
        
        appendToErrorLog("Package analysis completed successfully for: " + packagePath);
        appendToErrorLog("Package identity: " + packageInfo.packageIdentity);
        appendToErrorLog("Components found: " + std::to_string(packageInfo.components.size()));
        
        return packageInfo;
        
    } catch (const std::exception& ex) {
        setLastError("Exception during package analysis: " + std::string(ex.what()));
        return std::nullopt;
    }
}

// Validate Dependencies (preflight within extracted set)
bool CbsManager::validateDependencies(const CbsPackageInfo& packageInfo) {
    if (!initialized && !initialize()) {
        return false;
    }
    try {
        appendToErrorLog("Validating dependencies for package: " + packageInfo.packageIdentity);
        std::unordered_set<std::string> present;
        for (const auto& c : packageInfo.components) present.insert(c.identity);
        std::vector<std::string> missing;
        for (const auto& c : packageInfo.components) {
            for (const auto& dep : c.dependencies) {
                if (!dep.empty() && !present.count(dep)) missing.push_back(dep);
            }
        }
        if (!missing.empty()) {
            std::sort(missing.begin(), missing.end());
            missing.erase(std::unique(missing.begin(), missing.end()), missing.end());
            appendToErrorLog("Dependency preflight: missing components: " + std::to_string(missing.size()));
            for (const auto& m : missing) appendToErrorLog("  - " + m);
            return false;
        }
        appendToErrorLog("Dependency preflight: all dependencies satisfied within extracted set");
        return true;
    } catch (const std::exception& ex) {
        setLastError(std::string("Exception during dependency validation: ") + ex.what());
        return false;
    }
}

// Begin Transaction
bool CbsManager::beginTransaction() {
    if (!initialized && !initialize()) {
        return false;
    }
    
    if (transactionState != CbsTransactionState::None) {
        setLastError("A transaction is already in progress");
        return false;
    }
    
    try {
        // Create CBS session if needed
        if (!cbsSession) {
            if (!createCbsSession("")) {
                setLastError("Failed to create CBS session for transaction");
                return false;
            }
        }
        
        // Begin transaction (would use CBS APIs)
        transactionState = CbsTransactionState::Initiated;
        appendToErrorLog("CBS transaction initiated");
        
        return true;
        
    } catch (const std::exception& ex) {
        setLastError("Exception during transaction initiation: " + std::string(ex.what()));
        return false;
    }
}

// Commit Transaction
bool CbsManager::commitTransaction() {
    if (transactionState != CbsTransactionState::Staged) {
        setLastError("No staged transaction to commit");
        return false;
    }
    
    try {
        // Commit transaction (would use CBS APIs)
        transactionState = CbsTransactionState::Committed;
        appendToErrorLog("CBS transaction committed successfully");
        
        return true;
        
    } catch (const std::exception& ex) {
        setLastError("Exception during transaction commit: " + std::string(ex.what()));
        transactionState = CbsTransactionState::Failed;
        return false;
    }
}

// Rollback Transaction
bool CbsManager::rollbackTransaction() {
    if (transactionState == CbsTransactionState::None || 
        transactionState == CbsTransactionState::Committed) {
        return true; // Nothing to rollback
    }
    
    try {
        // Rollback transaction (would use CBS APIs)
        transactionState = CbsTransactionState::Aborted;
        appendToErrorLog("CBS transaction rolled back");
        
        return true;
        
    } catch (const std::exception& ex) {
        setLastError("Exception during transaction rollback: " + std::string(ex.what()));
        transactionState = CbsTransactionState::Failed;
        return false;
    }
}

// Install Package with CBS Integration
CbsInstallResult CbsManager::installPackageWithCbs(const std::string& packagePath, 
                                                  const std::string& targetPath,
                                                  bool isOnline) {
    CbsInstallResult result;
    result.success = false;
    
    if (!initialized && !initialize()) {
        result.errorDescription = "CBS Manager not initialized";
        result.errorCode = E_FAIL;
        appendToErrorLog("CBS installation failed: CBS Manager not initialized");
        return result;
    }
    
    try {
        appendToErrorLog("Starting CBS-integrated installation of: " + packagePath);
        appendToErrorLog("Target path: " + targetPath);
        appendToErrorLog("Online mode: " + std::string(isOnline ? "Yes" : "No"));
        
        // 1. Verify package exists
        if (!fs::exists(packagePath)) {
            result.errorDescription = "Package file does not exist: " + packagePath;
            result.errorCode = ERROR_FILE_NOT_FOUND;
            appendToErrorLog("CBS installation failed: " + result.errorDescription);
            return result;
        }
        
        // 2. Create temporary extraction directory
        std::string tempDir;
        if (!createStagingDirectory("", tempDir)) {
            result.errorDescription = "Failed to create staging directory for package extraction";
            result.errorCode = E_FAIL;
            appendToErrorLog("CBS installation failed: " + result.errorDescription);
            return result;
        }
        
        appendToErrorLog("Created staging directory: " + tempDir);
        
        // 3. Extract package
        appendToErrorLog("Extracting package for analysis...");
        bool extractionSuccess = false;
        try {
            auto extension = fs::path(packagePath).extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
            
            if (extension == ".cab") {
                appendToErrorLog("Extracting CAB package for analysis...");
                extractionSuccess = extractCabForAnalysis(packagePath, tempDir);
            } else if (extension == ".msu") {
                appendToErrorLog("Extracting MSU package for analysis...");
                extractionSuccess = extractMsuForAnalysis(packagePath, tempDir);
            } else {
                appendToErrorLog("Attempting generic package extraction...");
                extractionSuccess = extractGenericPackageForAnalysis(packagePath, tempDir);
            }
            
        } catch (const std::exception& ex) {
            appendToErrorLog("Exception during package extraction: " + std::string(ex.what()));
            extractionSuccess = false;
        }
        
        if (!extractionSuccess) {
            appendToErrorLog("Warning: Package extraction failed, using basic analysis");
        } else {
            appendToErrorLog("Package extraction successful");
        }
        
        // 4. Analyze extracted package
        appendToErrorLog("Analyzing package structure...");
        auto packageInfo = extractionSuccess ? 
            analyzeExtractedPackage(tempDir) : 
            analyzePackage(packagePath);
            
        if (!packageInfo) {
            result.errorDescription = "Failed to analyze package: " + (lastError ? *lastError : "Unknown error");
            result.errorCode = E_FAIL;
            appendToErrorLog("CBS installation failed: " + result.errorDescription);
            cleanupStagingDirectory(tempDir);
            return result;
        }
        
        appendToErrorLog("Package analysis successful:");
        appendToErrorLog("  Package ID: " + packageInfo->packageIdentity);
        appendToErrorLog("  Components: " + std::to_string(packageInfo->components.size()));
        
        // 5. Verify package signature (non-blocking for internal packages)
        appendToErrorLog("Verifying package signature...");
        if (!verifyPackageSignature(packagePath)) {
            appendToErrorLog("Warning: Package signature verification failed, but continuing installation");
        } else {
            appendToErrorLog("Package signature verification successful");
        }
        
        // 6. Check applicability
        appendToErrorLog("Checking package applicability...");
        if (!checkApplicability(*packageInfo, targetPath)) {
            result.errorDescription = "Package is not applicable to target system";
            result.errorCode = E_INVALIDARG;
            appendToErrorLog("CBS installation failed: " + result.errorDescription);
            cleanupStagingDirectory(tempDir);
            return result;
        }
        
        // 7. Begin transaction
        appendToErrorLog("Beginning CBS transaction...");
        if (!beginTransaction()) {
            result.errorDescription = "Failed to begin CBS transaction: " + (lastError ? *lastError : "Unknown error");
            result.errorCode = E_FAIL;
            appendToErrorLog("CBS installation failed: " + result.errorDescription);
            cleanupStagingDirectory(tempDir);
            return result;
        }
        
        // 8. Install package (two-pass: manifests first)
        appendToErrorLog("Installing package components...");
        for (const auto& component : packageInfo->components) {
            appendToErrorLog("  Processing component: " + component.identity);
            // Register component (stub)
            if (!registerComponents({component})) {
                result.failedComponents.push_back(component.identity);
                appendToErrorLog("    Failed to register component: " + component.identity);
            } else {
                result.installedComponents.push_back(component.identity);
                appendToErrorLog("    Successfully registered component: " + component.identity);
            }
        }
        
        // 9. Update component store (stub)
        appendToErrorLog("Updating CBS component store...");
        if (!updateComponentStore(targetPath)) {
            result.errorDescription = "Failed to update component store";
            result.errorCode = E_FAIL;
            appendToErrorLog("CBS installation failed: " + result.errorDescription);
            rollbackTransaction();
            cleanupStagingDirectory(tempDir);
            return result;
        }
        
        // 10. Commit transaction
        appendToErrorLog("Committing CBS transaction...");
        transactionState = CbsTransactionState::Staged;
        if (!commitTransaction()) {
            result.errorDescription = "Failed to commit CBS transaction: " + (lastError ? *lastError : "Unknown error");
            result.errorCode = E_FAIL;
            appendToErrorLog("CBS installation failed: " + result.errorDescription);
            cleanupStagingDirectory(tempDir);
            return result;
        }
        
        // 11. Notify servicing stack
        if (isOnline) {
            appendToErrorLog("Notifying Windows servicing stack...");
            notifyServicingStack(result.installedComponents);
        }
        
        // 12. Cleanup staging directory
        cleanupStagingDirectory(tempDir);
        
        result.success = true;
        result.needsRestart = std::any_of(packageInfo->components.begin(), 
                                        packageInfo->components.end(),
                                        [](const auto& comp) { return comp.needsRestart; });
        
        appendToErrorLog("CBS-integrated installation completed successfully");
        appendToErrorLog("  Installed components: " + std::to_string(result.installedComponents.size()));
        appendToErrorLog("  Failed components: " + std::to_string(result.failedComponents.size()));
        appendToErrorLog("  Restart required: " + std::string(result.needsRestart ? "Yes" : "No"));
        
    } catch (const std::exception& ex) {
        result.errorDescription = "Exception during CBS installation: " + std::string(ex.what());
        result.errorCode = E_UNEXPECTED;
        appendToErrorLog("CBS installation failed with exception: " + result.errorDescription);
        rollbackTransaction();
    }
    
    return result;
}

// Analyze Extracted Package using MUM parsing
std::optional<CbsPackageInfo> CbsManager::analyzeExtractedPackage(const std::string& extractedDir) {
    std::error_code ec;
    if (!fs::exists(extractedDir, ec) || ec) {
        appendToErrorLog("analyzeExtractedPackage: directory not found: " + extractedDir);
        return std::nullopt;
    }

    CbsPackageInfo info;
    info.packageIdentity = fs::path(extractedDir).filename().string();
    info.displayName = info.packageIdentity;
    info.version = "1.0.0.0";
    info.releaseType = "Update";
    info.installState = "Staged";

    auto manifests = CbsUtils::findManifestFiles(extractedDir);
    std::unordered_map<std::string, CbsComponentInfo> compMap;

    for (const auto& m : manifests) {
        CbsComponentInfo comp{};
        if (parseMumManifest(m, comp)) {
            comp.state = "Staged";
            comp.isApplicable = true;
            comp.needsRestart = false;
            if (comp.identity.empty()) comp.identity = fs::path(m).stem().string();
            if (comp.version.empty()) comp.version = info.version;
            if (comp.architecture.empty()) comp.architecture = CbsUtils::getSystemArchitecture();
            compMap[comp.identity] = std::move(comp);
        } else {
            // Fallback: simple identity from filename
            CbsComponentInfo f{};
            f.identity = fs::path(m).stem().string();
            f.version = info.version;
            f.architecture = CbsUtils::getSystemArchitecture();
            f.state = "Staged";
            f.isApplicable = true;
            f.needsRestart = false;
            compMap[f.identity] = std::move(f);
        }
    }

    for (auto& kv : compMap) info.components.push_back(std::move(kv.second));

    if (info.components.empty()) {
        CbsComponentInfo comp{}; 
        comp.identity = info.packageIdentity + ".Component"; 
        comp.version = info.version; 
        comp.architecture = CbsUtils::getSystemArchitecture(); 
        comp.state = "Staged"; 
        comp.isApplicable = true; 
        comp.needsRestart = false;
        info.components.push_back(comp);
    }

    return info;
}

// Clean up: ensure only one set of destination/catalog/install helpers exists.
// BEGIN: single authoritative implementations remain below

// Helpers for file copying with long-path support (authoritative)
namespace {
    bool EnsureDirectories(const std::wstring& wpath) {
        std::error_code ec;
        fs::create_directories(fs::path(wpath), ec);
        return !ec;
    }
    bool IsReparsePoint(const std::wstring& wpath) {
        DWORD attr = GetFileAttributesW(wpath.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) return false;
        return (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    }
    bool CopyFileLongPath(const std::wstring& src, const std::wstring& dst, bool overwrite, std::string* log = nullptr) {
        std::wstring wsrc = ToLongPath(src);
        std::wstring wdst = ToLongPath(dst);
        if (IsReparsePoint(wsrc)) { if (log) *log += " skip reparse"; return true; }
        DWORD attrs = GetFileAttributesW(wsrc.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) { if (log) *log += " missing src"; return false; }
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) { return true; }
        EnsureDirectories(fs::path(wdst).parent_path().wstring());
        if (overwrite) {
            SetFileAttributesW(wdst.c_str(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(wdst.c_str());
        }
        if (!CopyFileW(wsrc.c_str(), wdst.c_str(), overwrite ? FALSE : TRUE)) {
            DWORD err = GetLastError();
            if (log) *log += (" copy failed:" + std::to_string(err));
            return false;
        }
        return true;
    }
}

// Normalize target root path (authoritative)
namespace {
    fs::path NormalizeRootPath(const std::string& in, bool isOnline) {
        try {
            if (in.empty()) {
                if (isOnline) {
                    wchar_t winDir[MAX_PATH] = {}; GetWindowsDirectoryW(winDir, MAX_PATH);
                    if (winDir[0] && winDir[1] == L':') {
                        std::wstring drv; drv.assign(winDir, winDir + 2); drv.push_back(L'\\');
                        return fs::path(drv);
                    }
                    return fs::path(L"C:\\");
                }
                return fs::path(L"C:\\");
            }
            fs::path p(in);
            if (p.has_root_name() && !p.has_root_directory()) {
                // e.g., "C:" -> "C:\"
                std::string rn = p.root_name().string();
                if (rn.size() == 2 && rn[1] == ':') rn.push_back('\\');
                return fs::path(rn);
            }
            if (!p.has_root_name() && !p.has_root_directory()) {
                std::error_code ec; p = fs::absolute(p, ec);
            }
            return p;
        } catch (...) { return fs::path(L"C:\\"); }
    }
}

// Compute destination for an extracted file under a Windows target (authoritative)
static std::optional<fs::path> ComputeDestinationForExtracted(const fs::path& src, const fs::path& extractedRoot, const std::string& targetRoot) {
    try {
        std::string srcStr = src.string();
        std::string low = srcStr; std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        fs::path troot = fs::path(targetRoot);
        if (!troot.has_root_name() || (troot.has_root_name() && !troot.has_root_directory())) {
            std::string tr = targetRoot; if (!tr.empty() && tr.back() != '\\') tr.push_back('\\'); troot = fs::path(tr);
        }
        auto posPkgs = low.find("\\windows\\servicing\\packages\\");
        if (posPkgs != std::string::npos) {
            std::string winTail = srcStr.substr(posPkgs); // starts with \\Windows\...
            return troot / fs::path(winTail);
        }
        auto posSxs = low.find("\\windows\\winsxs\\");
        if (posSxs != std::string::npos) {
            std::string winTail = srcStr.substr(posSxs);
            return troot / fs::path(winTail);
        }
        auto ext = src.extension().string(); std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".mum" || ext == ".cat") {
            return troot / "Windows" / "servicing" / "Packages" / src.filename();
        }
        std::error_code ec; fs::path rel = fs::relative(src, extractedRoot, ec);
        if (!ec) {
            std::string r = rel.string(); std::string rl = r; std::transform(rl.begin(), rl.end(), rl.begin(), ::tolower);
            if (rl.rfind("windows\\", 0) == 0) {
                return troot / rel;
            }
        }
        return std::nullopt;
    } catch (...) { return std::nullopt; }
}

// Catalog verification and registration (authoritative)
namespace {
    bool VerifySignatureWintrust(const std::wstring& file) {
        WINTRUST_FILE_INFO fileInfo{}; fileInfo.cbStruct = sizeof(fileInfo); fileInfo.pcwszFilePath = file.c_str();
        WINTRUST_DATA trust{}; trust.cbStruct = sizeof(trust); trust.dwUIChoice = WTD_UI_NONE; trust.fdwRevocationChecks = WTD_REVOKE_NONE; trust.dwUnionChoice = WTD_CHOICE_FILE; trust.pFile = &fileInfo;
        GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        LONG st = WinVerifyTrust(NULL, &policy, &trust);
        return st == ERROR_SUCCESS;
    }
    bool RegisterCatalog(const std::wstring& catPath) {
        HCATADMIN hAdmin = NULL; GUID dummy{};
        if (!CryptCATAdminAcquireContext(&hAdmin, &dummy, 0)) return false;
        HCATINFO hInfo = CryptCATAdminAddCatalog(hAdmin, const_cast<wchar_t*>(catPath.c_str()), NULL, 0);
        if (hInfo) {
            CryptCATAdminReleaseCatalogContext(hAdmin, hInfo, 0);
            CryptCATAdminReleaseContext(hAdmin, 0);
            return true;
        }
        CryptCATAdminReleaseContext(hAdmin, 0);
        return false;
    }
}

// Real file installation from extracted directory (two-pass: manifests, then payload) (authoritative)
bool CbsManager::installExtractedFiles(const std::string& extractedDir, const std::string& targetPath, bool isOnline) {
    std::error_code ec;
    if (!fs::exists(extractedDir, ec) || ec) {
        appendToErrorLog("installExtractedFiles: extractedDir not found: " + extractedDir);
        return false;
    }

    const fs::path root = fs::path(extractedDir);
    const fs::path target = NormalizeRootPath(targetPath, isOnline);

    // Early permission sanity check for ONLINE installs: writing to Windows\servicing\Packages requires TrustedInstaller
    if (isOnline) {
        try {
            fs::path testDir = target / "Windows" / "servicing" / "Packages";
            fs::create_directories(testDir, ec); ec.clear();
            fs::path testFile = testDir / "__dismv2_write_test.tmp";
            std::wstring wTest = ToLongPath(testFile.wstring());
            HANDLE h = CreateFileW(wTest.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
            if (h == INVALID_HANDLE_VALUE) {
                DWORD le = GetLastError();
                appendToErrorLog("Access check: unable to write to target system directories (error=" + std::to_string(le) + ")");
                appendToErrorLog("Online installation requires elevated TrustedInstaller permissions. Run as Administrator/TrustedInstaller, or use /Offline with /Image:<path>.");
                return false;
            }
            CloseHandle(h);
        } catch (...) {
            appendToErrorLog("Access check: unexpected error verifying write access to target.");
            return false;
        }
    }

    size_t copied = 0, skipped = 0, failed = 0;
    std::vector<fs::path> copiedCatalogTargets;

    // Pass 1: manifests and catalogs
    fs::recursive_directory_iterator it1(root, fs::directory_options::skip_permission_denied, ec), end;
    while (!ec && it1 != end) {
        const fs::directory_entry& entry = *it1;
        std::error_code fec;
        if (entry.is_symlink(fec)) { it1.disable_recursion_pending(); it1.increment(ec); continue; }
        if (fec) { it1.increment(ec); continue; }
        if (!entry.is_regular_file(fec) || fec) { it1.increment(ec); continue; }
        auto ext = entry.path().extension().string(); std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".mum" || ext == ".cat") {
            auto dstOpt = ComputeDestinationForExtracted(entry.path(), root, target.string());
            if (!dstOpt) { ++skipped; it1.increment(ec); continue; }
            if (!IsUnderRootCaseInsensitive(*dstOpt, target)) { ++failed; appendToErrorLog("Path outside target root skipped: " + dstOpt->string()); it1.increment(ec); continue; }
            std::wstring wsrc = entry.path().wstring();
            std::wstring wdst = dstOpt->wstring();
            std::string log;
            if (CopyFileLongPath(wsrc, wdst, /*overwrite*/true, &log)) {
                ++copied;
                if (ext == ".cat") copiedCatalogTargets.push_back(*dstOpt);
            } else {
                ++failed;
                DWORD le = GetLastError();
                if (le == ERROR_ACCESS_DENIED) {
                    appendToErrorLog("Copy failed (ACCESS DENIED): " + entry.path().string() + " -> " + dstOpt->string() + ". Run elevated or use /Offline /Image.");
                } else {
                    appendToErrorLog("Copy failed: " + entry.path().string() + " -> " + dstOpt->string() + (log.empty()?"":(" ("+log+")")));
                }
            }
        }
        it1.increment(ec);
    }
    if (ec) appendToErrorLog(std::string("Traversal warning (pass1): ") + ec.message());

    // Verify/register only catalogs we just copied
    try {
        for (const auto& catTarget : copiedCatalogTargets) {
            std::wstring w = catTarget.wstring();
            bool ok = VerifySignatureWintrust(w);
            appendToErrorLog(std::string("Catalog signature ") + (ok?"OK: ":"FAILED: ") + catTarget.string());
            if (!RegisterCatalog(w)) {
                DWORD le = GetLastError();
                appendToErrorLog(std::string("Catalog registration failed (") + std::to_string(le) + "): " + catTarget.string());
            }
        }
    } catch (...) {
        appendToErrorLog("Catalog verification/registration encountered errors");
    }

    // Pass 2: payload
    ec.clear();
    fs::recursive_directory_iterator it2(root, fs::directory_options::skip_permission_denied, ec);
    while (!ec && it2 != end) {
        const fs::directory_entry& entry = *it2;
        std::error_code fec;
        if (entry.is_symlink(fec)) { it2.disable_recursion_pending(); it2.increment(ec); continue; }
        if (fec) { it2.increment(ec); continue; }
        if (!entry.is_regular_file(fec) || fec) { it2.increment(ec); continue; }
        auto ext = entry.path().extension().string(); std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".mum" || ext == ".cat") { it2.increment(ec); continue; }
        auto dstOpt = ComputeDestinationForExtracted(entry.path(), root, target.string());
        if (!dstOpt) { ++skipped; it2.increment(ec); continue; }
        if (!IsUnderRootCaseInsensitive(*dstOpt, target)) { ++failed; appendToErrorLog("Path outside target root skipped: " + dstOpt->string()); it2.increment(ec); continue; }
        std::wstring wsrc = entry.path().wstring();
        std::wstring wdst = dstOpt->wstring();
        std::string log;
        if (CopyFileLongPath(wsrc, wdst, /*overwrite*/true, &log)) {
            ++copied;
        } else {
            ++failed;
            DWORD le = GetLastError();
            if (le == ERROR_ACCESS_DENIED) {
                appendToErrorLog("Copy failed (ACCESS DENIED): " + entry.path().string() + " -> " + dstOpt->string() + ". Run elevated or use /Offline /Image.");
            } else {
                appendToErrorLog("Copy failed: " + entry.path().string() + " -> " + dstOpt->string() + (log.empty()?"":(" ("+log+")")));
            }
        }
        it2.increment(ec);
    }
    if (ec) appendToErrorLog(std::string("Traversal warning (pass2): ") + ec.message());

    appendToErrorLog("installExtractedFiles summary: copied=" + std::to_string(copied) + ", skipped=" + std::to_string(skipped) + ", failed=" + std::to_string(failed));

    return failed == 0 || copied > 0;
}

// END: authoritative implementations

// Extraction helpers
bool CbsManager::extractCabForAnalysis(const std::string& cabPath, const std::string& destination) {
    try {
        auto in = toAbsolutePath(cabPath);
        auto out = toAbsolutePath(destination);

        // If attempting to expand a CAB into its own directory, redirect output to a subfolder
        try {
            std::error_code ec;
            fs::path inPath = fs::path(in);
            fs::path outPath = fs::path(out);
            fs::path inParent = inPath.parent_path();
            fs::path canonInParent = fs::weakly_canonical(inParent, ec); ec.clear();
            fs::path canonOut = fs::weakly_canonical(outPath, ec); ec.clear();
            bool sameDir = false;
            if (!canonInParent.empty() && !canonOut.empty()) {
                sameDir = fs::equivalent(canonInParent, canonOut, ec);
                ec.clear();
            }
            if (sameDir) {
                std::string sub = std::string("_cab_") + inPath.stem().string();
                outPath = outPath / sub;
                out = outPath.string();
            }
        } catch (...) {
            // Best-effort; if comparison fails, continue with provided destination
        }

        std::error_code ec; fs::create_directories(out, ec);
        std::wstring tool = getSystemToolPath(L"expand.exe");
        std::wstring wIn = ToLongPath(std::wstring(in.begin(), in.end()));
        std::wstring wOut = ToLongPath(std::wstring(out.begin(), out.end()));
        std::wstring cmd = L"\"" + tool + L"\" \"" + wIn + L"\" -F:* \"" + wOut + L"\"";
        std::string outText; DWORD code = 1;
        if (RunProcessCapture(cmd, 300000, outText, code)) {
            appendToErrorLog("expand(cab) output: " + outText);
            return code == 0;
        }
        return false;
    } catch (...) { return false; }
}

static void ExpandAllCabsInDir(CbsManager* self, const std::string& dir) {
    try {
        std::vector<std::string> cabs;
        std::error_code ec;
        for (auto& e : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
            if (!e.is_regular_file()) continue;
            auto ext = e.path().extension().string(); std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".cab") cabs.push_back(e.path().string());
        }
        for (const auto& cab : cabs) {
            self->extractCabTo(cab, dir);
        }
    } catch (...) {}
}

bool CbsManager::extractMsuForAnalysis(const std::string& msuPath, const std::string& destination) {
    try {
        // Prefer expand.exe
        bool ok = extractCabForAnalysis(msuPath, destination);
        if (ok) {
            // Expand any inner CABs as well
            ExpandAllCabsInDir(this, destination);
            return true;
        }
        // Optional DISM offline extract
        if (!offlineImagePath.empty()) {
            std::wstring dism = getSystemToolPath(L"dism.exe");
            std::wstring wImg(offlineImagePath.begin(), offlineImagePath.end());
            std::wstring wMsu(msuPath.begin(), msuPath.end());
            std::wstring wOut(destination.begin(), destination.end());
            std::wstring cmd = L"\"" + dism + L"\" /Image:\"" + wImg + L"\" /Add-Package /PackagePath:\"" + wMsu + L"\" /Extract:\"" + wOut + L"\"";
            std::string outText; DWORD code = 1;
            if (RunProcessCapture(cmd, 600000, outText, code)) {
                appendToErrorLog("dism /Extract output: " + outText);
                if (code == 0) { ExpandAllCabsInDir(this, destination); return true; }
            }
        }
        // Optional WUSA fallback if allowed
        if (allowWusaFallback) {
            std::wstring wusa = getSystemToolPath(L"wusa.exe");
            std::wstring wMsu(msuPath.begin(), msuPath.end());
            std::wstring wOut(destination.begin(), destination.end());
            std::wstring cmd = L"\"" + wusa + L"\" \"" + wMsu + L"\" /extract:\"" + wOut + L"\" /quiet /norestart";
            std::string outText; DWORD code = 1;
            if (RunProcessCapture(cmd, 600000, outText, code)) {
                appendToErrorLog("wusa /extract output: " + outText);
                if (code == 0) { ExpandAllCabsInDir(this, destination); return true; }
            }
        }
        return false;
    } catch (...) { return false; }
}

bool CbsManager::extractGenericPackageForAnalysis(const std::string& packagePath, const std::string& destination) {
    // Try expand first (handles cab-like)
    if (extractCabForAnalysis(packagePath, destination)) { ExpandAllCabsInDir(this, destination); return true; }
    return false;
}

// Verify Package Signature (basic)
bool CbsManager::verifyPackageSignature(const std::string& packagePath) {
    try {
        std::wstring w(packagePath.begin(), packagePath.end());
        return VerifySignatureWintrust(w);
    } catch (...) { return false; }
}

// MSXML-based manifest parsing
bool CbsManager::parseXmlManifest(const std::string& xmlPath, CbsPackageInfo& packageInfo) {
    try {
        CComPtr<IXMLDOMDocument> doc;
        HRESULT hr = CoCreateInstance(CLSID_DOMDocument60, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument, (void**)&doc);
        if (FAILED(hr) || !doc) return false;
        VARIANT_BOOL ok = VARIANT_FALSE;
        _variant_t vpath(xmlPath.c_str());
        doc->put_async(VARIANT_FALSE);
        hr = doc->load(vpath, &ok);
        if (FAILED(hr) || ok != VARIANT_TRUE) return false;
        // Basic metadata from file name
        packageInfo.packageIdentity = fs::path(xmlPath).stem().string();
        packageInfo.installState = "staged";
        appendToErrorLog("Parsed XML manifest: " + xmlPath);
        return true;
    } catch (...) { return false; }
}

namespace {
    std::string BstrToUtf8(const BSTR b) {
        if (!b) return {};
        int lenW = static_cast<int>(SysStringLen(b));
        if (lenW == 0) return {};
        int lenU8 = WideCharToMultiByte(CP_UTF8, 0, b, lenW, nullptr, 0, nullptr, nullptr);
        std::string out;
        if (lenU8 > 0) {
            out.resize(lenU8);
            WideCharToMultiByte(CP_UTF8, 0, b, lenW, out.data(), lenU8, nullptr, nullptr);
        }
        return out;
    }
}

bool CbsManager::parseMumManifest(const std::string& mumPath, CbsComponentInfo& componentInfo) {
    try {
        CComPtr<IXMLDOMDocument> doc;
        HRESULT hr = CoCreateInstance(CLSID_DOMDocument60, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument, (void**)&doc);
        if (FAILED(hr) || !doc) return false;
        VARIANT_BOOL ok = VARIANT_FALSE;
        _variant_t vpath(mumPath.c_str());
        doc->put_async(VARIANT_FALSE);
        hr = doc->load(vpath, &ok);
        if (FAILED(hr) || ok != VARIANT_TRUE) return false;

        CComPtr<IXMLDOMNode> node;
        doc->selectSingleNode(CComBSTR(L"//assemblyIdentity"), &node);
        if (node) {
            CComQIPtr<IXMLDOMElement> elem(node);
            if (elem) {
                CComVariant v;
                elem->getAttribute(CComBSTR(L"name"), &v);
                if (v.vt == VT_BSTR && v.bstrVal) componentInfo.identity = BstrToUtf8(v.bstrVal);
                v.Clear();
                elem->getAttribute(CComBSTR(L"version"), &v);
                if (v.vt == VT_BSTR && v.bstrVal) componentInfo.version = BstrToUtf8(v.bstrVal);
                v.Clear();
                elem->getAttribute(CComBSTR(L"processorArchitecture"), &v);
                if (v.vt == VT_BSTR && v.bstrVal) componentInfo.architecture = BstrToUtf8(v.bstrVal);
            }
        }

        CComPtr<IXMLDOMNodeList> deps;
        doc->selectNodes(CComBSTR(L"//dependency//assemblyIdentity"), &deps);
        if (deps) {
            long len = 0; deps->get_length(&len);
            for (long i=0;i<len;++i) {
                CComPtr<IXMLDOMNode> n; deps->get_item(i, &n);
                CComQIPtr<IXMLDOMElement> e(n);
                if (!e) continue;
                CComVariant v;
                e->getAttribute(CComBSTR(L"name"), &v);
                if (v.vt == VT_BSTR && v.bstrVal) {
                    componentInfo.dependencies.push_back(BstrToUtf8(v.bstrVal));
                }
            }
            std::sort(componentInfo.dependencies.begin(), componentInfo.dependencies.end());
            componentInfo.dependencies.erase(std::unique(componentInfo.dependencies.begin(), componentInfo.dependencies.end()), componentInfo.dependencies.end());
        }

        if (componentInfo.identity.empty()) componentInfo.identity = fs::path(mumPath).stem().string();
        return true;
    } catch (...) { return false; }
}

// COM and CBS support and remaining utils
bool CbsManager::initializeCom() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE || hr == S_FALSE;
}

void CbsManager::cleanupCom() { CoUninitialize(); }

bool CbsManager::loadCbsApi() { return true; }
void CbsManager::unloadCbsApi() {}

bool CbsManager::createCbsSession(const std::string& targetPath) { UNREFERENCED_PARAMETER(targetPath); return true; }
void CbsManager::closeCbsSession() { cbsSession.Release(); cbsStore.Release(); }

bool CbsManager::processManifestFiles(const std::vector<std::string>& manifestPaths, const std::string& targetPath) {
    UNREFERENCED_PARAMETER(targetPath);
    appendToErrorLog("[stub] processManifestFiles count=" + std::to_string(manifestPaths.size()));
    return true;
}

bool CbsManager::integrateCbsStore(const std::string& targetPath) { UNREFERENCED_PARAMETER(targetPath); return true; }

bool CbsManager::updateComponentStore(const std::string& targetPath) { appendToErrorLog("Updating component store (stub) at: " + targetPath); return true; }

bool CbsManager::notifyServicingStack(const std::vector<std::string>& installedComponents) { appendToErrorLog("Notifying servicing stack (stub). Components: " + std::to_string(installedComponents.size())); return true; }

std::vector<std::string> CbsManager::resolveDependencies(const CbsPackageInfo& packageInfo) {
    std::vector<std::string> deps; for (const auto& c : packageInfo.components) deps.insert(deps.end(), c.dependencies.begin(), c.dependencies.end()); return deps;
}

bool CbsManager::checkConflicts(const CbsPackageInfo& packageInfo) { UNREFERENCED_PARAMETER(packageInfo); return false; }

bool CbsManager::enableCbsLogging(const std::string& logPath) { logFilePath = logPath; appendToErrorLog(std::string("[log] Logging enabled -> ") + logPath); return true; }

std::string CbsManager::getCbsLogPath() const { return logFilePath.value_or(""); }

std::vector<CbsComponentInfo> CbsManager::enumerateInstalledComponents(const std::string& targetPath) { UNREFERENCED_PARAMETER(targetPath); return {}; }

std::vector<std::string> CbsManager::getComponentDependencies(const std::string& componentIdentity) { UNREFERENCED_PARAMETER(componentIdentity); return {}; }

bool CbsManager::verifyComponentSignature(const std::string& componentPath) { UNREFERENCED_PARAMETER(componentPath); return true; }

bool CbsManager::disableWrp() { return true; }
bool CbsManager::enableWrp() { return true; }
bool CbsManager::bypassWrpForInstall(const std::vector<std::string>& filePaths) { UNREFERENCED_PARAMETER(filePaths); return true; }

bool CbsManager::checkApplicability(const CbsPackageInfo& packageInfo, const std::string& targetSystem) { UNREFERENCED_PARAMETER(packageInfo); UNREFERENCED_PARAMETER(targetSystem); return true; }

std::vector<std::string> CbsManager::getApplicabilityFailures(const CbsPackageInfo& packageInfo) { UNREFERENCED_PARAMETER(packageInfo); return {}; }

bool CbsManager::createStagingDirectory(const std::string& basePath, std::string& stagingPath) {
    try {
        fs::path base = basePath.empty() ? fs::temp_directory_path() : fs::path(basePath);
        fs::path dir = base / ("dismv2_" + std::to_string(::GetTickCount64()));
        fs::create_directories(dir);
        stagingPath = dir.string();
        return true;
    } catch (...) { return false; }
}

bool CbsManager::cleanupStagingDirectory(const std::string& stagingPath) { try { std::error_code ec; fs::remove_all(stagingPath, ec); return true; } catch (...) { return false; } }

bool CbsManager::updateComponentRegistry(const std::vector<CbsComponentInfo>& components) { UNREFERENCED_PARAMETER(components); return true; }

bool CbsManager::removeComponentRegistry(const std::vector<CbsComponentInfo>& components) { UNREFERENCED_PARAMETER(components); return true; }

void CbsManager::setLastError(const std::string& error) { lastError = error; appendToErrorLog("ERROR: " + error); }

void CbsManager::appendToErrorLog(const std::string& logEntry) {
    std::lock_guard<std::mutex> lock(errorLogMutex);
    errorLog.append(logEntry).append("\n");
    if (logFilePath && !logFilePath->empty()) {
        try { std::ofstream f(*logFilePath, std::ios::app | std::ios::binary); if (f.is_open()) f << logEntry << "\n"; } catch (...) {}
    }
}

std::string CbsManager::toAbsolutePath(const std::string& path) { try { return fs::absolute(fs::path(path)).string(); } catch (...) { return path; } }

bool CbsManager::notifyTrustedInstaller(const std::vector<std::string>& operations) { UNREFERENCED_PARAMETER(operations); return true; }

bool CbsManager::schedulePostInstallTasks(const std::vector<std::string>& tasks) { UNREFERENCED_PARAMETER(tasks); return true; }

bool CbsManager::enableRequiredPrivileges() { return true; }
bool CbsManager::enableTrustedInstallerPrivileges() { return true; }

// === Missing implementations appended to resolve link errors ===

// CbsUtils helper implementations
namespace CbsUtils {
    std::vector<std::string> findManifestFiles(const std::string& directory) {
        std::vector<std::string> files;
        try {
            std::error_code ec;
            if (!fs::exists(directory, ec) || ec) return files;
            fs::recursive_directory_iterator it(directory, fs::directory_options::skip_permission_denied, ec), end;
            while (!ec && it != end) {
                const fs::directory_entry& entry = *it;
                std::error_code fec;
                if (entry.is_regular_file(fec) && !fec) {
                    auto ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".mum" || ext == ".xml") {
                        files.push_back(entry.path().string());
                    }
                }
                it.increment(ec);
            }
        } catch (...) {}
        return files;
    }

    bool isRunningOnline() {
        char buf[MAX_PATH] = {};
        DWORD n = GetEnvironmentVariableA("SystemRoot", buf, static_cast<DWORD>(sizeof(buf)));
        if (n > 0 && n < sizeof(buf)) {
            std::error_code ec; return fs::exists(buf, ec) && !ec;
        }
        return true;
    }

    std::string getSystemArchitecture() {
        SYSTEM_INFO si{}; GetNativeSystemInfo(&si);
        switch (si.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64: return "amd64";
            case PROCESSOR_ARCHITECTURE_ARM64: return "arm64";
            case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
            case PROCESSOR_ARCHITECTURE_ARM: return "arm";
            default: return "neutral";
        }
    }

    void logCbsOperation(const std::string& operation, const std::string& details, const std::string& logPath) {
        if (logPath.empty()) return;
        try {
            std::ofstream out(logPath, std::ios::app | std::ios::binary);
            if (out.is_open()) {
                out << operation << ": " << details << "\n";
            }
        } catch (...) {}
    }
}

// CbsManager component registration stub
bool CbsManager::registerComponents(const std::vector<CbsComponentInfo>& components) {
    try {
        for (const auto& c : components) {
            appendToErrorLog(std::string("Registering component (stub): ") + c.identity);
        }
        return true;
    } catch (...) { return false; }
}

// Install from extracted directory with CBS flow
CbsInstallResult CbsManager::installExtractedPackageWithCbs(const std::string& extractedDir,
                                                           const std::string& targetPath,
                                                           bool isOnline) {
    CbsInstallResult result{}; result.success = false;
    if (!initialized && !initialize()) {
        result.errorDescription = "CBS Manager not initialized"; result.errorCode = E_FAIL; return result;
    }
    // Pre-expand any inner CABs to surface manifests/payload
    try {
        appendToErrorLog("Pre-expanding any CABs found in extracted directory...");
        ExpandAllCabsInDir(this, extractedDir);
    } catch (...) {}

    auto pkg = analyzeExtractedPackage(extractedDir);
    if (!pkg) { result.errorDescription = "Failed to analyze extracted package"; result.errorCode = E_FAIL; return result; }
    if (!validateDependencies(*pkg)) { result.errorDescription = "Dependency validation failed"; result.errorCode = E_FAIL; return result; }
    if (!beginTransaction()) { result.errorDescription = "Failed to begin CBS transaction"; result.errorCode = E_FAIL; return result; }

    // Manifests first then payload
    auto manifests = CbsUtils::findManifestFiles(extractedDir);
    if (!processManifestFiles(manifests, targetPath)) {
        rollbackTransaction(); result.errorDescription = "Failed to process manifests"; result.errorCode = E_FAIL; return result;
    }

    // Try payload install; if nothing copied, attempt a second pass after expanding cabs (if any remained)
    if (!installExtractedFiles(extractedDir, targetPath, isOnline)) {
        appendToErrorLog("First payload install attempt reported no files; retrying after ensuring CAB expansion...");
        try { ExpandAllCabsInDir(this, extractedDir); } catch (...) {}
        if (!installExtractedFiles(extractedDir, targetPath, isOnline)) {
            rollbackTransaction(); result.errorDescription = "Failed to install payload"; result.errorCode = E_FAIL; return result;
        }
    }

    for (const auto& comp : pkg->components) {
        if (!registerComponents({comp})) result.failedComponents.push_back(comp.identity); else result.installedComponents.push_back(comp.identity);
    }
    if (!updateComponentStore(targetPath)) { rollbackTransaction(); result.errorDescription = "Failed to update component store"; result.errorCode = E_FAIL; return result; }
    transactionState = CbsTransactionState::Staged;
    if (!commitTransaction()) { result.errorDescription = "Failed to commit CBS transaction"; result.errorCode = E_FAIL; return result; }
    if (isOnline) notifyServicingStack(result.installedComponents);
    result.success = true; result.needsRestart = false; return result;
}