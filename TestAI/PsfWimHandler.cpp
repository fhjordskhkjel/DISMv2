#include "PsfWimHandler.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <memory>
#include <algorithm>
#include <chrono>

// Windows APIs for APPX/MSIX handling
#include <windows.h>
#include <appxpackaging.h>
#include <shlwapi.h>
#include <comutil.h>
#include <atlbase.h>

// WIM API
#include <wimgapi.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "wimgapi.lib")

namespace fs = std::filesystem;

class PsfWimHandlerImpl {
private:
    bool initialized = false;
    std::string lastError;
    
public:
    bool initialize() {
        if (initialized) return true;
        
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) {
            lastError = "Failed to initialize COM";
            return false;
        }
        
        initialized = true;
        return true;
    }
    
    void cleanup() {
        if (initialized) {
            CoUninitialize();
            initialized = false;
        }
    }
    
    // APPX/MSIX operations using proper Windows APIs
    bool extractAppxPackage(const std::string& packagePath, const std::string& destination) {
        if (!initialize()) return false;
        
        try {
            std::wstring wPackagePath(packagePath.begin(), packagePath.end());
            std::wstring wDestination(destination.begin(), destination.end());
            
            // Create AppX factory
            CComPtr<IAppxFactory> appxFactory;
            HRESULT hr = CoCreateInstance(__uuidof(AppxFactory), NULL, CLSCTX_INPROC_SERVER,
                                         __uuidof(IAppxFactory), (void**)&appxFactory);
            if (FAILED(hr)) {
                lastError = "Failed to create APPX factory";
                return false;
            }
            
            // Open package file
            CComPtr<IStream> inputStream;
            hr = SHCreateStreamOnFileW(wPackagePath.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, &inputStream);
            if (FAILED(hr)) {
                lastError = "Failed to open package file: " + packagePath;
                return false;
            }
            
            // Create package reader
            CComPtr<IAppxPackageReader> packageReader;
            hr = appxFactory->CreatePackageReader(inputStream, &packageReader);
            if (FAILED(hr)) {
                lastError = "Failed to create package reader";
                return false;
            }
            
            // Get payload files
            CComPtr<IAppxFilesEnumerator> filesEnumerator;
            hr = packageReader->GetPayloadFiles(&filesEnumerator);
            if (FAILED(hr)) {
                lastError = "Failed to get payload files";
                return false;
            }
            
            // Create destination directory
            fs::create_directories(destination);
            
            // Extract files
            BOOL hasCurrent = FALSE;
            hr = filesEnumerator->GetHasCurrent(&hasCurrent);
            
            while (SUCCEEDED(hr) && hasCurrent) {
                CComPtr<IAppxFile> file;
                hr = filesEnumerator->GetCurrent(&file);
                if (FAILED(hr)) break;
                
                // Get file name
                LPWSTR fileName = NULL;
                hr = file->GetName(&fileName);
                if (FAILED(hr)) {
                    filesEnumerator->MoveNext(&hasCurrent);
                    continue;
                }
                
                std::wstring wFileName(fileName);
                std::string fileNameStr(wFileName.begin(), wFileName.end());
                
                // Get file stream
                CComPtr<IStream> fileStream;
                hr = file->GetStream(&fileStream);
                if (SUCCEEDED(hr)) {
                    // Create target file path
                    fs::path targetPath = fs::path(destination) / fileNameStr;
                    fs::create_directories(targetPath.parent_path());
                    
                    // Create output file
                    CComPtr<IStream> outputStream;
                    std::wstring wTargetPath = targetPath.wstring();
                    hr = SHCreateStreamOnFileW(wTargetPath.c_str(), 
                                              STGM_CREATE | STGM_WRITE | STGM_SHARE_DENY_WRITE, 
                                              &outputStream);
                    if (SUCCEEDED(hr)) {
                        // Copy stream
                        ULARGE_INTEGER bytesRead, bytesWritten;
                        hr = fileStream->CopyTo(outputStream, {MAXULONGLONG, MAXULONGLONG}, 
                                              &bytesRead, &bytesWritten);
                    }
                }
                
                CoTaskMemFree(fileName);
                filesEnumerator->MoveNext(&hasCurrent);
            }
            
            return SUCCEEDED(hr);
            
        } catch (const std::exception& ex) {
            lastError = "Exception during APPX extraction: " + std::string(ex.what());
            return false;
        }
    }
    
    bool getAppxManifestInfo(const std::string& packagePath, std::string& packageName, 
                           std::string& version, std::string& architecture) {
        if (!initialize()) return false;
        
        try {
            std::wstring wPackagePath(packagePath.begin(), packagePath.end());
            
            // Create AppX factory
            CComPtr<IAppxFactory> appxFactory;
            HRESULT hr = CoCreateInstance(__uuidof(AppxFactory), NULL, CLSCTX_INPROC_SERVER,
                                         __uuidof(IAppxFactory), (void**)&appxFactory);
            if (FAILED(hr)) {
                lastError = "Failed to create APPX factory";
                return false;
            }
            
            // Open package file
            CComPtr<IStream> inputStream;
            hr = SHCreateStreamOnFileW(wPackagePath.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, &inputStream);
            if (FAILED(hr)) {
                lastError = "Failed to open package file";
                return false;
            }
            
            // Create package reader
            CComPtr<IAppxPackageReader> packageReader;
            hr = appxFactory->CreatePackageReader(inputStream, &packageReader);
            if (FAILED(hr)) {
                lastError = "Failed to create package reader";
                return false;
            }
            
            // Get manifest
            CComPtr<IAppxManifestReader> manifestReader;
            hr = packageReader->GetManifest(&manifestReader);
            if (FAILED(hr)) {
                lastError = "Failed to get manifest";
                return false;
            }
            
            // Get package ID
            CComPtr<IAppxManifestPackageId> packageId;
            hr = manifestReader->GetPackageId(&packageId);
            if (FAILED(hr)) {
                lastError = "Failed to get package ID";
                return false;
            }
            
            // Extract information
            LPWSTR name = NULL, ver = NULL, arch = NULL;
            
            if (SUCCEEDED(packageId->GetName(&name))) {
                std::wstring wName(name);
                packageName = std::string(wName.begin(), wName.end());
                CoTaskMemFree(name);
            }
            
            if (SUCCEEDED(packageId->GetVersion(&ver))) {
                std::wstring wVer(ver);
                version = std::string(wVer.begin(), wVer.end());
                CoTaskMemFree(ver);
            }
            
            if (SUCCEEDED(packageId->GetArchitecture(&arch))) {
                std::wstring wArch(arch);
                architecture = std::string(wArch.begin(), wArch.end());
                CoTaskMemFree(arch);
            }
            
            return true;
            
        } catch (const std::exception& ex) {
            lastError = "Exception during manifest reading: " + std::string(ex.what());
            return false;
        }
    }
    
    // WIM operations using wimgapi.dll
    bool listWimImages(const std::string& wimPath, std::vector<WimImageInfo>& images) {
        try {
            std::wstring wWimPath(wimPath.begin(), wimPath.end());
            
            // Open WIM file
            HANDLE wimHandle = WIMCreateFile(wWimPath.c_str(), WIM_GENERIC_READ, 
                                           WIM_OPEN_EXISTING, 0, 0, NULL);
            if (wimHandle == INVALID_HANDLE_VALUE) {
                lastError = "Failed to open WIM file: " + wimPath;
                return false;
            }
            
            // Get image count
            DWORD imageCount = 0;
            if (!WIMGetImageCount(wimHandle, &imageCount)) {
                lastError = "Failed to get WIM image count";
                WIMCloseHandle(wimHandle);
                return false;
            }
            
            images.clear();
            
            // Enumerate images
            for (DWORD i = 1; i <= imageCount; i++) {
                WimImageInfo imageInfo;
                imageInfo.index = i;
                
                // Get image information
                DWORD infoSize = 0;
                WIMGetImageInformation(wimHandle, i, NULL, &infoSize);
                
                if (infoSize > 0) {
                    std::vector<wchar_t> buffer(infoSize / sizeof(wchar_t));
                    if (WIMGetImageInformation(wimHandle, i, buffer.data(), &infoSize)) {
                        std::wstring wInfo(buffer.data());
                        imageInfo.description = std::string(wInfo.begin(), wInfo.end());
                        
                        // Parse basic info from XML (simplified)
                        size_t nameStart = wInfo.find(L"<NAME>");
                        size_t nameEnd = wInfo.find(L"</NAME>");
                        if (nameStart != std::wstring::npos && nameEnd != std::wstring::npos) {
                            nameStart += 6; // Length of "<NAME>"
                            std::wstring wName = wInfo.substr(nameStart, nameEnd - nameStart);
                            imageInfo.name = std::string(wName.begin(), wName.end());
                        }
                    }
                }
                
                images.push_back(imageInfo);
            }
            
            WIMCloseHandle(wimHandle);
            return true;
            
        } catch (const std::exception& ex) {
            lastError = "Exception during WIM listing: " + std::string(ex.what());
            return false;
        }
    }
    
    bool extractWimImage(const std::string& wimPath, int imageIndex, const std::string& destination) {
        try {
            std::wstring wWimPath(wimPath.begin(), wimPath.end());
            std::wstring wDestination(destination.begin(), destination.end());
            
            // Open WIM file
            HANDLE wimHandle = WIMCreateFile(wWimPath.c_str(), WIM_GENERIC_READ, 
                                           WIM_OPEN_EXISTING, 0, 0, NULL);
            if (wimHandle == INVALID_HANDLE_VALUE) {
                lastError = "Failed to open WIM file: " + wimPath;
                return false;
            }
            
            // Load image
            HANDLE imageHandle = WIMLoadImage(wimHandle, imageIndex);
            if (imageHandle == INVALID_HANDLE_VALUE) {
                lastError = "Failed to load WIM image at index: " + std::to_string(imageIndex);
                WIMCloseHandle(wimHandle);
                return false;
            }
            
            // Create destination directory
            fs::create_directories(destination);
            
            // Apply image
            if (!WIMApplyImage(imageHandle, wDestination.c_str(), 0)) {
                lastError = "Failed to apply WIM image to: " + destination;
                WIMCloseHandle(imageHandle);
                WIMCloseHandle(wimHandle);
                return false;
            }
            
            WIMCloseHandle(imageHandle);
            WIMCloseHandle(wimHandle);
            return true;
            
        } catch (const std::exception& ex) {
            lastError = "Exception during WIM extraction: " + std::string(ex.what());
            return false;
        }
    }
    
    bool captureWimImage(const std::string& sourcePath, const std::string& wimPath, 
                        const std::string& imageName, const std::string& description) {
        try {
            std::wstring wSourcePath(sourcePath.begin(), sourcePath.end());
            std::wstring wWimPath(wimPath.begin(), wimPath.end());
            std::wstring wImageName(imageName.begin(), imageName.end());
            std::wstring wDescription(description.begin(), description.end());
            
            // Create or open WIM file
            HANDLE wimHandle = WIMCreateFile(wWimPath.c_str(), WIM_GENERIC_WRITE, 
                                           WIM_CREATE_NEW, WIM_FLAG_VERIFY, 
                                           WIM_COMPRESS_TYPE_LZX, NULL);
            if (wimHandle == INVALID_HANDLE_VALUE) {
                // Try to open existing
                wimHandle = WIMCreateFile(wWimPath.c_str(), WIM_GENERIC_WRITE, 
                                        WIM_OPEN_EXISTING, 0, 0, NULL);
                if (wimHandle == INVALID_HANDLE_VALUE) {
                    lastError = "Failed to create/open WIM file: " + wimPath;
                    return false;
                }
            }
            
            // Capture image
            HANDLE imageHandle = WIMCaptureImage(wimHandle, wSourcePath.c_str(), 0);
            if (imageHandle == INVALID_HANDLE_VALUE) {
                lastError = "Failed to capture image from: " + sourcePath;
                WIMCloseHandle(wimHandle);
                return false;
            }
            
            // Set image information
            std::wstring imageInfo = L"<WIM><IMAGE><NAME>" + wImageName + 
                                   L"</NAME><DESCRIPTION>" + wDescription + 
                                   L"</DESCRIPTION></IMAGE></WIM>";
            
            WIMSetImageInformation(imageHandle, imageInfo.c_str(), 
                                 static_cast<DWORD>(imageInfo.length() * sizeof(wchar_t)));
            
            WIMCloseHandle(imageHandle);
            WIMCloseHandle(wimHandle);
            return true;
            
        } catch (const std::exception& ex) {
            lastError = "Exception during WIM capture: " + std::string(ex.what());
            return false;
        }
    }
    
    const std::string& getLastError() const { return lastError; }
};

