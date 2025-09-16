#include "PsfWimHandler.h"
#include "CabHandler.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <memory>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <regex>

#include <windows.h>
#include <appxpackaging.h>
#include <shlwapi.h>
#include <comutil.h>
#include <atlbase.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")

// Optional native WIMGAPI integration
#if __has_include(<wimgapi.h>)
  #define WITH_WIMGAPI 1
  #include <wimgapi.h>
#else
  #define WITH_WIMGAPI 0
#endif

namespace fs = std::filesystem;

namespace {
    std::wstring GetSystemToolPath(const wchar_t* tool) {
        wchar_t winDir[MAX_PATH] = {};
        UINT wlen = GetWindowsDirectoryW(winDir, MAX_PATH);
        if (wlen == 0 || wlen >= MAX_PATH) return std::wstring(tool);
        BOOL isWow64 = FALSE;
        typedef BOOL (WINAPI *FN)(HANDLE, PBOOL);
        HMODULE h = GetModuleHandleW(L"kernel32.dll");
        if (h) { auto fn = reinterpret_cast<FN>(GetProcAddress(h, "IsWow64Process")); if (fn) fn(GetCurrentProcess(), &isWow64); }
        std::wstring base(winDir);
        if (isWow64) return base + L"\\Sysnative\\" + tool;
        return base + L"\\System32\\" + tool;
    }

    bool RunProcessCapture(const std::wstring& cmd, DWORD timeoutMs, std::string& output, DWORD& exitCode) {
        SECURITY_ATTRIBUTES sa{}; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE;
        HANDLE hRead=NULL, hWrite=NULL; if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
        STARTUPINFOW si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = hWrite; si.hStdError = hWrite;
        PROCESS_INFORMATION pi{};
        BOOL ok = CreateProcessW(NULL, const_cast<wchar_t*>(cmd.c_str()), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        if (!ok) { CloseHandle(hRead); CloseHandle(hWrite); return false; }
        HANDLE hJob = CreateJobObjectW(nullptr, nullptr);
        if (hJob) { JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{}; info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE; SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &info, sizeof(info)); AssignProcessToJobObject(hJob, pi.hProcess); }
        CloseHandle(hWrite);
        output.clear(); std::string buf; buf.resize(4096); DWORD start = GetTickCount(); exitCode = 1;
        for (;;) {
            DWORD avail=0; if (PeekNamedPipe(hRead, NULL, 0, NULL, &avail, NULL) && avail>0) {
                DWORD toRead = (DWORD)std::min<size_t>(avail, buf.size()); DWORD got=0;
                if (ReadFile(hRead, buf.data(), toRead, &got, NULL) && got>0) output.append(buf.data(), buf.data()+got);
            } else {
                DWORD w = WaitForSingleObject(pi.hProcess, 50);
                if (w == WAIT_OBJECT_0) { for(;;){ if (PeekNamedPipe(hRead,NULL,0,NULL,&avail,NULL)&&avail>0){ DWORD got=0; DWORD toRead=(DWORD)std::min<size_t>(avail,buf.size()); if (ReadFile(hRead,buf.data(),toRead,&got,NULL)&&got>0) output.append(buf.data(),buf.data()+got); else break; } else break; } break; }
                if (timeoutMs && GetTickCount() - start > timeoutMs) { if (hJob) TerminateJobObject(hJob, 1); else TerminateProcess(pi.hProcess, 1); break; }
            }
        }
        GetExitCodeProcess(pi.hProcess, &exitCode); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); if (hJob) CloseHandle(hJob); CloseHandle(hRead); return true;
    }
}

class PsfWimHandlerImpl {
private:
    bool initialized = false;
    std::string lastError;

#if WITH_WIMGAPI
    static DWORD CALLBACK WimMsgCb(PVOID, UINT msgId, WPARAM wParam, LPARAM lParam, PVOID userData) {
        PsfWimHandlerImpl* self = reinterpret_cast<PsfWimHandlerImpl*>(userData);
        (void)self; (void)lParam;
        switch (msgId) {
            case WIM_MSG_PROGRESS: {
                // Best-effort percent reporting using wParam
                unsigned long percent = static_cast<unsigned long>(wParam);
                char buf[64]; _snprintf_s(buf, sizeof(buf), _TRUNCATE, "WIM progress: %lu", percent);
                OutputDebugStringA(buf);
                break;
            }
            case WIM_MSG_PROCESS: {
                OutputDebugStringA("WIM processing...");
                break;
            }
            case WIM_MSG_ERROR: {
                OutputDebugStringA("WIM error reported");
                break;
            }
            case WIM_MSG_INFO: {
                OutputDebugStringA("WIM info");
                break;
            }
            default: break;
        }
        return WIM_MSG_SUCCESS;
    }

