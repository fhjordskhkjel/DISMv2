#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <windows.h>

// Forward declarations
struct WimImageInfo;

// WIM compression types
enum class WimCompressionType {
    None = 0,
    Xpress = 1,  // WIM_COMPRESS_XPRESS
    LZX = 2,     // WIM_COMPRESS_LZX  
    LZMS = 3     // WIM_COMPRESS_LZMS (ESD only)
};

// WIM access mode flags
enum class WimAccessMode {
    Read = 0x80000000,   // WIM_GENERIC_READ
    Write = 0x40000000   // WIM_GENERIC_WRITE
};

// WIM message types for progress callback
enum class WimMessageType {
    Progress = 0x00000001,    // WIM_MSG_PROGRESS
    ProcessStart = 0x00000002, // WIM_MSG_PROCESS
    Error = 0x00000010,       // WIM_MSG_ERROR
    Warning = 0x00000020,     // WIM_MSG_WARNING
    Info = 0x00000040         // WIM_MSG_INFO
};

// Progress information structure
struct WimProgressInfo {
    uint64_t totalBytes = 0;
    uint64_t processedBytes = 0;
    double percentComplete = 0.0;
    double throughputMBps = 0.0;
    std::chrono::seconds estimatedTimeRemaining{0};
    std::string currentOperation;
    std::string currentFile;
};

// Error information structure
struct WimErrorInfo {
    DWORD errorCode = 0;
    std::string errorMessage;
    std::string userFriendlyMessage;
    std::string context;
};

// Progress callback function type
using WimProgressCallback = std::function<void(WimMessageType type, const WimProgressInfo& progress, const std::string& message)>;

/**
 * @brief Abstraction wrapper for WIMGAPI using LoadLibrary/GetProcAddress pattern
 * 
 * This class provides a clean interface to Windows Imaging API (WIMGAPI) functions
 * with proper error handling, progress tracking, and Win32 error mapping.
 */
class WimgApiWrapper {
private:
    // WIMGAPI function pointer types
    using HANDLE_t = HANDLE;
    using WIMCreateFile_t = HANDLE_t(WINAPI*)(LPCWSTR, DWORD, DWORD, DWORD, DWORD, PDWORD);
    using WIMLoadImage_t = HANDLE_t(WINAPI*)(HANDLE_t, DWORD);
    using WIMApplyImage_t = BOOL(WINAPI*)(HANDLE_t, LPCWSTR, DWORD);
    using WIMCaptureImage_t = HANDLE_t(WINAPI*)(HANDLE_t, LPCWSTR, DWORD);
    using WIMRegisterMessageCallback_t = DWORD(WINAPI*)(HANDLE_t, FARPROC, PVOID);
    using WIMUnregisterMessageCallback_t = DWORD(WINAPI*)(HANDLE_t, FARPROC);
    using WIMSetTemporaryPath_t = DWORD(WINAPI*)(HANDLE_t, LPCWSTR);
    using WIMSetReferenceFile_t = DWORD(WINAPI*)(HANDLE_t, LPCWSTR, DWORD);
    using WIMSetIntegrityCheck_t = DWORD(WINAPI*)(HANDLE_t, BOOL);
    using WIMCloseHandle_t = BOOL(WINAPI*)(HANDLE_t);
    using WIMGetImageCount_t = DWORD(WINAPI*)(HANDLE_t);
    using WIMGetImageInformation_t = BOOL(WINAPI*)(HANDLE_t, PVOID*, PDWORD);
    using WIMSetImageInformation_t = BOOL(WINAPI*)(HANDLE_t, LPCVOID, DWORD);

    // Dynamic library and function pointers
    HMODULE m_wimgapiHandle = nullptr;
    WIMCreateFile_t m_WIMCreateFile = nullptr;
    WIMLoadImage_t m_WIMLoadImage = nullptr;
    WIMApplyImage_t m_WIMApplyImage = nullptr;
    WIMCaptureImage_t m_WIMCaptureImage = nullptr;
    WIMRegisterMessageCallback_t m_WIMRegisterMessageCallback = nullptr;
    WIMUnregisterMessageCallback_t m_WIMUnregisterMessageCallback = nullptr;
    WIMSetTemporaryPath_t m_WIMSetTemporaryPath = nullptr;
    WIMSetReferenceFile_t m_WIMSetReferenceFile = nullptr;
    WIMSetIntegrityCheck_t m_WIMSetIntegrityCheck = nullptr;
    WIMCloseHandle_t m_WIMCloseHandle = nullptr;
    WIMGetImageCount_t m_WIMGetImageCount = nullptr;
    WIMGetImageInformation_t m_WIMGetImageInformation = nullptr;
    WIMSetImageInformation_t m_WIMSetImageInformation = nullptr;

    // State management
    bool m_initialized = false;
    WimErrorInfo m_lastError;
    WimProgressCallback m_progressCallback;
    std::chrono::steady_clock::time_point m_operationStartTime;
    uint64_t m_lastProcessedBytes = 0;
    std::chrono::steady_clock::time_point m_lastThroughputUpdate;