// PsfWimHandler implementation
PsfWimHandler::PsfWimHandler() : impl(std::make_unique<PsfWimHandlerImpl>()) {
}

PsfWimHandler::~PsfWimHandler() {
    if (impl) {
        impl->cleanup();
    }
}

bool PsfWimHandler::initialize() {
    return impl->initialize();
}

void PsfWimHandler::cleanup() {
    impl->cleanup();
}

bool PsfWimHandler::extractPsfPackage(const std::string& packagePath, const std::string& destination) {
    return impl->extractAppxPackage(packagePath, destination);
}

bool PsfWimHandler::getPsfPackageInfo(const std::string& packagePath, std::string& packageName, 
                                    std::string& version, std::string& architecture) {
    return impl->getAppxManifestInfo(packagePath, packageName, version, architecture);
}

bool PsfWimHandler::listWimImages(const std::string& wimPath, std::vector<WimImageInfo>& images) {
    return impl->listWimImages(wimPath, images);
}

bool PsfWimHandler::extractWimImage(const std::string& wimPath, int imageIndex, const std::string& destination) {
    return impl->extractWimImage(wimPath, imageIndex, destination);
}

bool PsfWimHandler::applyWimImage(const std::string& wimPath, int imageIndex, const std::string& destination) {
    // Apply is the same as extract for our purposes
    return impl->extractWimImage(wimPath, imageIndex, destination);
}