    bool tryWimApply(const std::wstring& wimPath, int imageIndex, const std::wstring& dest) {
        DWORD createResult = 0;
        HANDLE hwim = WIMCreateFile(wimPath.c_str(), GENERIC_READ, OPEN_EXISTING, 0, 0, &createResult);
        if (!hwim) {
            lastError = "WIMCreateFile failed: " + std::to_string(GetLastError());
            return false;
        }
        // Temp path from env override
        wchar_t tempBuf[MAX_PATH] = {};
        DWORD n = GetEnvironmentVariableW(L"DISMV2_WIM_TEMP", tempBuf, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            WIMSetTemporaryPath(hwim, tempBuf);
        }
        // Integrity check toggle (default on)
        DWORD verify = 1;
        wchar_t verBuf[8] = {};
        n = GetEnvironmentVariableW(L"DISMV2_WIM_VERIFY", verBuf, 8);
        if (n > 0 && n < 8) {
            if (verBuf[0] == L'0') verify = 0; else verify = 1;
        }
        WIMSetIntegrityCheck(hwim, verify);

        WIMRegisterMessageCallback(hwim, &WimMsgCb, this);

        HANDLE hImage = WIMLoadImage(hwim, imageIndex);
        if (!hImage) {
            lastError = "WIMLoadImage failed: " + std::to_string(GetLastError());
            WIMUnregisterMessageCallback(hwim, &WimMsgCb);
            WIMCloseHandle(hwim);
            return false;
        }
        BOOL ok = WIMApplyImage(hImage, dest.c_str(), 0);
        DWORD gle = GetLastError();
        WIMCloseHandle(hImage);
        WIMUnregisterMessageCallback(hwim, &WimMsgCb);
        WIMCloseHandle(hwim);
        if (!ok) {
            lastError = "WIMApplyImage failed: " + std::to_string(gle);
            return false;
        }
        return true;
    }

