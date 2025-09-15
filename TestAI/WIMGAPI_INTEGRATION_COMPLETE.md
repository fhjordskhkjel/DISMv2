# WIMGAPI Integration Implementation

This document describes the comprehensive WIMGAPI integration implemented for DISMv2, providing clean abstraction layers and enhanced WIM operations as requested.

## Overview

The implementation provides a complete abstraction wrapper around Windows Imaging API (WIMGAPI) with the following key features:

- **Dynamic Library Loading**: Uses LoadLibrary/GetProcAddress pattern for maximum compatibility
- **Intelligent Fallback**: Automatically falls back to DISM when WIMGAPI is unavailable
- **Progress Tracking**: Real-time progress with throughput and ETA calculations
- **Integrity Verification**: Built-in integrity checking and hash verification
- **Comprehensive Error Handling**: Win32 error codes mapped to user-friendly messages
- **Compression Support**: Full support for Xpress, LZX, and LZMS compression

## Architecture

### WimgApiWrapper Class

The core `WimgApiWrapper` class provides a clean interface to WIMGAPI functions:

```cpp
class WimgApiWrapper {
    // Core WIMGAPI functions with typed pointers
    WIMCreateFile_t m_WIMCreateFile;
    WIMLoadImage_t m_WIMLoadImage;
    WIMApplyImage_t m_WIMApplyImage;
    WIMCaptureImage_t m_WIMCaptureImage;
    WIMRegisterMessageCallback_t m_WIMRegisterMessageCallback;
    WIMSetTemporaryPath_t m_WIMSetTemporaryPath;
    WIMSetReferenceFile_t m_WIMSetReferenceFile;
    WIMSetIntegrityCheck_t m_WIMSetIntegrityCheck;
    WIMCloseHandle_t m_WIMCloseHandle;
    // ... additional functions
};
```

### Enhanced PsfWimHandler

The existing `PsfWimHandler` has been enhanced to use the new wrapper while maintaining backward compatibility:

- **Native WIMGAPI Operations**: When available, uses native WIMGAPI for better performance
- **DISM Fallback**: Seamlessly falls back to DISM when WIMGAPI is not available
- **Unified Interface**: Same API regardless of underlying implementation

## New Command Line Features

### Enhanced WIM Operations

#### extract-wim (Enhanced)
```bash
TestAI.exe extract-wim <wim> <index> <destination> [options]

Options:
  --wim-verify           - Enable integrity verification
  --no-acls              - Don't preserve ACLs
  --no-timestamps        - Don't preserve timestamps
  --no-reparse-points    - Don't preserve reparse points
```

#### capture-wim (Enhanced)
```bash
TestAI.exe capture-wim <source> <wim> <name> <description> [options]

Options:
  --compress {xpress|lzx|lzms}  - Set compression type (default: lzx)
  --wim-verify                  - Enable integrity verification
```

#### list-wim (Enhanced)
```bash
TestAI.exe list-wim <wim> [--wim-verify]
```

#### verify-wim (New)
```bash
TestAI.exe verify-wim <wim>
```

### Compression Types

| Type   | Description                    | Use Case                           | File Support |
|--------|--------------------------------|------------------------------------|--------------|
| xpress | Fast compression, larger files | Quick operations, less CPU usage  | .wim         |
| lzx    | Balanced compression (default) | General purpose, good balance      | .wim         |
| lzms   | Maximum compression            | Space-critical deployments        | .esd only    |

## Implementation Details

### Dynamic Library Loading

The wrapper uses `LoadLibrary/GetProcAddress` pattern for maximum compatibility:

```cpp
bool WimgApiWrapper::loadWimgapiLibrary() {
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
```

### Progress Callbacks

Progress information includes:
- **Percentage Complete**: Real-time progress tracking
- **Throughput**: MB/s transfer rate calculation
- **ETA**: Estimated time remaining
- **Current Operation**: Description of current activity
- **Current File**: File being processed (when available)

```cpp
struct WimProgressInfo {
    uint64_t totalBytes = 0;
    uint64_t processedBytes = 0;
    double percentComplete = 0.0;
    double throughputMBps = 0.0;
    std::chrono::seconds estimatedTimeRemaining{0};
    std::string currentOperation;
    std::string currentFile;
};
```

### Error Handling

Comprehensive error mapping from Win32 error codes to user-friendly messages:

```cpp
std::string WimgApiWrapper::mapErrorToUserMessage(DWORD errorCode) const {
    switch (errorCode) {
    case ERROR_FILE_NOT_FOUND:
        return "The specified WIM file was not found.";
    case ERROR_ACCESS_DENIED:
        return "Access denied. Please run as administrator or check file permissions.";
    case ERROR_INVALID_DATA:
        return "The WIM file appears to be corrupted or invalid.";
    case ERROR_CRC:
        return "Data integrity check failed. The WIM file may be corrupted.";
    // ... additional mappings
    }
}
```

### Integrity Verification

Built-in integrity checking using `WIMSetIntegrityCheck`:

```cpp
bool WimgApiWrapper::setIntegrityCheck(HANDLE wimHandle, bool enable) {
    DWORD result = m_WIMSetIntegrityCheck(wimHandle, enable ? TRUE : FALSE);
    if (result != ERROR_SUCCESS) {
        setLastError(result, "Failed to " + std::string(enable ? "enable" : "disable") + " integrity check");
        return false;
    }
    return true;
}
```

## Compression Validation

Automatic validation ensures LZMS compression is only used with .esd files:

```cpp
bool WimgApiWrapper::validateCompressionForFile(const std::string& filePath, WimCompressionType compression) {
    auto ext = fs::path(filePath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // LZMS compression is only valid for .esd files
    if (compression == WimCompressionType::LZMS && ext != ".esd") {
        return false;
    }
    return true;
}
```

## Backward Compatibility

The implementation maintains full backward compatibility:

1. **Existing Commands**: All existing WIM commands continue to work
2. **DISM Fallback**: When WIMGAPI is unavailable, operations fall back to DISM
3. **API Compatibility**: Existing code using PsfWimHandler continues to work unchanged
4. **Default Behavior**: Default options provide the same behavior as before

## Testing

Use the provided test script to validate functionality:

```bash
test_wimgapi_integration.bat
```

The test validates:
- Basic executable functionality
- WIMGAPI availability detection
- Enhanced WIM command availability
- Compression validation
- Error handling

## Benefits

### Performance Improvements
- **Native WIMGAPI**: Direct API calls eliminate DISM process overhead
- **Progress Tracking**: Real-time feedback instead of DISM text parsing
- **Memory Efficiency**: Direct memory operations vs. process communication

### Enhanced Functionality
- **Granular Control**: Individual flags for ACLs, timestamps, reparse points
- **Integrity Verification**: Built-in hash checking and validation
- **Compression Options**: Full control over compression algorithms
- **Error Reporting**: Detailed, user-friendly error messages

### Compatibility
- **Universal Operation**: Works on systems with or without WIMGAPI
- **Graceful Degradation**: Automatic fallback to proven DISM operations
- **Future-Proof**: Easy to extend with additional WIMGAPI features

## Conclusion

This implementation provides a comprehensive, production-ready WIMGAPI integration that significantly enhances WIM operation capabilities while maintaining full backward compatibility. The abstraction layer ensures reliable operation across different Windows environments while providing enhanced functionality when native WIMGAPI is available.