bool PsfWimHandler::captureWimImage(const std::string& sourcePath, const std::string& wimPath, 
                                  const std::string& imageName, const std::string& description) {
    return impl->captureWimImage(sourcePath, wimPath, imageName, description);
}

std::string PsfWimHandler::getLastError() const {
    return impl->getLastError();
}

// Static utility functions
bool PsfWimHandler::detectPackageType(const std::string& packagePath, PackageType& type) {
    auto extension = fs::path(packagePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == ".appx" || extension == ".msix" || extension == ".appxbundle" || extension == ".msixbundle") {
        type = PackageType::APPX_MSIX;
        return true;
    } else if (extension == ".wim" || extension == ".esd") {
        type = PackageType::WIM;
        return true;
    } else if (extension == ".cab") {
        type = PackageType::CAB;
        return true;
    } else if (extension == ".msu") {
        type = PackageType::MSU;
        return true;
    }
    
    // Try to detect by file signature
    std::ifstream file(packagePath, std::ios::binary);
    if (!file) return false;
    
    char header[8] = {};
    file.read(header, sizeof(header));
    
    // Check for ZIP signature (APPX/MSIX)
    if (header[0] == 'P' && header[1] == 'K' && header[2] == 0x03 && header[3] == 0x04) {
        type = PackageType::APPX_MSIX;
        return true;
    }
    
    // Check for CAB signature
    if (header[0] == 'M' && header[1] == 'S' && header[2] == 'C' && header[3] == 'F') {
        type = PackageType::CAB;
        return true;
    }
    
    return false;
}