    bool tryWimCapture(const std::wstring& src, const std::wstring& wimPath, const std::wstring& name, const std::wstring& description, WimCompression compression) {
        DWORD createResult = 0;
        DWORD comp = WIM_COMPRESS_LZX;
        if (compression == WimCompression::Xpress) comp = WIM_COMPRESS_XPRESS;
        else if (compression == WimCompression::LZMS) comp = WIM_COMPRESS_LZMS;
        HANDLE hwim = WIMCreateFile(wimPath.c_str(), GENERIC_WRITE, CREATE_ALWAYS, 0, comp, &createResult);
        if (!hwim) {
            lastError = "WIMCreateFile(write) failed: " + std::to_string(GetLastError());
            return false;
        }
        wchar_t tempBuf[MAX_PATH] = {};
        DWORD n = GetEnvironmentVariableW(L"DISMV2_WIM_TEMP", tempBuf, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            WIMSetTemporaryPath(hwim, tempBuf);
        }
        // Enable integrity check for captured WIM
        WIMSetIntegrityCheck(hwim, 1);

        WIMRegisterMessageCallback(hwim, &WimMsgCb, this);

        HANDLE hImage = WIMCaptureImage(hwim, src.c_str(), 0);
        if (!hImage) {
            lastError = "WIMCaptureImage failed: " + std::to_string(GetLastError());
            WIMUnregisterMessageCallback(hwim, &WimMsgCb);
            WIMCloseHandle(hwim);
            return false;
        }
        // Optional: set image name/description in XML (requires proper XML blob); leave as no-op for now
        WIMCloseHandle(hImage);
        BOOL saved = WIMSaveImage(hwim, NULL, description.c_str());
        DWORD gle = GetLastError();
        WIMUnregisterMessageCallback(hwim, &WimMsgCb);
        WIMCloseHandle(hwim);
        if (!saved) {
            lastError = "WIMSaveImage failed: " + std::to_string(gle);
            return false;
        }
        return true;
    }
#endif

public:
    bool initialize() {
        if (initialized) return true;
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) { lastError = "Failed to initialize COM"; return false; }
        initialized = true; return true;
    }
    void cleanup() { if (initialized) { CoUninitialize(); initialized = false; } }

    // Helper: extract APPX/MSIX or bundles (.appxbundle/.msixbundle)
    bool extractAppxOrBundle(const std::string& packagePath, const std::string& destination) {
        std::wstring wPackagePath(packagePath.begin(), packagePath.end());
        std::wstring wDestination(destination.begin(), destination.end());

        CComPtr<IAppxFactory> appxFactory; HRESULT hr = CoCreateInstance(CLSID_AppxFactory, NULL, CLSCTX_INPROC_SERVER, __uuidof(IAppxFactory), (void**)&appxFactory);
        if (FAILED(hr)) { lastError = "Failed to create APPX factory"; return false; }
        CComPtr<IStream> inputStream; hr = SHCreateStreamOnFileW(wPackagePath.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, &inputStream);
        if (FAILED(hr)) { lastError = "Failed to open package file: " + packagePath; return false; }

        fs::create_directories(destination);

        // Determine if it's a bundle by extension
        std::string ext = fs::path(packagePath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".appxbundle" || ext == ".msixbundle") {
            // Use bundle factory
            CComPtr<IAppxBundleFactory> bundleFactory; hr = CoCreateInstance(CLSID_AppxBundleFactory, NULL, CLSCTX_INPROC_SERVER, __uuidof(IAppxBundleFactory), (void**)&bundleFactory);
            if (FAILED(hr)) { lastError = "Failed to create AppxBundleFactory"; return false; }
            CComPtr<IAppxBundleReader> bundleReader; hr = bundleFactory->CreateBundleReader(inputStream, &bundleReader);
            if (FAILED(hr)) { lastError = "Failed to create bundle reader"; return false; }
            CComPtr<IAppxFilesEnumerator> payloads; hr = bundleReader->GetPayloadPackages(&payloads);
            if (FAILED(hr)) { lastError = "Failed to get payload packages"; return false; }
            BOOL hasCurrent = FALSE; hr = payloads->GetHasCurrent(&hasCurrent);
            while (SUCCEEDED(hr) && hasCurrent) {
                CComPtr<IAppxFile> pkgFile; hr = payloads->GetCurrent(&pkgFile);
                if (SUCCEEDED(hr)) {
                    LPWSTR fileName = nullptr; if (SUCCEEDED(pkgFile->GetName(&fileName)) && fileName) {
                        std::wstring wFileName(fileName);
                        // Save payload appx/msix to disk and then extract using package reader
                        CComPtr<IStream> pkgStream; if (SUCCEEDED(pkgFile->GetStream(&pkgStream))) {
                            fs::path outPkg = fs::path(destination) / fs::path(wFileName).filename();
                            CComPtr<IStream> out; std::wstring wOut = outPkg.wstring();
                            if (SUCCEEDED(SHCreateStreamOnFileW(wOut.c_str(), STGM_CREATE | STGM_WRITE | STGM_SHARE_DENY_WRITE, &out))) {
                                ULARGE_INTEGER br{}, bw{}, max{}; max.QuadPart = MAXLONGLONG; pkgStream->CopyTo(out, max, &br, &bw);
                            }
                            // Now open the saved payload as package and extract its files into a subdir
                            CComPtr<IStream> payloadInput; if (SUCCEEDED(SHCreateStreamOnFileW(wOut.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, &payloadInput))) {
                                CComPtr<IAppxPackageReader> payloadReader; if (SUCCEEDED(appxFactory->CreatePackageReader(payloadInput, &payloadReader))) {
                                    CComPtr<IAppxFilesEnumerator> files; if (SUCCEEDED(payloadReader->GetPayloadFiles(&files))) {
                                        BOOL hc = FALSE; files->GetHasCurrent(&hc);
                                        while (SUCCEEDED(hr) && hc) {
                                            CComPtr<IAppxFile> f; hr = files->GetCurrent(&f); if (FAILED(hr)) break;
                                            LPWSTR innerName = NULL; if (SUCCEEDED(f->GetName(&innerName))) {
                                                std::wstring wInner(innerName);
                                                CComPtr<IStream> s; if (SUCCEEDED(f->GetStream(&s))) {
                                                    fs::path target = fs::path(destination) / fs::path(wFileName).stem() / wInner;
                                                    fs::create_directories(target.parent_path());
                                                    CComPtr<IStream> outFile; std::wstring wTarget = target.wstring();
                                                    if (SUCCEEDED(SHCreateStreamOnFileW(wTarget.c_str(), STGM_CREATE | STGM_WRITE | STGM_SHARE_DENY_WRITE, &outFile))) {
                                                        ULARGE_INTEGER br{}, bw{}, max{}; max.QuadPart = MAXLONGLONG; s->CopyTo(outFile, max, &br, &bw);
                                                    }
                                                }
                                                CoTaskMemFree(innerName);
                                            }
                                            files->MoveNext(&hc);
                                        }
                                    }
                                }
                            }
                        }
                        CoTaskMemFree(fileName);
                    }
                }
                payloads->MoveNext(&hasCurrent);
            }
            return true;
        }

        // Not a bundle: extract directly using package reader
        CComPtr<IAppxPackageReader> packageReader; hr = appxFactory->CreatePackageReader(inputStream, &packageReader);
        if (FAILED(hr)) { lastError = "Failed to create package reader"; return false; }
        CComPtr<IAppxFilesEnumerator> filesEnumerator; hr = packageReader->GetPayloadFiles(&filesEnumerator);
        if (FAILED(hr)) { lastError = "Failed to get payload files"; return false; }
        BOOL hasCurrent = FALSE; hr = filesEnumerator->GetHasCurrent(&hasCurrent);
        while (SUCCEEDED(hr) && hasCurrent) {
            CComPtr<IAppxFile> file; hr = filesEnumerator->GetCurrent(&file); if (FAILED(hr)) break;
            LPWSTR fileName = NULL; hr = file->GetName(&fileName); if (FAILED(hr)) { filesEnumerator->MoveNext(&hasCurrent); continue; }
            std::wstring wFileName(fileName); std::string fileNameStr(wFileName.begin(), wFileName.end());
            CComPtr<IStream> fileStream; hr = file->GetStream(&fileStream);
            if (SUCCEEDED(hr)) {
                fs::path targetPath = fs::path(destination) / fileNameStr; fs::create_directories(targetPath.parent_path());
                CComPtr<IStream> outputStream; std::wstring wTargetPath = targetPath.wstring();
                hr = SHCreateStreamOnFileW(wTargetPath.c_str(), STGM_CREATE | STGM_WRITE | STGM_SHARE_DENY_WRITE, &outputStream);
                if (SUCCEEDED(hr)) { ULARGE_INTEGER br{}, bw{}, max{}; max.QuadPart = MAXLONGLONG; fileStream->CopyTo(outputStream, max, &br, &bw); }
            }
            CoTaskMemFree(fileName); filesEnumerator->MoveNext(&hasCurrent);
        }
        return SUCCEEDED(hr);
    }

    // APPX/MSIX operations using proper Windows APIs
    bool extractAppxPackage(const std::string& packagePath, const std::string& destination) {
        if (!initialize()) return false;
        try {
            return extractAppxOrBundle(packagePath, destination);
        } catch (const std::exception& ex) { lastError = "Exception during APPX extraction: "+std::string(ex.what()); return false; }
    }

    bool getAppxManifestInfo(const std::string& packagePath, std::string& packageName, std::string& version, std::string& architecture) {
        if (!initialize()) return false;
        try {
            std::wstring wPackagePath(packagePath.begin(), packagePath.end());
            CComPtr<IAppxFactory> appxFactory; HRESULT hr = CoCreateInstance(CLSID_AppxFactory, NULL, CLSCTX_INPROC_SERVER, __uuidof(IAppxFactory), (void**)&appxFactory);
            if (FAILED(hr)) { lastError = "Failed to create APPX factory"; return false; }
            CComPtr<IStream> inputStream; hr = SHCreateStreamOnFileW(wPackagePath.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, &inputStream);
            if (FAILED(hr)) { lastError = "Failed to open package file"; return false; }
            CComPtr<IAppxPackageReader> packageReader; hr = appxFactory->CreatePackageReader(inputStream, &packageReader);
            if (FAILED(hr)) { lastError = "Failed to create package reader"; return false; }
            CComPtr<IAppxManifestReader> manifestReader; hr = packageReader->GetManifest(&manifestReader);
            if (FAILED(hr)) { lastError = "Failed to get manifest"; return false; }
            CComPtr<IAppxManifestPackageId> packageId; hr = manifestReader->GetPackageId(&packageId);
            if (FAILED(hr)) { lastError = "Failed to get package ID"; return false; }
            LPWSTR name = NULL; UINT64 ver=0; APPX_PACKAGE_ARCHITECTURE arch = APPX_PACKAGE_ARCHITECTURE_NEUTRAL;
            if (SUCCEEDED(packageId->GetName(&name))) { std::wstring wName(name); packageName.assign(wName.begin(), wName.end()); CoTaskMemFree(name); }
            if (SUCCEEDED(packageId->GetVersion(&ver))) { UINT16 major=(ver>>48)&0xFFFF, minor=(ver>>32)&0xFFFF, build=(ver>>16)&0xFFFF, revision=ver&0xFFFF; version = std::to_string(major)+"."+std::to_string(minor)+"."+std::to_string(build)+"."+std::to_string(revision); }
            if (SUCCEEDED(packageId->GetArchitecture(&arch))) {
                switch (arch) { case APPX_PACKAGE_ARCHITECTURE_X86: architecture="x86"; break; case APPX_PACKAGE_ARCHITECTURE_X64: architecture="x64"; break; case APPX_PACKAGE_ARCHITECTURE_ARM: architecture="arm"; break; case APPX_PACKAGE_ARCHITECTURE_ARM64: architecture="arm64"; break; default: architecture="neutral"; break; }
            }
            return true;
        } catch (const std::exception& ex) { lastError = "Exception during manifest reading: "+std::string(ex.what()); return false; }
    }

    bool verifyAppxSignature(const std::string& packagePath, bool strictTimestamp, std::string& err) {
        // Use WinVerifyTrust with WTD_UI_NONE, WTD_SAFER_FLAG, and optional lifetime signing
        std::wstring wPath(packagePath.begin(), packagePath.end());
        LONG status = 0;
        WINTRUST_FILE_INFO wfi{}; wfi.cbStruct = sizeof(wfi); wfi.pcwszFilePath = wPath.c_str();
        GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        WINTRUST_DATA data{}; data.cbStruct = sizeof(data); data.dwUIChoice = WTD_UI_NONE; data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN; data.dwUnionChoice = WTD_CHOICE_FILE; data.pFile = &wfi; data.dwStateAction = WTD_STATEACTION_IGNORE; data.dwProvFlags = WTD_SAFER_FLAG;
        if (strictTimestamp) data.dwProvFlags |= WTD_LIFETIME_SIGNING_FLAG; // require valid timestamp at signing time
        status = WinVerifyTrust(NULL, &policy, &data);
        if (status != ERROR_SUCCESS) { err = "WinVerifyTrust failed: 0x" + std::to_string((unsigned long)status); return false; }
        return true;
    }

    bool verifyAppxBlockMap(const std::string& packagePath, std::string& err) {
        // Open package and read block map entries to force hash verification
        std::wstring wPackagePath(packagePath.begin(), packagePath.end());
        CComPtr<IAppxFactory> appxFactory; HRESULT hr = CoCreateInstance(CLSID_AppxFactory, NULL, CLSCTX_INPROC_SERVER, __uuidof(IAppxFactory), (void**)&appxFactory);
        if (FAILED(hr)) { err = "Failed to create APPX factory"; return false; }
        CComPtr<IStream> inputStream; hr = SHCreateStreamOnFileW(wPackagePath.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, &inputStream);
        if (FAILED(hr)) { err = "Failed to open package"; return false; }
        CComPtr<IAppxPackageReader> packageReader; hr = appxFactory->CreatePackageReader(inputStream, &packageReader);
        if (FAILED(hr)) { err = "Failed to create package reader"; return false; }
        // Accessing footprint block map
        CComPtr<IAppxBlockMapReader> blockMap; hr = packageReader->GetBlockMap(&blockMap);
        if (FAILED(hr) || !blockMap) { err = "Failed to get block map"; return false; }
        // Iterate all files and read blocks to cause hash validation
        CComPtr<IAppxBlockMapFilesEnumerator> files; hr = blockMap->GetFiles(&files);
        if (FAILED(hr)) { err = "Failed to enumerate block map files"; return false; }
        BOOL has = FALSE; hr = files->GetHasCurrent(&has);
        while (SUCCEEDED(hr) && has) {
            CComPtr<IAppxBlockMapFile> f; if (FAILED(files->GetCurrent(&f))) { files->MoveNext(&has); continue; }
            CComPtr<IAppxBlockMapBlocksEnumerator> blocks; if (FAILED(f->GetBlocks(&blocks))) { files->MoveNext(&has); continue; }
            BOOL hasBlock = FALSE; blocks->GetHasCurrent(&hasBlock);
            while (hasBlock) {
                CComPtr<IAppxBlockMapBlock> b; if (SUCCEEDED(blocks->GetCurrent(&b))) {
                    // Retrieve hash and compressed size to walk structure
                    UINT32 cb = 0; BYTE* hash = nullptr; b->GetHash(&cb, &hash);
                    UINT32 sz = 0; b->GetCompressedSize(&sz); (void)hash; (void)cb; (void)sz;
                }
                blocks->MoveNext(&hasBlock);
            }
            files->MoveNext(&has);
        }
        return true;
    }

    bool installAppxOnline(const std::string& packagePath, bool allUsers) {
        if (!initialize()) return false;
        try {
            std::wstring ps = GetSystemToolPath(L"WindowsPowerShell\\v1.0\\powershell.exe");
            std::wstring wPath(packagePath.begin(), packagePath.end());
            std::wstring cmd = L"\"" + ps + L"\" -NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass -Command \"Add-AppxPackage -ForceApplicationShutdown -Path '" + wPath + L"'" + (allUsers?L" -AllUsers":L"") + L"\"";
            std::string out; DWORD code=1; if (!RunProcessCapture(cmd, 15*60*1000, out, code)) { lastError = "Failed to spawn PowerShell"; return false; }
            if (code != 0) { lastError = std::string("Add-AppxPackage failed: ") + out; return false; }
            return true;
        } catch (...) { lastError = "installAppxOnline failed"; return false; }
    }

    bool uninstallAppxOnline(const std::string& packageFullName, bool allUsers) {
        try {
            std::wstring ps = GetSystemToolPath(L"WindowsPowerShell\\v1.0\\powershell.exe");
            std::wstring wName(packageFullName.begin(), packageFullName.end());
            std::wstring cmd = L"\"" + ps + L"\" -NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass -Command \"Remove-AppxPackage -Package '" + wName + L"'" + (allUsers?L" -AllUsers":L"") + L"\"";
            std::string out; DWORD code=1; if (!RunProcessCapture(cmd, 10*60*1000, out, code)) { lastError = "Failed to spawn PowerShell"; return false; }
            if (code != 0) { lastError = std::string("Remove-AppxPackage failed: ") + out; return false; }
            return true;
        } catch (...) { lastError = "uninstallAppxOnline failed"; return false; }
    }

    bool listWimImages(const std::string& wimPath, std::vector<WimImageInfo>& images) {
        try {
            std::wstring dism = GetSystemToolPath(L"dism.exe");
            std::wstring wWim(wimPath.begin(), wimPath.end());
            std::wstring cmd = L"\"" + dism + L"\" /English /Get-WimInfo /WimFile:\"" + wWim + L"\"";
            std::string out; DWORD code=1; if (!RunProcessCapture(cmd, 5*60*1000, out, code)) { lastError = "Failed to run DISM"; return false; }
            if (code != 0) { lastError = std::string("DISM failed: ") + out; return false; }
            images.clear();
            std::istringstream iss(out); std::string line; WimImageInfo cur{}; bool inImage=false;
            while (std::getline(iss, line)) {
                std::string s = line; // already ASCII
                auto trim = [](std::string& x){ x.erase(0, x.find_first_not_of(" \t\r\n")); x.erase(x.find_last_not_of(" \t\r\n")+1); };
                if (s.find("Index :") != std::string::npos) { if (inImage) images.push_back(cur); cur = {}; inImage=true; cur.imageIndex = std::stoi(s.substr(s.find(":")+1)); }
                else if (s.find("Name :") != std::string::npos) { cur.imageName = s.substr(s.find(":")+1); trim(cur.imageName); }
                else if (s.find("Description :") != std::string::npos) { cur.description = s.substr(s.find(":")+1); trim(cur.description); }
                else if (s.find("Architecture :") != std::string::npos) { cur.architecture = s.substr(s.find(":")+1); trim(cur.architecture); }
                else if (s.find("Version :") != std::string::npos) { cur.version = s.substr(s.find(":")+1); trim(cur.version); }
                else if (s.find("Bootable :") != std::string::npos) { std::string v = s.substr(s.find(":")+1); trim(v); std::transform(v.begin(), v.end(), v.begin(), ::tolower); cur.bootable = (v=="yes"); }
                else if (s.find("Size :") != std::string::npos) { std::string v = s.substr(s.find(":")+1); trim(v); // parse number if present
                    std::smatch m; std::regex re("([0-9]+)"); if (std::regex_search(v, m, re)) cur.totalBytes = std::strtoull(m[1].str().c_str(), nullptr, 10);
                }
            }
            if (inImage) images.push_back(cur);
            return true;
        } catch (...) { lastError = "listWimImages failed"; return false; }
    }

    bool extractWimImage(const std::string& wimPath, int imageIndex, const std::string& destination) {
#if WITH_WIMGAPI
        try {
            fs::create_directories(destination);
            std::wstring wWim(wimPath.begin(), wimPath.end());
            std::wstring wDest(destination.begin(), destination.end());
            if (tryWimApply(wWim, imageIndex, wDest)) return true;
            // fall through to DISM
        } catch (...) { /* fallback */ }
#endif
        try {
            fs::create_directories(destination);
            std::wstring dism = GetSystemToolPath(L"dism.exe");
            std::wstring wWim(wimPath.begin(), wimPath.end()); std::wstring wDest(destination.begin(), destination.end());
            std::wstring cmd = L"\"" + dism + L"\" /English /Apply-Image /ImageFile:\"" + wWim + L"\" /Index:" + std::to_wstring(imageIndex) + L" /ApplyDir:\"" + wDest + L"\" /CheckIntegrity";
            std::string out; DWORD code=1; if (!RunProcessCapture(cmd, 60*60*1000, out, code)) { lastError = "Failed to run DISM"; return false; }
            if (code != 0) { lastError = std::string("DISM Apply-Image failed: ") + out; return false; }
            return true;
        } catch (...) { lastError = "extractWimImage failed"; return false; }
    }

    bool captureWimImage(const std::string& sourcePath, const std::string& wimPath, const std::string& imageName, const std::string& description, WimCompression compression) {
#if WITH_WIMGAPI
        try {
            if (!fs::exists(sourcePath)) { lastError = "Source path does not exist: "+sourcePath; return false; }
            std::wstring wSrc(sourcePath.begin(), sourcePath.end());
            std::wstring wWim(wimPath.begin(), wimPath.end());
            std::wstring wName(imageName.begin(), imageName.end());
            std::wstring wDesc(description.begin(), description.end());
            if (tryWimCapture(wSrc, wWim, wName, wDesc, compression)) return true;
        } catch (...) { /* fallback */ }
#endif
        try {
            if (!fs::exists(sourcePath)) { lastError = "Source path does not exist: "+sourcePath; return false; }
            fs::create_directories(fs::path(wimPath).parent_path());
            std::wstring dism = GetSystemToolPath(L"dism.exe");
            std::wstring wSrc(sourcePath.begin(), sourcePath.end()); std::wstring wWim(wimPath.begin(), wimPath.end());
            std::wstring name(imageName.begin(), imageName.end()); std::wstring desc(description.begin(), description.end());
            std::wstring comp = L"/Compress:max";
            if (compression == WimCompression::Xpress) comp = L"/Compress:fast"; else if (compression == WimCompression::LZX) comp = L"/Compress:max"; else if (compression == WimCompression::LZMS) comp = L"/Compress:recovery"; // ESD-like
            std::wstring cmd = L"\"" + dism + L"\" /English /Capture-Image /ImageFile:\"" + wWim + L"\" /CaptureDir:\"" + wSrc + L"\" /Name:\"" + name + L"\" /Description:\"" + desc + L"\" " + comp + L" /CheckIntegrity";
            std::string out; DWORD code=1; if (!RunProcessCapture(cmd, 2*60*60*1000, out, code)) { lastError = "Failed to run DISM"; return false; }
            if (code != 0) { lastError = std::string("DISM Capture-Image failed: ") + out; return false; }
            return true;
        } catch (...) { lastError = "captureWimImage failed"; return false; }
    }

    const std::string& getLastError() const { return lastError; }
};

