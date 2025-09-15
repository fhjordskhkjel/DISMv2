#include "WimgApiWrapper.h"
#include "CabHandler.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <iomanip>

#pragma comment(lib, "kernel32.lib")

namespace fs = std::filesystem;

// WIMGAPI constants (from wimgapi.h)
#define WIM_GENERIC_READ           0x80000000
#define WIM_GENERIC_WRITE          0x40000000
#define WIM_COMPRESS_NONE          0x00000000
#define WIM_COMPRESS_XPRESS        0x00000001
#define WIM_COMPRESS_LZX           0x00000002
#define WIM_COMPRESS_LZMS          0x00000003
#define WIM_FLAG_VERIFY            0x00000002
#define WIM_FLAG_INDEX             0x00000004
#define WIM_FLAG_NO_APPLY_ACL      0x00000008
#define WIM_FLAG_NO_APPLY_SECURITY 0x00000010
#define WIM_FLAG_NO_RP_FIX         0x00000100

// WIMGAPI message types
#define WIM_MSG                    0x00008000
#define WIM_MSG_TEXT               (WIM_MSG | 0x0001)
#define WIM_MSG_PROGRESS           (WIM_MSG | 0x0002)
#define WIM_MSG_PROCESS            (WIM_MSG | 0x0004)
#define WIM_MSG_SCANNING           (WIM_MSG | 0x0008)
#define WIM_MSG_SETRANGE           (WIM_MSG | 0x0010)
#define WIM_MSG_SETPOS             (WIM_MSG | 0x0020)
#define WIM_MSG_STEPIT             (WIM_MSG | 0x0040)
#define WIM_MSG_COMPRESS           (WIM_MSG | 0x0080)
#define WIM_MSG_ERROR              (WIM_MSG | 0x0100)
#define WIM_MSG_ALIGNMENT          (WIM_MSG | 0x0200)
#define WIM_MSG_RETRY              (WIM_MSG | 0x0400)
#define WIM_MSG_SPLIT              (WIM_MSG | 0x0800)
#define WIM_MSG_SUCCESS            (WIM_MSG | 0x1000)

// Additional WIMGAPI error codes
#define ERROR_INVALID_IMAGE_HASH   0xC1420117

namespace {
    // Convert wide string to UTF-8
    std::string wideToUtf8(const std::wstring& wide) {
        if (wide.empty()) return {};
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (sizeNeeded <= 0) return {};
        std::string result(sizeNeeded - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), sizeNeeded, nullptr, nullptr);
        return result;
    }

    // Convert UTF-8 to wide string
    std::wstring utf8ToWide(const std::string& utf8) {
        if (utf8.empty()) return {};
        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        if (sizeNeeded <= 0) return {};
        std::wstring result(sizeNeeded - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, result.data(), sizeNeeded);
        return result;
    }

    // Format bytes with appropriate units
    std::string formatBytes(uint64_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unitIndex = 0;
        double size = static_cast<double>(bytes);
        
        while (size >= 1024.0 && unitIndex < 4) {
            size /= 1024.0;
            unitIndex++;
        }
        
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
        return oss.str();
    }

    // Format time duration
    std::string formatDuration(std::chrono::seconds duration) {
        auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration - hours);
        auto seconds = duration - hours - minutes;
        
        if (hours.count() > 0) {
            return std::to_string(hours.count()) + "h " + 
                   std::to_string(minutes.count()) + "m " + 
                   std::to_string(seconds.count()) + "s";
        } else if (minutes.count() > 0) {
            return std::to_string(minutes.count()) + "m " + 
                   std::to_string(seconds.count()) + "s";
        } else {
            return std::to_string(seconds.count()) + "s";
        }
    }
}

WimgApiWrapper::WimgApiWrapper() = default;

WimgApiWrapper::~WimgApiWrapper() {
    cleanup();
}

bool WimgApiWrapper::initialize() {
    if (m_initialized) {
        return true;
    }

    if (!loadWimgapiLibrary()) {
        setLastError(GetLastError(), "Failed to load wimgapi.dll");
        return false;
    }

    if (!loadFunctionPointers()) {
        cleanup();
        setLastError(GetLastError(), "Failed to load WIMGAPI function pointers");
        return false;
    }

    m_initialized = true;
    return true;
}