    // Internal helper methods
    bool loadWimgapiLibrary();
    bool loadFunctionPointers();
    void setLastError(DWORD errorCode, const std::string& context);
    std::string getWin32ErrorMessage(DWORD errorCode) const;
    std::string mapErrorToUserMessage(DWORD errorCode) const;
    void updateProgressInfo(WimProgressInfo& progress) const;
    
    // Static callback wrapper for WIMGAPI
    static DWORD WINAPI progressCallbackWrapper(DWORD messageId, WPARAM wParam, LPARAM lParam, PVOID userData);

public:
    WimgApiWrapper();
    ~WimgApiWrapper();

    // Disable copy construction and assignment
    WimgApiWrapper(const WimgApiWrapper&) = delete;
    WimgApiWrapper& operator=(const WimgApiWrapper&) = delete;

    // Enable move construction and assignment
    WimgApiWrapper(WimgApiWrapper&&) = default;
    WimgApiWrapper& operator=(WimgApiWrapper&&) = default;

    /**
     * @brief Initialize the WIMGAPI wrapper
     * @return True if initialization succeeded
     */
    bool initialize();

    /**
     * @brief Cleanup and release resources
     */
    void cleanup();

    /**
     * @brief Check if the wrapper is initialized
     * @return True if initialized
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * @brief Set progress callback for operations
     * @param callback Function to call for progress updates
     */
    void setProgressCallback(WimProgressCallback callback) { m_progressCallback = std::move(callback); }

    /**
     * @brief Create or open a WIM file
     * @param wimPath Path to WIM file
     * @param accessMode Read or Write access
     * @param creationDisposition Creation flags (CREATE_NEW, OPEN_EXISTING, etc.)
     * @param compression Compression type for new WIMs
     * @return Handle to WIM file or nullptr on failure
     */
    HANDLE createWimFile(const std::string& wimPath, WimAccessMode accessMode, 
                        DWORD creationDisposition = OPEN_EXISTING, 
                        WimCompressionType compression = WimCompressionType::LZX);

    /**
     * @brief Load an image from a WIM file
     * @param wimHandle Handle to WIM file
     * @param imageIndex Image index (1-based)
     * @return Handle to loaded image or nullptr on failure
     */
    HANDLE loadImage(HANDLE wimHandle, DWORD imageIndex);

    /**
     * @brief Apply/extract a WIM image to a directory
     * @param imageHandle Handle to loaded image
     * @param targetPath Target directory path
     * @param flags Apply flags (WIM_FLAG_*)
     * @return True if successful
     */
    bool applyImage(HANDLE imageHandle, const std::string& targetPath, DWORD flags = 0);

    /**
     * @brief Capture a directory to a WIM image
     * @param wimHandle Handle to WIM file
     * @param sourcePath Source directory to capture
     * @param flags Capture flags (WIM_FLAG_*)
     * @return Handle to captured image or nullptr on failure
     */
    HANDLE captureImage(HANDLE wimHandle, const std::string& sourcePath, DWORD flags = 0);

    /**
     * @brief Set temporary directory for WIM operations
     * @param wimHandle Handle to WIM file
     * @param tempPath Temporary directory path
     * @return True if successful
     */
    bool setTemporaryPath(HANDLE wimHandle, const std::string& tempPath);

    /**
     * @brief Set reference file for split WIMs
     * @param wimHandle Handle to WIM file
     * @param refPath Reference file path
     * @param flags Reference flags
     * @return True if successful
     */
    bool setReferenceFile(HANDLE wimHandle, const std::string& refPath, DWORD flags = 0);

    /**
     * @brief Enable or disable integrity checking
     * @param wimHandle Handle to WIM file
     * @param enable True to enable integrity checking
     * @return True if successful
     */
    bool setIntegrityCheck(HANDLE wimHandle, bool enable);

    /**
     * @brief Close a WIM handle
     * @param handle Handle to close
     * @return True if successful
     */
    bool closeHandle(HANDLE handle);

    /**
     * @brief Get number of images in a WIM file
     * @param wimHandle Handle to WIM file
     * @return Number of images or 0 on failure
     */
    DWORD getImageCount(HANDLE wimHandle);

    /**
     * @brief Get image information from a WIM file
     * @param wimHandle Handle to WIM file
     * @param images Vector to store image information
     * @return True if successful
     */
    bool getImageInformation(HANDLE wimHandle, std::vector<WimImageInfo>& images);

    /**
     * @brief Get the last error information
     * @return Error information structure
     */
    const WimErrorInfo& getLastError() const { return m_lastError; }

    /**
     * @brief Check if WIMGAPI is available on the system
     * @return True if wimgapi.dll can be loaded
     */
    static bool isWimgapiAvailable();

    /**
     * @brief Validate compression type for file extension
     * @param filePath WIM file path
     * @param compression Requested compression type
     * @return True if compression is valid for file type
     */
    static bool validateCompressionForFile(const std::string& filePath, WimCompressionType compression);
};