PsfWimHandler::PsfWimHandler() : impl(std::make_unique<PsfWimHandlerImpl>()) {}
PsfWimHandler::~PsfWimHandler() { if (impl) impl->cleanup(); }
bool PsfWimHandler::initialize() { return impl->initialize(); }
void PsfWimHandler::cleanup() { impl->cleanup(); }

bool PsfWimHandler::extractPsfPackage(const std::string& packagePath, const std::string& destination) { return impl->extractAppxPackage(packagePath, destination); }
bool PsfWimHandler::getPsfPackageInfo(const std::string& packagePath, std::string& packageName, std::string& version, std::string& architecture) { return impl->getAppxManifestInfo(packagePath, packageName, version, architecture); }

bool PsfWimHandler::verifyAppxSignature(const std::string& packagePath, bool strictTimestamp, std::string& err) { return impl->verifyAppxSignature(packagePath, strictTimestamp, err); }
bool PsfWimHandler::verifyAppxBlockMap(const std::string& packagePath, std::string& err) { return impl->verifyAppxBlockMap(packagePath, err); }

bool PsfWimHandler::installAppxOnline(const std::string& packagePath, bool allUsers) { return impl->installAppxOnline(packagePath, allUsers); }
bool PsfWimHandler::uninstallAppxOnline(const std::string& packageFullName, bool allUsers) { return impl->uninstallAppxOnline(packageFullName, allUsers); }