void WimgApiWrapper::cleanup() {
    if (m_wimgapiHandle) {
        FreeLibrary(m_wimgapiHandle);
        m_wimgapiHandle = nullptr;
    }

    // Reset all function pointers
    m_WIMCreateFile = nullptr;
    m_WIMLoadImage = nullptr;
    m_WIMApplyImage = nullptr;
    m_WIMCaptureImage = nullptr;
    m_WIMRegisterMessageCallback = nullptr;
    m_WIMUnregisterMessageCallback = nullptr;
    m_WIMSetTemporaryPath = nullptr;
    m_WIMSetReferenceFile = nullptr;
    m_WIMSetIntegrityCheck = nullptr;
    m_WIMCloseHandle = nullptr;
    m_WIMGetImageCount = nullptr;
    m_WIMGetImageInformation = nullptr;
    m_WIMSetImageInformation = nullptr;

    m_initialized = false;
}

bool WimgApiWrapper::loadWimgapiLibrary() {
    // Try to load wimgapi.dll from system directory
    m_wimgapiHandle = LoadLibraryW(L"wimgapi.dll");
    if (!m_wimgapiHandle) {
        // Try loading from System32 explicitly
        wchar_t systemDir[MAX_PATH];
        if (GetSystemDirectoryW(systemDir, MAX_PATH) > 0) {
            std::wstring wimgapiPath = std::wstring(systemDir) + L"\\wimgapi.dll";
            m_wimgapiHandle = LoadLibraryW(wimgapiPath.c_str());
        }
    }

    return m_wimgapiHandle != nullptr;
}

bool WimgApiWrapper::loadFunctionPointers() {
    if (!m_wimgapiHandle) {
        return false;
    }

    // Load all required function pointers
    m_WIMCreateFile = reinterpret_cast<WIMCreateFile_t>(GetProcAddress(m_wimgapiHandle, "WIMCreateFile"));
    m_WIMLoadImage = reinterpret_cast<WIMLoadImage_t>(GetProcAddress(m_wimgapiHandle, "WIMLoadImage"));
    m_WIMApplyImage = reinterpret_cast<WIMApplyImage_t>(GetProcAddress(m_wimgapiHandle, "WIMApplyImage"));
    m_WIMCaptureImage = reinterpret_cast<WIMCaptureImage_t>(GetProcAddress(m_wimgapiHandle, "WIMCaptureImage"));
    m_WIMRegisterMessageCallback = reinterpret_cast<WIMRegisterMessageCallback_t>(GetProcAddress(m_wimgapiHandle, "WIMRegisterMessageCallback"));
    m_WIMUnregisterMessageCallback = reinterpret_cast<WIMUnregisterMessageCallback_t>(GetProcAddress(m_wimgapiHandle, "WIMUnregisterMessageCallback"));
    m_WIMSetTemporaryPath = reinterpret_cast<WIMSetTemporaryPath_t>(GetProcAddress(m_wimgapiHandle, "WIMSetTemporaryPath"));
    m_WIMSetReferenceFile = reinterpret_cast<WIMSetReferenceFile_t>(GetProcAddress(m_wimgapiHandle, "WIMSetReferenceFile"));
    m_WIMSetIntegrityCheck = reinterpret_cast<WIMSetIntegrityCheck_t>(GetProcAddress(m_wimgapiHandle, "WIMSetIntegrityCheck"));
    m_WIMCloseHandle = reinterpret_cast<WIMCloseHandle_t>(GetProcAddress(m_wimgapiHandle, "WIMCloseHandle"));
    m_WIMGetImageCount = reinterpret_cast<WIMGetImageCount_t>(GetProcAddress(m_wimgapiHandle, "WIMGetImageCount"));
    m_WIMGetImageInformation = reinterpret_cast<WIMGetImageInformation_t>(GetProcAddress(m_wimgapiHandle, "WIMGetImageInformation"));
    m_WIMSetImageInformation = reinterpret_cast<WIMSetImageInformation_t>(GetProcAddress(m_wimgapiHandle, "WIMSetImageInformation"));

    // Check that all critical functions were loaded
    return m_WIMCreateFile && m_WIMLoadImage && m_WIMApplyImage && m_WIMCaptureImage &&
           m_WIMRegisterMessageCallback && m_WIMSetTemporaryPath && m_WIMSetReferenceFile &&
           m_WIMSetIntegrityCheck && m_WIMCloseHandle && m_WIMGetImageCount &&
           m_WIMGetImageInformation;
}

