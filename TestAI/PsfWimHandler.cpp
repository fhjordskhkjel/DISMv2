#include "PsfWimHandler.h"
#include "CabHandler.h"
#include "WimgApiWrapper.h"
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

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")

#include <windows.h>
#include <appxpackaging.h>
#include <shlwapi.h>
#include <comutil.h>
#include <atlbase.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")

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
    std::unique_ptr<WimgApiWrapper> wimgApiWrapper;
    WimProgressCallback progressCallback;
    bool useWimgApi = false;

public:
    bool initialize() {
        if (initialized) return true;
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) { lastError = "Failed to initialize COM"; return false; }
        
        // Try to initialize WIMGAPI wrapper
        wimgApiWrapper = std::make_unique<WimgApiWrapper>();
        useWimgApi = wimgApiWrapper->initialize();
        
        if (!useWimgApi) {
            std::cout << "[INFO] WIMGAPI not available, falling back to DISM-based operations\n";
        } else {
            std::cout << "[INFO] Using native WIMGAPI for WIM operations\n";
        }
        
        initialized = true; 
        return true;
    }
    
    void cleanup() { 
        if (wimgApiWrapper) {
            wimgApiWrapper->cleanup();
            wimgApiWrapper.reset();
        }
        if (initialized) { 
            CoUninitialize(); 
            initialized = false; 
        } 
    }

    void setProgressCallback(WimProgressCallback callback) {
        progressCallback = std::move(callback);
        if (wimgApiWrapper && useWimgApi) {
            // Adapt callback for WimgApiWrapper
            wimgApiWrapper->setProgressCallback([this](WimMessageType type, const WimProgressInfo& progress, const std::string& message) {
                if (progressCallback) {
                    int messageType = static_cast<int>(type);
                    progressCallback(messageType, progress.processedBytes, progress.totalBytes, message);
                }
            });
        }
    }

    // APPX/MSIX operations using proper Windows APIs
    bool extractAppxPackage(const std::string& packagePath, const std::string& destination) {
        if (!initialize()) return false;
        try {
            std::wstring wPackagePath(packagePath.begin(), packagePath.end());
            std::wstring wDestination(destination.begin(), destination.end());
            CComPtr<IAppxFactory> appxFactory;
            HRESULT hr = CoCreateInstance(__uuidof(AppxFactory), NULL, CLSCTX_INPROC_SERVER, __uuidof(IAppxFactory), (void**)&appxFactory);
            if (FAILED(hr)) { lastError = "Failed to create APPX factory"; return false; }
            CComPtr<IStream> inputStream; hr = SHCreateStreamOnFileW(wPackagePath.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, &inputStream);
            if (FAILED(hr)) { lastError = "Failed to open package file: " + packagePath; return false; }
            CComPtr<IAppxPackageReader> packageReader; hr = appxFactory->CreatePackageReader(inputStream, &packageReader);
            if (FAILED(hr)) { lastError = "Failed to create package reader"; return false; }
            CComPtr<IAppxFilesEnumerator> filesEnumerator; hr = packageReader->GetPayloadFiles(&filesEnumerator);
            if (FAILED(hr)) { lastError = "Failed to get payload files"; return false; }
            fs::create_directories(destination);
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
        } catch (const std::exception& ex) { lastError = "Exception during APPX extraction: "+std::string(ex.what()); return false; }
    }

    bool getAppxManifestInfo(const std::string& packagePath, std::string& packageName, std::string& version, std::string& architecture) {
        if (!initialize()) return false;
        try {
            std::wstring wPackagePath(packagePath.begin(), packagePath.end());
            CComPtr<IAppxFactory> appxFactory; HRESULT hr = CoCreateInstance(__uuidof(AppxFactory), NULL, CLSCTX_INPROC_SERVER, __uuidof(IAppxFactory), (void**)&appxFactory);
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
        if (useWimgApi && wimgApiWrapper) {
            return listWimImagesNative(wimPath, images);
        } else {
            return listWimImagesDism(wimPath, images);
        }
    }

    bool extractWimImage(const std::string& wimPath, int imageIndex, const std::string& destination, 
                        bool verifyIntegrity, bool preserveAcls, bool preserveTimestamps, bool preserveReparsePoints) {
        if (useWimgApi && wimgApiWrapper) {
            return extractWimImageNative(wimPath, imageIndex, destination, verifyIntegrity, preserveAcls, preserveTimestamps, preserveReparsePoints);
        } else {
            return extractWimImageDism(wimPath, imageIndex, destination);
        }
    }

    bool captureWimImage(const std::string& sourcePath, const std::string& wimPath, const std::string& imageName, 
                        const std::string& description, WimCompression compression, bool verifyIntegrity) {
        if (useWimgApi && wimgApiWrapper) {
            return captureWimImageNative(sourcePath, wimPath, imageName, description, compression, verifyIntegrity);
        } else {
            return captureWimImageDism(sourcePath, wimPath, imageName, description, compression);
        }
    }

    bool verifyWimIntegrity(const std::string& wimPath) {
        if (useWimgApi && wimgApiWrapper) {
            // Open WIM with integrity checking enabled
            HANDLE wimHandle = wimgApiWrapper->createWimFile(wimPath, WimAccessMode::Read);
            if (!wimHandle) {
                lastError = "Failed to open WIM for integrity verification: " + wimgApiWrapper->getLastError().userFriendlyMessage;
                return false;
            }

            bool success = wimgApiWrapper->setIntegrityCheck(wimHandle, true);
            wimgApiWrapper->closeHandle(wimHandle);
            
            if (!success) {
                lastError = "Integrity verification failed: " + wimgApiWrapper->getLastError().userFriendlyMessage;
                return false;
            }
            return true;
        } else {
            // Use DISM for verification
            std::wstring dism = GetSystemToolPath(L"dism.exe");
            std::wstring wWim(wimPath.begin(), wimPath.end());
            std::wstring cmd = L"\"" + dism + L"\" /English /Get-WimInfo /WimFile:\"" + wWim + L"\" /CheckIntegrity";
            std::string out; 
            DWORD code = 1; 
            if (!RunProcessCapture(cmd, 5*60*1000, out, code)) { 
                lastError = "Failed to run DISM"; 
                return false; 
            }
            if (code != 0) { 
                lastError = std::string("DISM integrity check failed: ") + out; 
                return false; 
            }
            return true;
        }
    }

    bool validateCompressionType(const std::string& wimPath, WimCompression compression) {
        WimCompressionType nativeCompression;
        switch (compression) {
            case WimCompression::None:
                nativeCompression = WimCompressionType::None;
                break;
            case WimCompression::Xpress:
                nativeCompression = WimCompressionType::Xpress;
                break;
            case WimCompression::LZX:
                nativeCompression = WimCompressionType::LZX;
                break;
            case WimCompression::LZMS:
                nativeCompression = WimCompressionType::LZMS;
                break;
            default:
                return false;
        }
        
        return WimgApiWrapper::validateCompressionForFile(wimPath, nativeCompression);
    }

private:
    bool listWimImagesNative(const std::string& wimPath, std::vector<WimImageInfo>& images) {
        HANDLE wimHandle = wimgApiWrapper->createWimFile(wimPath, WimAccessMode::Read);
        if (!wimHandle) {
            lastError = "Failed to open WIM file: " + wimgApiWrapper->getLastError().userFriendlyMessage;
            return false;
        }

        bool success = wimgApiWrapper->getImageInformation(wimHandle, images);
        wimgApiWrapper->closeHandle(wimHandle);
        
        if (!success) {
            lastError = "Failed to get image information: " + wimgApiWrapper->getLastError().userFriendlyMessage;
            return false;
        }
        
        return true;
    }

    bool extractWimImageNative(const std::string& wimPath, int imageIndex, const std::string& destination,
                              bool verifyIntegrity, bool preserveAcls, bool preserveTimestamps, bool preserveReparsePoints) {
        HANDLE wimHandle = wimgApiWrapper->createWimFile(wimPath, WimAccessMode::Read);
        if (!wimHandle) {
            lastError = "Failed to open WIM file: " + wimgApiWrapper->getLastError().userFriendlyMessage;
            return false;
        }

        // Set integrity checking if requested
        if (verifyIntegrity) {
            wimgApiWrapper->setIntegrityCheck(wimHandle, true);
        }

        HANDLE imageHandle = wimgApiWrapper->loadImage(wimHandle, imageIndex);
        if (!imageHandle) {
            lastError = "Failed to load image: " + wimgApiWrapper->getLastError().userFriendlyMessage;
            wimgApiWrapper->closeHandle(wimHandle);
            return false;
        }

        // Build flags based on options
        DWORD flags = 0;
        if (!preserveAcls) flags |= 0x00000008; // WIM_FLAG_NO_APPLY_ACL
        if (!preserveTimestamps) flags |= 0x00000010; // WIM_FLAG_NO_APPLY_SECURITY  
        if (!preserveReparsePoints) flags |= 0x00000100; // WIM_FLAG_NO_RP_FIX

        bool success = wimgApiWrapper->applyImage(imageHandle, destination, flags);
        
        wimgApiWrapper->closeHandle(imageHandle);
        wimgApiWrapper->closeHandle(wimHandle);
        
        if (!success) {
            lastError = "Failed to extract image: " + wimgApiWrapper->getLastError().userFriendlyMessage;
            return false;
        }
        
        return true;
    }

    bool captureWimImageNative(const std::string& sourcePath, const std::string& wimPath, const std::string& imageName, 
                              const std::string& description, WimCompression compression, bool verifyIntegrity) {
        // Validate compression type
        if (!validateCompressionType(wimPath, compression)) {
            lastError = "Invalid compression type for file: " + wimPath;
            return false;
        }

        WimCompressionType nativeCompression;
        switch (compression) {
            case WimCompression::None: nativeCompression = WimCompressionType::None; break;
            case WimCompression::Xpress: nativeCompression = WimCompressionType::Xpress; break;
            case WimCompression::LZX: nativeCompression = WimCompressionType::LZX; break;
            case WimCompression::LZMS: nativeCompression = WimCompressionType::LZMS; break;
            default: nativeCompression = WimCompressionType::LZX; break;
        }

        HANDLE wimHandle = wimgApiWrapper->createWimFile(wimPath, WimAccessMode::Write, CREATE_ALWAYS, nativeCompression);
        if (!wimHandle) {
            lastError = "Failed to create WIM file: " + wimgApiWrapper->getLastError().userFriendlyMessage;
            return false;
        }

        // Set integrity checking if requested
        if (verifyIntegrity) {
            wimgApiWrapper->setIntegrityCheck(wimHandle, true);
        }

        HANDLE imageHandle = wimgApiWrapper->captureImage(wimHandle, sourcePath, 0);
        if (!imageHandle) {
            lastError = "Failed to capture image: " + wimgApiWrapper->getLastError().userFriendlyMessage;
            wimgApiWrapper->closeHandle(wimHandle);
            return false;
        }

        wimgApiWrapper->closeHandle(imageHandle);
        wimgApiWrapper->closeHandle(wimHandle);
        return true;
    }

    bool listWimImagesDism(const std::string& wimPath, std::vector<WimImageInfo>& images) {
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

    bool extractWimImageDism(const std::string& wimPath, int imageIndex, const std::string& destination) {
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

    bool captureWimImageDism(const std::string& sourcePath, const std::string& wimPath, const std::string& imageName, const std::string& description, WimCompression compression) {
        try {
            if (!fs::exists(sourcePath)) { lastError = "Source path does not exist: "+sourcePath; return false; }
            fs::create_directories(fs::path(wimPath).parent_path());
            std::wstring dism = GetSystemToolPath(L"dism.exe");
            std::wstring wSrc(sourcePath.begin(), sourcePath.end()); std::wstring wWim(wimPath.begin(), wimPath.end());
            std::wstring name(imageName.begin(), imageName.end()); std::wstring desc(description.begin(), description.end());
            std::wstring comp = L"/Compress:max"; // default LZX-like
            if (compression == WimCompression::Xpress) comp = L"/Compress:fast"; else if (compression == WimCompression::LZX) comp = L"/Compress:max"; else if (compression == WimCompression::LZMS) comp = L"/Compress:max"; // Note: LZMS requires ESD, not WIM
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

bool PsfWimHandler::installAppxOnline(const std::string& packagePath, bool allUsers) { return impl->installAppxOnline(packagePath, allUsers); }
bool PsfWimHandler::uninstallAppxOnline(const std::string& packageFullName, bool allUsers) { return impl->uninstallAppxOnline(packageFullName, allUsers); }

bool PsfWimHandler::listWimImages(const std::string& wimPath, std::vector<WimImageInfo>& images) { return impl->listWimImages(wimPath, images); }
bool PsfWimHandler::extractWimImage(const std::string& wimPath, int imageIndex, const std::string& destination, 
                                   bool verifyIntegrity, bool preserveAcls, bool preserveTimestamps, bool preserveReparsePoints) { 
    return impl->extractWimImage(wimPath, imageIndex, destination, verifyIntegrity, preserveAcls, preserveTimestamps, preserveReparsePoints); 
}
bool PsfWimHandler::applyWimImage(const std::string& wimPath, int imageIndex, const std::string& destination,
                                 bool verifyIntegrity, bool preserveAcls, bool preserveTimestamps, bool preserveReparsePoints) { 
    return impl->extractWimImage(wimPath, imageIndex, destination, verifyIntegrity, preserveAcls, preserveTimestamps, preserveReparsePoints); 
}
bool PsfWimHandler::captureWimImage(const std::string& sourcePath, const std::string& wimPath, const std::string& imageName, 
                                   const std::string& description, WimCompression compression, bool verifyIntegrity) { 
    return impl->captureWimImage(sourcePath, wimPath, imageName, description, compression, verifyIntegrity); 
}

void PsfWimHandler::setProgressCallback(WimProgressCallback callback) { impl->setProgressCallback(std::move(callback)); }
bool PsfWimHandler::verifyWimIntegrity(const std::string& wimPath) { return impl->verifyWimIntegrity(wimPath); }
bool PsfWimHandler::validateCompressionType(const std::string& wimPath, WimCompression compression) { return impl->validateCompressionType(wimPath, compression); }

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

bool PsfWimHandler::isWimgapiAvailable() {
    return WimgApiWrapper::isWimgapiAvailable();
}

namespace PsfWimUtils {
    PackageType detectPackageType(const std::string& packagePath) { PackageType type; if (PsfWimHandler::detectPackageType(packagePath, type)) return type; return PackageType::UNKNOWN; }
    bool isAppxPackage(const std::string& packagePath) { PackageType type; return PsfWimHandler::detectPackageType(packagePath, type) && type == PackageType::APPX_MSIX; }
    bool isWimImage(const std::string& packagePath) { PackageType type; return PsfWimHandler::detectPackageType(packagePath, type) && type == PackageType::WIM; }
    void logOperation(const std::string& operation, const std::string& details, const std::string& logPath) {
        try { if (!logPath.empty()) { std::ofstream logFile(logPath, std::ios::app); if (logFile.is_open()) { auto now=std::chrono::system_clock::now(); auto t=std::chrono::system_clock::to_time_t(now); std::tm tm; if (localtime_s(&tm,&t)==0) logFile<<std::put_time(&tm,"%Y-%m-%d %H:%M:%S"); else logFile<<"UNKNOWN_TIME"; logFile<<" - "<<operation<<": "<<details<<std::endl; } } } catch (...) {}
    }
}