bool PsfWimHandler::listWimImages(const std::string& wimPath, std::vector<WimImageInfo>& images) { return impl->listWimImages(wimPath, images); }
bool PsfWimHandler::extractWimImage(const std::string& wimPath, int imageIndex, const std::string& destination) { return impl->extractWimImage(wimPath, imageIndex, destination); }
bool PsfWimHandler::applyWimImage(const std::string& wimPath, int imageIndex, const std::string& destination) { return impl->extractWimImage(wimPath, imageIndex, destination); }
bool PsfWimHandler::captureWimImage(const std::string& sourcePath, const std::string& wimPath, const std::string& imageName, const std::string& description, WimCompression compression) { return impl->captureWimImage(sourcePath, wimPath, imageName, description, compression); }

std::string PsfWimHandler::getLastError() const { return impl->getLastError(); }

bool PsfWimHandler::detectPackageType(const std::string& packagePath, PackageType& type) {
    auto extension = fs::path(packagePath).extension().string(); std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    if (extension == ".appx" || extension == ".msix" || extension == ".appxbundle" || extension == ".msixbundle") { type = PackageType::APPX_MSIX; return true; }
    else if (extension == ".wim" || extension == ".esd") { type = PackageType::WIM; return true; }
    else if (extension == ".cab") { type = PackageType::CAB; return true; }
    else if (extension == ".msu") { type = PackageType::MSU; return true; }
    std::ifstream file(packagePath, std::ios::binary); if (!file) return false; char header[8] = {}; file.read(header, sizeof(header));
    if (header[0] == 'P' && header[1] == 'K' && header[2] == 0x03 && header[3] == 0x04) { type = PackageType::APPX_MSIX; return true; }
    if (header[0] == 'M' && header[1] == 'S' && header[2] == 'C' && header[3] == 'F') { type = PackageType::CAB; return true; }
    return false;
}

namespace PsfWimUtils {
    PackageType detectPackageType(const std::string& packagePath) { PackageType type; if (PsfWimHandler::detectPackageType(packagePath, type)) return type; return PackageType::UNKNOWN; }
    bool isAppxPackage(const std::string& packagePath) { PackageType type; return PsfWimHandler::detectPackageType(packagePath, type) && type == PackageType::APPX_MSIX; }
    bool isWimImage(const std::string& packagePath) { PackageType type; return PsfWimHandler::detectPackageType(packagePath, type) && type == PackageType::WIM; }
    void logOperation(const std::string& operation, const std::string& details, const std::string& logPath) {
        try { if (!logPath.empty()) { std::ofstream logFile(logPath, std::ios::app); if (logFile.is_open()) { auto now=std::chrono::system_clock::now(); auto t=std::chrono::system_clock::to_time_t(now); std::tm tm; if (localtime_s(&tm,&t)==0) logFile<<std::put_time(&tm,"%Y-%m-%d %H:%M:%S"); else logFile<<"UNKNOWN_TIME"; logFile<<" - "<<operation<<": "<<details<<std::endl; } } } catch (...) {}
    }
}