void WimgApiWrapper::setLastError(DWORD errorCode, const std::string& context) {
    m_lastError.errorCode = errorCode;
    m_lastError.errorMessage = getWin32ErrorMessage(errorCode);
    m_lastError.userFriendlyMessage = mapErrorToUserMessage(errorCode);
    m_lastError.context = context;
}

std::string WimgApiWrapper::getWin32ErrorMessage(DWORD errorCode) const {
    if (errorCode == 0) {
        return "Success";
    }

    LPWSTR messageBuffer = nullptr;
    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&messageBuffer),
        0,
        nullptr
    );

    std::string message;
    if (size > 0 && messageBuffer) {
        message = wideToUtf8(messageBuffer);
        // Remove trailing whitespace
        message.erase(message.find_last_not_of(" \t\r\n") + 1);
    } else {
        message = "Unknown error (0x" + std::to_string(errorCode) + ")";
    }

    if (messageBuffer) {
        LocalFree(messageBuffer);
    }

    return message;
}

std::string WimgApiWrapper::mapErrorToUserMessage(DWORD errorCode) const {
    switch (errorCode) {
    case ERROR_FILE_NOT_FOUND:
        return "The specified WIM file was not found.";
    case ERROR_PATH_NOT_FOUND:
        return "The specified path was not found.";
    case ERROR_ACCESS_DENIED:
        return "Access denied. Please run as administrator or check file permissions.";
    case ERROR_SHARING_VIOLATION:
        return "The WIM file is in use by another process.";
    case ERROR_INVALID_PARAMETER:
        return "Invalid parameter provided to WIM operation.";
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return "Insufficient memory to complete the operation.";
    case ERROR_DISK_FULL:
        return "Insufficient disk space to complete the operation.";
    case ERROR_INVALID_DATA:
        return "The WIM file appears to be corrupted or invalid.";
    case ERROR_CRC:
        return "Data integrity check failed. The WIM file may be corrupted.";
    case ERROR_IO_DEVICE:
        return "I/O device error occurred during WIM operation.";
    case ERROR_INVALID_IMAGE_HASH:
        return "Image hash verification failed. The WIM file may be corrupted.";
    case 0xC1420117: // WIM_ERROR_INVALID_IMAGE
        return "Invalid WIM image specified.";
    case 0xC1420118: // WIM_ERROR_IMAGE_NOT_FOUND
        return "The specified image was not found in the WIM file.";
    default:
        if (errorCode >= 0xC1420100 && errorCode <= 0xC14201FF) {
            return "WIM-specific error occurred.";
        }
        return "An unexpected error occurred during WIM operation.";
    }
}

DWORD WINAPI WimgApiWrapper::progressCallbackWrapper(DWORD messageId, WPARAM wParam, LPARAM lParam, PVOID userData) {
    auto* wrapper = static_cast<WimgApiWrapper*>(userData);
    if (!wrapper || !wrapper->m_progressCallback) {
        return WIM_MSG_SUCCESS;
    }

    WimProgressInfo progress;
    std::string message;

    switch (messageId) {
    case WIM_MSG_PROGRESS:
        {
            // wParam contains progress info, lParam contains additional data
            auto currentBytes = static_cast<uint64_t>(wParam);
            auto totalBytes = static_cast<uint64_t>(lParam);
            
            progress.processedBytes = currentBytes;
            progress.totalBytes = totalBytes;
            progress.percentComplete = totalBytes > 0 ? (static_cast<double>(currentBytes) / totalBytes) * 100.0 : 0.0;
            
            wrapper->updateProgressInfo(progress);
            
            message = "Processing: " + formatBytes(currentBytes) + " / " + formatBytes(totalBytes) + 
                     " (" + std::to_string(static_cast<int>(progress.percentComplete)) + "%)";
            
            wrapper->m_progressCallback(WimMessageType::Progress, progress, message);
        }
        break;

    case WIM_MSG_PROCESS:
        {
            // Process start message
            LPCWSTR processName = reinterpret_cast<LPCWSTR>(lParam);
            if (processName) {
                progress.currentOperation = wideToUtf8(processName);
                message = "Starting: " + progress.currentOperation;
            } else {
                message = "Starting operation...";
            }
            wrapper->m_progressCallback(WimMessageType::ProcessStart, progress, message);
        }
        break;

    case WIM_MSG_SCANNING:
        {
            LPCWSTR fileName = reinterpret_cast<LPCWSTR>(lParam);
            if (fileName) {
                progress.currentFile = wideToUtf8(fileName);
                message = "Scanning: " + progress.currentFile;
            } else {
                message = "Scanning files...";
            }
            wrapper->m_progressCallback(WimMessageType::Info, progress, message);
        }
        break;

    case WIM_MSG_ERROR:
        {
            LPCWSTR errorMsg = reinterpret_cast<LPCWSTR>(lParam);
            if (errorMsg) {
                message = "Error: " + wideToUtf8(errorMsg);
            } else {
                message = "An error occurred during WIM operation";
            }
            wrapper->m_progressCallback(WimMessageType::Error, progress, message);
        }
        break;

    case WIM_MSG_TEXT:
        {
            LPCWSTR textMsg = reinterpret_cast<LPCWSTR>(lParam);
            if (textMsg) {
                message = wideToUtf8(textMsg);
                wrapper->m_progressCallback(WimMessageType::Info, progress, message);
            }
        }
        break;

    default:
        // Other message types, treat as info
        message = "WIM operation in progress...";
        wrapper->m_progressCallback(WimMessageType::Info, progress, message);
        break;
    }

    return WIM_MSG_SUCCESS;
}

