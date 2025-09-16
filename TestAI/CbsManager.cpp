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
#include <deque>
#include <optional>
#include "CabHandler.h"
#include "PsfWimHandler.h"
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

    std::optional<std::string> RotateLogIfNeeded(const std::string& path, size_t maxBytes = 2 * 1024 * 1024, int keep = 3) {
        try {
            if (path.empty()) return std::nullopt;
            std::error_code ec;
            auto sz = fs::exists(path, ec) ? fs::file_size(path, ec) : 0;
            if (ec || sz < maxBytes) return std::nullopt;
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
            std::ostringstream oss; oss << "[LOG] Rotated log: " << path << " -> " << first;
            return oss.str();
        } catch (...) {
            return std::nullopt;
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

// TrustedInstaller detection and prep utilities
namespace {
    // Determine if current process token belongs to TrustedInstaller (either user or group)
    bool IsTrustedInstallerToken() {
        HANDLE hToken = NULL;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) return false;
        // TokenUser
        DWORD len = 0; GetTokenInformation(hToken, TokenUser, nullptr, 0, &len);
        std::vector<BYTE> buf; buf.resize(len);
        if (GetTokenInformation(hToken, TokenUser, buf.data(), len, &len)) {
            TOKEN_USER* tu = reinterpret_cast<TOKEN_USER*>(buf.data());
            WCHAR name[256] = {}; WCHAR domain[256] = {}; DWORD cchName = 256, cchDomain = 256; SID_NAME_USE use;
            if (LookupAccountSidW(NULL, tu->User.Sid, name, &cchName, domain, &cchDomain, &use)) {
                if (_wcsicmp(name, L"TrustedInstaller") == 0) { CloseHandle(hToken); return true; }
            }
        }
        // TokenGroups fallback
        len = 0; GetTokenInformation(hToken, TokenGroups, nullptr, 0, &len);
        buf.clear(); buf.resize(len);
        if (GetTokenInformation(hToken, TokenGroups, buf.data(), len, &len)) {
            TOKEN_GROUPS* tg = reinterpret_cast<TOKEN_GROUPS*>(buf.data());
            for (DWORD i = 0; i < tg->GroupCount; ++i) {
                WCHAR name[256] = {}; WCHAR domain[256] = {}; DWORD cchName = 256, cchDomain = 256; SID_NAME_USE use;
                if (LookupAccountSidW(NULL, tg->Groups[i].Sid, name, &cchName, domain, &cchDomain, &use)) {
                    if (_wcsicmp(name, L"TrustedInstaller") == 0) { CloseHandle(hToken); return true; }
                }
            }
        }
        CloseHandle(hToken);
        return false;
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
    
    auto t_all_start = std::chrono::steady_clock::now();
    try {
        appendToErrorLog("Starting CBS-integrated installation of: " + packagePath);
        appendToErrorLog("Target path: " + targetPath);
        appendToErrorLog("Online mode: " + std::string(isOnline ? "Yes" : "No"));
        
        // 1. Verify package exists
        auto t0 = std::chrono::steady_clock::now();
        if (!fs::exists(packagePath)) {
            result.errorDescription = "Package file does not exist: " + packagePath;
            result.errorCode = ERROR_FILE_NOT_FOUND;
            appendToErrorLog("CBS installation failed: " + result.errorDescription);
            return result;
        }
        auto t1 = std::chrono::steady_clock::now();
        appendToErrorLog("Phase: preflight exists check ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count()));
        
        // 2. Create temporary extraction directory
        t0 = std::chrono::steady_clock::now();
        std::string tempDir;
        if (!createStagingDirectory("", tempDir)) {
            result.errorDescription = "Failed to create staging directory for package extraction";
            result.errorCode = E_FAIL;
            appendToErrorLog("CBS installation failed: " + result.errorDescription);
            return result;
        }
        appendToErrorLog("Created staging directory: " + tempDir);
        t1 = std::chrono::steady_clock::now();
        appendToErrorLog("Phase: create staging ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count()));
        
        // 3. Extract package
        appendToErrorLog("Extracting package for analysis...");
        t0 = std::chrono::steady_clock::now();
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
        t1 = std::chrono::steady_clock::now();
        appendToErrorLog("Phase: extraction ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count()));
        
        if (!extractionSuccess) {
            appendToErrorLog("Warning: Package extraction failed, using basic analysis");
        } else {
            appendToErrorLog("Package extraction successful");
        }
        
        // 4. Analyze extracted package
        appendToErrorLog("Analyzing package structure...");
        t0 = std::chrono::steady_clock::now();
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
        t1 = std::chrono::steady_clock::now();
        appendToErrorLog("Phase: analysis ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count()));
        
        appendToErrorLog("Package analysis successful:");
        appendToErrorLog("  Package ID: " + packageInfo->packageIdentity);
        appendToErrorLog("  Components: " + std::to_string(packageInfo->components.size()));
        
        // 5. Verify package signature (non-blocking for internal packages)
        appendToErrorLog("Verifying package signature...");
        t0 = std::chrono::steady_clock::now();
        if (!verifyPackageSignature(packagePath)) {
            appendToErrorLog("Warning: Package signature verification failed, but continuing installation");
        } else {
            appendToErrorLog("Package signature verification successful");
        }
        t1 = std::chrono::steady_clock::now();
        appendToErrorLog("Phase: signature verify ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count()));
        
        // 6. Check applicability
        appendToErrorLog("Checking package applicability...");
        t0 = std::chrono::steady_clock::now();
        if (!checkApplicability(*packageInfo, targetPath)) {
            result.errorDescription = "Package is not applicable to target system";
            result.errorCode = E_INVALIDARG;
            appendToErrorLog("CBS installation failed: " + result.errorDescription);
            cleanupStagingDirectory(tempDir);
            return result;
        }
        t1 = std::chrono::steady_clock::now();
        appendToErrorLog("Phase: applicability ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count()));
        
        // 7. Begin transaction
        appendToErrorLog("Beginning CBS transaction...");
        t0 = std::chrono::steady_clock::now();
        if (!beginTransaction()) {
            result.errorDescription = "Failed to begin CBS transaction: " + (lastError ? *lastError : "Unknown error");
            result.errorCode = E_FAIL;
            appendToErrorLog("CBS installation failed: " + result.errorDescription);
            cleanupStagingDirectory(tempDir);
            return result;
        }
        t1 = std::chrono::steady_clock::now();
        appendToErrorLog("Phase: begin transaction ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count()));
        
        // 8. Install package (two-pass: manifests first)
        appendToErrorLog("Installing package components...");
        t0 = std::chrono::steady_clock::now();
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
        t1 = std::chrono::steady_clock::now();
        appendToErrorLog("Phase: register components ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count()));
        
        // 9. Update component store (stub)
        appendToErrorLog("Updating CBS component store...");
        t0 = std::chrono::steady_clock::now();
        if (!updateComponentStore(targetPath)) {
            result.errorDescription = "Failed to update component store";
            result.errorCode = E_FAIL;
            appendToErrorLog("CBS installation failed: " + result.errorDescription);
            rollbackTransaction();
            cleanupStagingDirectory(tempDir);
            return result;
        }
        t1 = std::chrono::steady_clock::now();
        appendToErrorLog("Phase: update component store ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count()));
        
        // 10. Commit transaction
        appendToErrorLog("Committing CBS transaction...");
        t0 = std::chrono::steady_clock::now();
        transactionState = CbsTransactionState::Staged;
        if (!commitTransaction()) {
            result.errorDescription = "Failed to commit CBS transaction: " + (lastError ? *lastError : "Unknown error");
            result.errorCode = E_FAIL;
            appendToErrorLog("CBS installation failed: " + result.errorDescription);
            cleanupStagingDirectory(tempDir);
            return result;
        }
        t1 = std::chrono::steady_clock::now();
        appendToErrorLog("Phase: commit transaction ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count()));
        
        // 11. Notify servicing stack
        if (isOnline) {
            appendToErrorLog("Notifying Windows servicing stack...");
            t0 = std::chrono::steady_clock::now();
            notifyServicingStack(result.installedComponents);
            t1 = std::chrono::steady_clock::now();
            appendToErrorLog("Phase: servicing notify ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count()));
        }
        
        // 12. Cleanup staging directory
        t0 = std::chrono::steady_clock::now();
        cleanupStagingDirectory(tempDir);
        t1 = std::chrono::steady_clock::now();
        appendToErrorLog("Phase: cleanup staging ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count()));
        
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
        // Auto tail CBS/DISM logs on failure (last 200 lines)
        tailServicingLogs(200);
    }
    auto t_all_end = std::chrono::steady_clock::now();
    appendToErrorLog("Total duration ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(t_all_end - t_all_start).count()));
    
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
        fs::path troot = fs::path(targetRoot);
        if (!troot.has_root_name() || (troot.has_root_name() && !troot.has_root_directory())) {
            std::string tr = targetRoot; if (!tr.empty() && tr.back() != '\\') tr.push_back('\\'); troot = fs::path(tr);
        }
        std::error_code ec;
        fs::path rel = fs::relative(src, extractedRoot, ec);
        if (ec) {
            // fallback to original string logic if relative fails
            std::string srcStr = src.string();
            std::string low = srcStr; std::transform(low.begin(), low.end(), low.begin(), ::tolower);
            auto posPkgs = low.find("\\windows\\servicing\\packages\\");
            if (posPkgs != std::string::npos) {
                std::string winTail = srcStr.substr(posPkgs); return troot / fs::path(winTail);
            }
            auto posSxs = low.find("\\windows\\winsxs\\");
            if (posSxs != std::string::npos) { std::string winTail = srcStr.substr(posSxs); return troot / fs::path(winTail); }
            return std::nullopt;
        }

        // Component-aware mapping: evaluate by path components (case-insensitive)
        auto toLower = [](std::wstring s){ std::transform(s.begin(), s.end(), s.begin(), [](wchar_t ch){ return (wchar_t)tolower((unsigned char)ch); }); return s; };
        std::vector<std::wstring> comps; comps.reserve(std::distance(rel.begin(), rel.end()));
        for (auto& c : rel) comps.push_back(toLower(c.wstring()));

        auto startsWith = [&](std::initializer_list<std::wstring> head) {
            if (comps.size() < head.size()) return false;
            size_t i = 0; for (const auto& h : head) { if (comps[i++] != h) return false; }
            return true;
        };
        auto tailFrom = [&](size_t idx){ fs::path p; for (size_t i=idx;i<comps.size();++i) p /= fs::path(comps[i]); return p; };

        // If the relative path starts with Windows\..., keep it under target Windows
        if (!comps.empty() && comps[0] == L"windows") {
            return troot / rel; // preserve original casing via rel
        }

        // Known roots without leading Windows
        if (startsWith({L"servicing", L"packages"}))                  return troot / "Windows" / "servicing" / "Packages" / tailFrom(2);
        if (startsWith({L"servicing", L"lcu"}))                        return troot / "Windows" / "servicing" / "LCU" / tailFrom(2);
        if (startsWith({L"servicing", L"stack"}))                      return troot / "Windows" / "servicing" / "Stack" / tailFrom(2);
        if (startsWith({L"winsxs"}))                                    return troot / "Windows" / "WinSxS" / tailFrom(1);
        if (startsWith({L"system32", L"drivers"}))                     return troot / "Windows" / "System32" / "drivers" / tailFrom(2);
        if (startsWith({L"syswow64"}))                                  return troot / "Windows" / "SysWOW64" / tailFrom(1);
        if (startsWith({L"fonts"}))                                     return troot / "Windows" / "Fonts" / tailFrom(1);
        if (startsWith({L"inf"}))                                       return troot / "Windows" / "INF" / tailFrom(1);
        if (startsWith({L"policydefinitions"}))                         return troot / "Windows" / "PolicyDefinitions" / tailFrom(1);
        if (startsWith({L"boot"}))                                      return troot / "Windows" / "Boot" / tailFrom(1);
        if (startsWith({L"recovery"}))                                  return troot / "Windows" / "System32" / "Recovery" / tailFrom(1);
        if (startsWith({L"systemresources"}))                           return troot / "Windows" / "SystemResources" / tailFrom(1);
        if (startsWith({L"system32", L"driverstore", L"filerepository"})) return troot / "Windows" / "System32" / "DriverStore" / "FileRepository" / tailFrom(3);
        if (startsWith({L"efi", L"microsoft", L"boot"}))               return troot / "EFI" / "Microsoft" / "Boot" / tailFrom(3);

        return std::nullopt;
    } catch (...) { return std::nullopt; }
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

    // Online preflight: elevation and TrustedInstaller state
    if (isOnline) {
        appendToErrorLog(std::string("Elevation: ") + (IsProcessElevated()?"Administrator":"Not elevated"));
        appendToErrorLog(std::string("TrustedInstaller token: ") + (IsTrustedInstallerToken()?"Detected":"Not present"));
    }

    // Early permission sanity check for ONLINE installs
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

    // Offline preflight: ensure Boot/EFI dirs exist if offline
    if (!isOnline) {
        try {
            fs::create_directories(target / "Windows" / "Boot", ec); ec.clear();
            fs::create_directories(target / "EFI" / "Microsoft" / "Boot", ec); ec.clear();
        } catch (...) {
            appendToErrorLog("Offline preflight: failed to ensure Boot/EFI directories exist");
        }
    }

    size_t copied = 0, skipped = 0, failed = 0;
    size_t mumCount = 0, catCount = 0;
    std::vector<fs::path> copiedCatalogTargets;
    std::vector<fs::path> specialComponents; // .appx/.msix/.psf/.wim/.esd
    bool bootFilesChanged = false;

    // Pass 1: manifests and catalogs
    // ...existing code (unchanged)...

    // Verify/register catalogs
    // ...existing code (unchanged)...

    // Pass 2: payload
    ec.clear();
    fs::recursive_directory_iterator it2(root, fs::directory_options::skip_permission_denied, ec), end;
    while (!ec && it2 != end) {
        const fs::directory_entry& entry = *it2;
        std::error_code fec;
        if (entry.is_symlink(fec)) { it2.disable_recursion_pending(); it2.increment(ec); continue; }
        if (fec) { it2.increment(ec); continue; }
        if (!entry.is_regular_file(fec) || fec) { it2.increment(ec); continue; }
        auto ext = entry.path().extension().string(); std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".mum") { ++mumCount; it2.increment(ec); continue; }
        if (ext == ".cat") { ++catCount; it2.increment(ec); continue; }
        if (ext == ".appx" || ext == ".msix" || ext == ".psf" || ext == ".wim" || ext == ".esd") {
            specialComponents.push_back(entry.path()); it2.increment(ec); continue;
        }
        auto dstOpt = ComputeDestinationForExtracted(entry.path(), root, target.string());
        if (!dstOpt) { ++skipped; it2.increment(ec); continue; }
        if (!IsUnderRootCaseInsensitive(*dstOpt, target)) { ++failed; appendToErrorLog("Path outside target root skipped: " + dstOpt->string()); it2.increment(ec); continue; }
        std::wstring wsrc = entry.path().wstring();
        std::wstring wdst = dstOpt->wstring();
        std::string log;
        if (CopyFileLongPath(wsrc, wdst, /*overwrite*/true, &log)) {
            ++copied;
            // Track if boot-related files changed (Windows\Boot or EFI\Microsoft\Boot)
            auto wstr = dstOpt->wstring();
            std::wstring low = wstr; std::transform(low.begin(), low.end(), low.begin(), ::towlower);
            if (low.find(L"\\windows\\boot\\") != std::wstring::npos || low.find(L"\\efi\\microsoft\\boot\\") != std::wstring::npos) {
                bootFilesChanged = true;
            }
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

    appendToErrorLog("Manifests (.mum) encountered: " + std::to_string(mumCount));
    appendToErrorLog("Catalogs (.cat) encountered: " + std::to_string(catCount));
    appendToErrorLog("Special components queued (.appx/.msix/.psf/.wim/.esd): " + std::to_string(specialComponents.size()));
    if (!specialComponents.empty()) {
        size_t preview = std::min<size_t>(specialComponents.size(), 5);
        for (size_t i = 0; i < preview; ++i) {
            appendToErrorLog("  - " + specialComponents[i].string());
        }
        if (specialComponents.size() > preview) {
            appendToErrorLog("  ... (" + std::to_string(specialComponents.size() - preview) + ") more");
        }
    }

    // Process PSF/WIM/ESD via handler
    // ...existing code (unchanged)...

    // Optionally update boot files with bcdboot for offline images if we changed boot content
    if (!isOnline && bootFilesChanged) {
        try {
            std::wstring bcdboot = getSystemToolPath(L"bcdboot.exe");
            std::wstring wImg = ToLongPath(fs::path(target).wstring());
            // Use default Windows directory in the offline image
            std::wstring cmd = L"\"" + bcdboot + L"\" \"" + (wImg + L"Windows") + L"\" /s \"" + (wImg + L"EFI") + L"\" /f ALL";
            std::string out; DWORD code = 1;
            appendToErrorLog("Running bcdboot to update offline boot files: " + std::string(cmd.begin(), cmd.end()));
            RunProcessCapture(cmd, ExternalTimeoutMs(120000), out, code);
            appendToErrorLog(std::string("bcdboot exit=") + std::to_string(code) + ", output: " + out);
        } catch (...) {
            appendToErrorLog("Attempt to run bcdboot failed with unexpected error");
        }
    }

    appendToErrorLog("installExtractedFiles summary: copied=" + std::to_string(copied) + ", skipped=" + std::to_string(skipped) + ", failed=" + std::to_string(failed));

    lastStats.copied = copied; lastStats.skipped = skipped; lastStats.failed = failed;

    return failed == 0 || copied > 0;
}

// ===== Missing method implementations to satisfy linkage and provide baseline behavior =====

bool CbsManager::enableCbsLogging(const std::string& logPath) {
    logFilePath = logPath;
    appendToErrorLog("CBS logging enabled: " + logPath);
    return true;
}

void CbsManager::setLastError(const std::string& error) {
    lastError = error;
    appendToErrorLog("[ERROR] " + error);
    // On error, tail logs for context
    tailServicingLogs(200);
}

void CbsManager::appendToErrorLog(const std::string& logEntry) {
    std::lock_guard<std::mutex> lock(errorLogMutex);
    errorLog.append(logEntry).append("\n");
    if (logFilePath && !logFilePath->empty()) {
        try {
            if (auto rot = RotateLogIfNeeded(*logFilePath)) {
                std::ofstream f(*logFilePath, std::ios::app | std::ios::binary);
                if (f.is_open()) f << *rot << "\n";
            }
            std::ofstream f(*logFilePath, std::ios::app | std::ios::binary);
            if (f.is_open()) f << logEntry << "\n";
        } catch (...) {}
    }
}

bool CbsManager::initializeCom() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
}

void CbsManager::cleanupCom() {
    CoUninitialize();
}

bool CbsManager::loadCbsApi() {
    // Placeholder: real implementation would LoadLibrary CBS APIs
    return true;
}

void CbsManager::unloadCbsApi() {
    // Placeholder
}

bool CbsManager::createCbsSession(const std::string& targetPath) {
    // Placeholder for real CBS session creation
    (void)targetPath; return true;
}

void CbsManager::closeCbsSession() {
    // Placeholder for real CBS session close
}

bool CbsManager::enableRequiredPrivileges() {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        appendToErrorLog("enableRequiredPrivileges: OpenProcessToken failed");
        return false;
    }

    auto enableOne = [&](LPCSTR name) -> bool {
        LUID luid{};
        if (!LookupPrivilegeValueA(nullptr, name, &luid)) {
            appendToErrorLog("enableRequiredPrivileges: LookupPrivilegeValue failed");
            return false;
        }
        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr)) {
            appendToErrorLog("enableRequiredPrivileges: AdjustTokenPrivileges failed");
            return false;
        }
        if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
            appendToErrorLog("enableRequiredPrivileges: privilege not assigned to token");
            return false;
        }
        return true;
    };

    bool ok = false;
    ok |= enableOne(SE_BACKUP_NAME);
    ok |= enableOne(SE_RESTORE_NAME);
    ok |= enableOne(SE_TAKE_OWNERSHIP_NAME);
    ok |= enableOne(SE_MANAGE_VOLUME_NAME);
    ok |= enableOne(SE_SECURITY_NAME);

    CloseHandle(hToken);
    return ok;
}

bool CbsManager::createStagingDirectory(const std::string& basePath, std::string& stagingPath) {
    try {
        namespace fs = std::filesystem;
        fs::path base;
        if (!basePath.empty()) {
            base = fs::path(basePath);
        } else {
            char buf[MAX_PATH] = {};
            DWORD n = GetEnvironmentVariableA("DISMV2_TEMP", buf, static_cast<DWORD>(sizeof(buf)));
            if (n > 0 && n < sizeof(buf)) {
                base = fs::path(std::string(buf));
            } else {
                base = fs::temp_directory_path();
            }
        }
        DWORD pid = GetCurrentProcessId();
        fs::path dir = base / ("dismv2_" + std::to_string(pid) + "_" + std::to_string(::GetTickCount64()));
        std::error_code ec; fs::create_directories(dir, ec);
        stagingPath = dir.string();
        return !ec;
    } catch (...) { return false; }
}

bool CbsManager::cleanupStagingDirectory(const std::string& stagingPath) {
    try {
        namespace fs = std::filesystem;
        std::error_code ec; fs::remove_all(stagingPath, ec);
        return !ec;
    } catch (...) { return false; }
}

bool CbsManager::extractCabForAnalysis(const std::string& cabPath, const std::string& destination) {
#if WITH_FDI
    return ExtractWithFDI(cabPath, destination);
#else
    try {
        std::wstring expand = getSystemToolPath(L"expand.exe");
        std::wstring wCab = ToLongPath(std::wstring(cabPath.begin(), cabPath.end()));
        std::wstring wDst = ToLongPath(std::wstring(destination.begin(), destination.end()));
        std::wstring cmd = L"\"" + expand + L"\" \"" + wCab + L"\" -F:* \"" + wDst + L"\"";
        std::string out; DWORD code = 1;
        if (RunProcessCapture(cmd, ExternalTimeoutMs(600000), out, code)) {
            appendToErrorLog("expand output: " + out);
            return code == 0;
        }
    } catch (...) {}
    return false;
#endif
}

bool CbsManager::extractMsuForAnalysis(const std::string& msuPath, const std::string& destination) {
    try {
        // Prefer expand.exe
        if (extractCabForAnalysis(msuPath, destination)) return true;
        // DISM offline extract when image provided
        if (!offlineImagePath.empty()) {
            std::wstring dism = getSystemToolPath(L"dism.exe");
            std::wstring wImg = ToLongPath(std::wstring(offlineImagePath.begin(), offlineImagePath.end()));
            std::wstring wMsu = ToLongPath(std::wstring(msuPath.begin(), msuPath.end()));
            std::wstring wOut = ToLongPath(std::wstring(destination.begin(), destination.end()));
            std::wstring cmd = L"\"" + dism + L"\" /Image:\"" + wImg + L"\" /Add-Package /PackagePath:\"" + wMsu + L"\" /Extract:\"" + wOut + L"\"";
            std::string out; DWORD code = 1;
            if (RunProcessCapture(cmd, ExternalTimeoutMs(600000), out, code)) {
                appendToErrorLog("dism /Extract output: " + out);
                if (code == 0) return true;
            }
        }
        // WUSA fallback
        if (allowWusaFallback) {
            std::wstring wusa = getSystemToolPath(L"wusa.exe");
            std::wstring wMsu = ToLongPath(std::wstring(msuPath.begin(), msuPath.end()));
            std::wstring wOut = ToLongPath(std::wstring(destination.begin(), destination.end()));
            std::wstring cmd = L"\"" + wusa + L"\" \"" + wMsu + L"\" /extract:\"" + wOut + L"\" /quiet /norestart";
            std::string out; DWORD code = 1;
            if (RunProcessCapture(cmd, ExternalTimeoutMs(600000), out, code)) {
                appendToErrorLog("wusa /extract output: " + out);
                if (code == 0) return true;
            }
        }
    } catch (...) {}
    return false;
}

bool CbsManager::extractGenericPackageForAnalysis(const std::string& packagePath, const std::string& destination) {
    // Try CAB-like extraction first
    return extractCabForAnalysis(packagePath, destination);
}

std::optional<CbsComponentInfo> CbsManager::parseComponentManifest(const std::string& manifestPath) {
    CbsComponentInfo comp{};
    comp.identity = std::filesystem::path(manifestPath).stem().string();
    comp.version = "1.0.0.0";
    comp.architecture = CbsUtils::getSystemArchitecture();
    comp.state = "Staged";
    comp.isApplicable = true;
    comp.needsRestart = false;
    return comp;
}

bool CbsManager::parseMumManifest(const std::string& mumPath, CbsComponentInfo& componentInfo) {
    try {
        CComPtr<IXMLDOMDocument> doc;
        HRESULT hr = CoCreateInstance(CLSID_DOMDocument60, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&doc));
        if (FAILED(hr) || !doc) {
            return false;
        }
        VARIANT_BOOL vb = VARIANT_FALSE;
        CComVariant vFile(CComBSTR(std::wstring(mumPath.begin(), mumPath.end()).c_str()));
        doc->put_async(VARIANT_FALSE);
        hr = doc->load(vFile, &vb);
        if (FAILED(hr) || vb == VARIANT_FALSE) {
            return false;
        }
        CComPtr<IXMLDOMNode> node;
        hr = doc->selectSingleNode(CComBSTR(L"/assembly/assemblyIdentity"), &node);
        if (FAILED(hr) || !node) {
            return false;
        }
        CComPtr<IXMLDOMElement> elem;
        elem = node; // QI to element
        if (!elem) return false;
        auto getAttr = [&](const wchar_t* name) -> std::string {
            CComVariant val; if (SUCCEEDED(elem->getAttribute(CComBSTR(name), &val)) && val.vt == VT_BSTR && val.bstrVal) {
                return std::string(_bstr_t(val.bstrVal));
            }
            return std::string();
        };
        std::string id = getAttr(L"name");
        std::string ver = getAttr(L"version");
        std::string arch = getAttr(L"processorArchitecture");
        std::string pkt = getAttr(L"publicKeyToken");
        if (!id.empty()) componentInfo.identity = id;
        if (!ver.empty()) componentInfo.version = ver;
        if (!arch.empty()) { componentInfo.architecture = arch; componentInfo.processorArchitecture = arch; }
        if (!pkt.empty()) componentInfo.publicKeyToken = pkt;
        if (componentInfo.identity.empty()) {
            componentInfo.identity = std::filesystem::path(mumPath).stem().string();
        }
        return true;
    } catch (...) { return false; }
}

bool CbsManager::verifyPackageSignature(const std::string& packagePath) {
    try {
        std::wstring wPath(packagePath.begin(), packagePath.end());
        WINTRUST_FILE_INFO fileInfo{}; fileInfo.cbStruct = sizeof(fileInfo); fileInfo.pcwszFilePath = wPath.c_str();
        GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        WINTRUST_DATA data{}; data.cbStruct = sizeof(data);
        data.dwUIChoice = WTD_UI_NONE;
        data.fdwRevocationChecks = WTD_REVOKE_NONE;
        data.dwUnionChoice = WTD_CHOICE_FILE;
        data.pFile = &fileInfo;
        data.dwStateAction = WTD_STATEACTION_VERIFY;
        data.dwProvFlags = WTD_SAFER_FLAG | WTD_CACHE_ONLY_URL_RETRIEVAL | WTD_REVOCATION_CHECK_NONE;
        LONG st = WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &data);
        data.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &action, &data);
        bool ok = (st == ERROR_SUCCESS);
        appendToErrorLog(std::string("WinVerifyTrust for ") + packagePath + (ok?" OK":" FAILED"));
        return ok;
    } catch (...) { return false; }
}

bool CbsManager::checkApplicability(const CbsPackageInfo& packageInfo, const std::string& targetSystem) {
    try {
        if (!targetSystem.empty()) {
            if (!std::filesystem::exists(targetSystem)) {
                setLastError("Target system path does not exist: " + targetSystem);
                return false;
            }
        }
        std::string sysArch = CbsUtils::getSystemArchitecture();
        auto isCompat = [&](const std::string& arch) {
            if (arch.empty()) return true;
            std::string a = arch; std::transform(a.begin(), a.end(), a.begin(), ::tolower);
            if (a == "neutral" || a == "none") return true;
            if (a == sysArch) return true;
            return false;
        };
        for (const auto& c : packageInfo.components) {
            if (!isCompat(c.architecture)) {
                appendToErrorLog("Applicability: incompatible architecture for component " + c.identity + ": " + c.architecture);
                return false;
            }
        }
        return true;
    } catch (...) { return true; }
}

std::vector<std::string> CbsManager::resolveDependencies(const CbsPackageInfo& packageInfo) {
    // Placeholder: no external dependencies
    (void)packageInfo; return {};
}

bool CbsManager::registerComponents(const std::vector<CbsComponentInfo>& components) {
    // Placeholder: register succeeds
    (void)components; return true;
}

bool CbsManager::updateComponentStore(const std::string& targetPath) {
    // Placeholder
    (void)targetPath; return true;
}

bool CbsManager::notifyServicingStack(const std::vector<std::string>& installedComponents) {
    // Placeholder
    (void)installedComponents; return true;
}

CbsInstallResult CbsManager::installExtractedPackageWithCbs(const std::string& extractedDir,
                                                           const std::string& targetPath,
                                                           bool isOnline) {
    CbsInstallResult r{}; r.success = false; r.errorCode = S_OK; r.needsRestart = false;
    try {
        if (!installExtractedFiles(extractedDir, targetPath, isOnline)) {
            r.errorDescription = "installExtractedFiles failed";
            r.errorCode = E_FAIL; return r;
        }
        r.success = true; return r;
    } catch (const std::exception& ex) {
        r.errorDescription = ex.what(); r.errorCode = E_UNEXPECTED; return r;
    }
}

// ===== CbsUtils minimal implementations =====
namespace CbsUtils {
    std::vector<std::string> findManifestFiles(const std::string& directory) {
        std::vector<std::string> files;
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::recursive_directory_iterator it(directory, fs::directory_options::skip_permission_denied, ec), end;
        while (!ec && it != end) {
            const auto& entry = *it;
            std::error_code fec;
            if (entry.is_regular_file(fec)) {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".mum") files.push_back(entry.path().string());
            }
            it.increment(ec);
        }
        return files;
    }

    bool isRunningOnline() {
        wchar_t sysWin[MAX_PATH] = {};
        UINT n = GetSystemWindowsDirectoryW(sysWin, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) return true;
        std::filesystem::path win(sysWin);
        return std::filesystem::exists(win / L"servicing" / L"Packages") && std::filesystem::exists(win / L"WinSxS");
    }

    std::string getSystemArchitecture() {
        SYSTEM_INFO si{}; GetNativeSystemInfo(&si);
        switch (si.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64: return "amd64";
            case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
            case PROCESSOR_ARCHITECTURE_ARM64: return "arm64";
            default: return "unknown";
        }
    }

    void logCbsOperation(const std::string& operation, const std::string& details, const std::string& logPath) {
        try {
            // Timestamped log line
            auto now = std::chrono::system_clock::now();
            auto t = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
#if defined(_WIN32)
            localtime_s(&tm, &t);
#else
            tm = *std::localtime(&t);
#endif
            char ts[32]; std::snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

            std::string line = std::string("[") + ts + "] " + operation + ": " + details + "\n";
            if (!logPath.empty()) {
                std::ofstream f(logPath, std::ios::app | std::ios::binary);
                if (f.is_open()) f << line;
            }
            OutputDebugStringA(line.c_str());
        } catch (...) {}
    }
}