void WimgApiWrapper::updateProgressInfo(WimProgressInfo& progress) const {
    auto now = std::chrono::steady_clock::now();
    
    // Calculate throughput
    if (m_lastThroughputUpdate.time_since_epoch().count() > 0) {
        auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastThroughputUpdate).count();
        if (timeDiff > 1000) { // Update every second
            auto bytesDiff = progress.processedBytes - m_lastProcessedBytes;
            progress.throughputMBps = (static_cast<double>(bytesDiff) / (1024 * 1024)) / (timeDiff / 1000.0);
        }
    }

    // Calculate ETA
    if (progress.throughputMBps > 0 && progress.totalBytes > 0 && progress.processedBytes < progress.totalBytes) {
        auto remainingBytes = progress.totalBytes - progress.processedBytes;
        auto remainingMB = static_cast<double>(remainingBytes) / (1024 * 1024);
        auto etaSeconds = static_cast<int64_t>(remainingMB / progress.throughputMBps);
        progress.estimatedTimeRemaining = std::chrono::seconds(etaSeconds);
    }
}

HANDLE WimgApiWrapper::createWimFile(const std::string& wimPath, WimAccessMode accessMode, 
                                    DWORD creationDisposition, WimCompressionType compression) {
    if (!m_initialized || !m_WIMCreateFile) {
        setLastError(ERROR_NOT_READY, "WimgApiWrapper not initialized");
        return nullptr;
    }

    DWORD access = static_cast<DWORD>(accessMode);
    DWORD compressionType = static_cast<DWORD>(compression);
    
    std::wstring wWimPath = utf8ToWide(wimPath);
    DWORD lastError = 0;
    
    HANDLE handle = m_WIMCreateFile(
        wWimPath.c_str(),
        access,
        creationDisposition,
        0, // flags
        compressionType,
        &lastError
    );

    if (!handle || handle == INVALID_HANDLE_VALUE) {
        setLastError(lastError ? lastError : GetLastError(), "Failed to create/open WIM file: " + wimPath);
        return nullptr;
    }

    // Register progress callback if set
    if (m_progressCallback && m_WIMRegisterMessageCallback) {
        m_WIMRegisterMessageCallback(handle, reinterpret_cast<FARPROC>(progressCallbackWrapper), this);
    }

    m_operationStartTime = std::chrono::steady_clock::now();
    m_lastProcessedBytes = 0;
    m_lastThroughputUpdate = std::chrono::steady_clock::time_point{};

    return handle;
}

HANDLE WimgApiWrapper::loadImage(HANDLE wimHandle, DWORD imageIndex) {
    if (!m_initialized || !m_WIMLoadImage) {
        setLastError(ERROR_NOT_READY, "WimgApiWrapper not initialized");
        return nullptr;
    }

    if (!wimHandle || wimHandle == INVALID_HANDLE_VALUE) {
        setLastError(ERROR_INVALID_PARAMETER, "Invalid WIM handle");
        return nullptr;
    }

    HANDLE imageHandle = m_WIMLoadImage(wimHandle, imageIndex);
    if (!imageHandle || imageHandle == INVALID_HANDLE_VALUE) {
        setLastError(GetLastError(), "Failed to load image " + std::to_string(imageIndex));
        return nullptr;
    }

    return imageHandle;
}

bool WimgApiWrapper::applyImage(HANDLE imageHandle, const std::string& targetPath, DWORD flags) {
    if (!m_initialized || !m_WIMApplyImage) {
        setLastError(ERROR_NOT_READY, "WimgApiWrapper not initialized");
        return false;
    }

    if (!imageHandle || imageHandle == INVALID_HANDLE_VALUE) {
        setLastError(ERROR_INVALID_PARAMETER, "Invalid image handle");
        return false;
    }

    // Create target directory if it doesn't exist
    try {
        fs::create_directories(targetPath);
    } catch (const std::exception& e) {
        setLastError(ERROR_PATH_NOT_FOUND, "Failed to create target directory: " + std::string(e.what()));
        return false;
    }

    std::wstring wTargetPath = utf8ToWide(targetPath);
    BOOL success = m_WIMApplyImage(imageHandle, wTargetPath.c_str(), flags);

    if (!success) {
        setLastError(GetLastError(), "Failed to apply image to: " + targetPath);
        return false;
    }

    return true;
}

HANDLE WimgApiWrapper::captureImage(HANDLE wimHandle, const std::string& sourcePath, DWORD flags) {
    if (!m_initialized || !m_WIMCaptureImage) {
        setLastError(ERROR_NOT_READY, "WimgApiWrapper not initialized");
        return nullptr;
    }

    if (!wimHandle || wimHandle == INVALID_HANDLE_VALUE) {
        setLastError(ERROR_INVALID_PARAMETER, "Invalid WIM handle");
        return nullptr;
    }

    if (!fs::exists(sourcePath)) {
        setLastError(ERROR_PATH_NOT_FOUND, "Source path does not exist: " + sourcePath);
        return nullptr;
    }

    std::wstring wSourcePath = utf8ToWide(sourcePath);
    HANDLE imageHandle = m_WIMCaptureImage(wimHandle, wSourcePath.c_str(), flags);

    if (!imageHandle || imageHandle == INVALID_HANDLE_VALUE) {
        setLastError(GetLastError(), "Failed to capture image from: " + sourcePath);
        return nullptr;
    }

    return imageHandle;
}

bool WimgApiWrapper::setTemporaryPath(HANDLE wimHandle, const std::string& tempPath) {
    if (!m_initialized || !m_WIMSetTemporaryPath) {
        setLastError(ERROR_NOT_READY, "WimgApiWrapper not initialized");
        return false;
    }

    if (!wimHandle || wimHandle == INVALID_HANDLE_VALUE) {
        setLastError(ERROR_INVALID_PARAMETER, "Invalid WIM handle");
        return false;
    }

    std::wstring wTempPath = utf8ToWide(tempPath);
    DWORD result = m_WIMSetTemporaryPath(wimHandle, wTempPath.c_str());

    if (result != ERROR_SUCCESS) {
        setLastError(result, "Failed to set temporary path: " + tempPath);
        return false;
    }

    return true;
}

bool WimgApiWrapper::setReferenceFile(HANDLE wimHandle, const std::string& refPath, DWORD flags) {
    if (!m_initialized || !m_WIMSetReferenceFile) {
        setLastError(ERROR_NOT_READY, "WimgApiWrapper not initialized");
        return false;
    }

    if (!wimHandle || wimHandle == INVALID_HANDLE_VALUE) {
        setLastError(ERROR_INVALID_PARAMETER, "Invalid WIM handle");
        return false;
    }

    std::wstring wRefPath = utf8ToWide(refPath);
    DWORD result = m_WIMSetReferenceFile(wimHandle, wRefPath.c_str(), flags);

    if (result != ERROR_SUCCESS) {
        setLastError(result, "Failed to set reference file: " + refPath);
        return false;
    }

    return true;
}

bool WimgApiWrapper::setIntegrityCheck(HANDLE wimHandle, bool enable) {
    if (!m_initialized || !m_WIMSetIntegrityCheck) {
        setLastError(ERROR_NOT_READY, "WimgApiWrapper not initialized");
        return false;
    }

    if (!wimHandle || wimHandle == INVALID_HANDLE_VALUE) {
        setLastError(ERROR_INVALID_PARAMETER, "Invalid WIM handle");
        return false;
    }

    DWORD result = m_WIMSetIntegrityCheck(wimHandle, enable ? TRUE : FALSE);

    if (result != ERROR_SUCCESS) {
        setLastError(result, "Failed to " + std::string(enable ? "enable" : "disable") + " integrity check");
        return false;
    }

    return true;
}

bool WimgApiWrapper::closeHandle(HANDLE handle) {
    if (!m_initialized || !m_WIMCloseHandle) {
        return false; // Don't set error for cleanup operations
    }

    if (!handle || handle == INVALID_HANDLE_VALUE) {
        return true; // Already closed
    }

    // Unregister callback if it was registered
    if (m_progressCallback && m_WIMUnregisterMessageCallback) {
        m_WIMUnregisterMessageCallback(handle, reinterpret_cast<FARPROC>(progressCallbackWrapper));
    }

    BOOL success = m_WIMCloseHandle(handle);
    if (!success) {
        setLastError(GetLastError(), "Failed to close WIM handle");
        return false;
    }

    return true;
}

DWORD WimgApiWrapper::getImageCount(HANDLE wimHandle) {
    if (!m_initialized || !m_WIMGetImageCount) {
        setLastError(ERROR_NOT_READY, "WimgApiWrapper not initialized");
        return 0;
    }

    if (!wimHandle || wimHandle == INVALID_HANDLE_VALUE) {
        setLastError(ERROR_INVALID_PARAMETER, "Invalid WIM handle");
        return 0;
    }

    return m_WIMGetImageCount(wimHandle);
}

bool WimgApiWrapper::getImageInformation(HANDLE wimHandle, std::vector<WimImageInfo>& images) {
    if (!m_initialized || !m_WIMGetImageInformation) {
        setLastError(ERROR_NOT_READY, "WimgApiWrapper not initialized");
        return false;
    }

    if (!wimHandle || wimHandle == INVALID_HANDLE_VALUE) {
        setLastError(ERROR_INVALID_PARAMETER, "Invalid WIM handle");
        return false;
    }

    DWORD imageCount = getImageCount(wimHandle);
    if (imageCount == 0) {
        return true; // Empty WIM is valid
    }

    images.clear();
    images.reserve(imageCount);

    for (DWORD i = 1; i <= imageCount; ++i) {
        HANDLE imageHandle = loadImage(wimHandle, i);
        if (!imageHandle) {
            continue; // Skip invalid images
        }

        WimImageInfo info;
        info.imageIndex = i;
        
        // Get image information buffer
        PVOID imageInfoBuffer = nullptr;
        DWORD bufferSize = 0;
        
        if (m_WIMGetImageInformation(imageHandle, &imageInfoBuffer, &bufferSize) && imageInfoBuffer) {
            // Parse XML information (simplified parsing)
            std::string xmlData(static_cast<char*>(imageInfoBuffer), bufferSize);
            
            // Extract basic information using simple string parsing
            auto findValue = [&xmlData](const std::string& tag) -> std::string {
                auto startTag = "<" + tag + ">";
                auto endTag = "</" + tag + ">";
                auto startPos = xmlData.find(startTag);
                if (startPos != std::string::npos) {
                    startPos += startTag.length();
                    auto endPos = xmlData.find(endTag, startPos);
                    if (endPos != std::string::npos) {
                        return xmlData.substr(startPos, endPos - startPos);
                    }
                }
                return "";
            };

            info.imageName = findValue("NAME");
            info.description = findValue("DESCRIPTION");
            info.architecture = findValue("ARCHITECTURE");
            info.version = findValue("VERSION");
            
            auto sizeStr = findValue("TOTALBYTES");
            if (!sizeStr.empty()) {
                info.totalBytes = std::strtoull(sizeStr.c_str(), nullptr, 10);
            }

            auto bootableStr = findValue("BOOTABLE");
            info.bootable = (bootableStr == "1" || bootableStr == "true");

            // Free the buffer
            LocalFree(imageInfoBuffer);
        }

        closeHandle(imageHandle);
        images.push_back(info);
    }

    return true;
}

bool WimgApiWrapper::isWimgapiAvailable() {
    HMODULE handle = LoadLibraryW(L"wimgapi.dll");
    if (handle) {
        FreeLibrary(handle);
        return true;
    }
    return false;
}

bool WimgApiWrapper::validateCompressionForFile(const std::string& filePath, WimCompressionType compression) {
    auto ext = fs::path(filePath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // LZMS compression is only valid for .esd files
    if (compression == WimCompressionType::LZMS && ext != ".esd") {
        return false;
    }

    // All other combinations are valid
    return true;
}