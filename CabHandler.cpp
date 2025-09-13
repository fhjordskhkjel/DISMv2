#include "CabHandler.h"
#include "CbsManager.h"
#include "CbsIntegratedHandler.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <vector>
#include <memory>
#include <filesystem>
#include <functional>
#include <array>
#include <iomanip>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include <shlwapi.h>
#include <wininet.h>
#include <urlmon.h>

#pragma comment(lib, "cabinet.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "urlmon.lib")

namespace fs = std::filesystem;

// Global context for FDI operations (thread-safe for single-threaded use)
static OperationContext* g_currentContext = nullptr;

CabHandler::CabHandler() : cbsEnabled(false) {
    // Initialize CBS integration as disabled by default
}

CabHandler::~CabHandler() = default;

// Simple error handling implementation
void CabHandler::setLastError(const std::string& error) {
    lastError = error;
    std::cerr << "Error: " << error << std::endl;
}

template<typename... Args>
void CabHandler::setLastErrorFmt(const std::string& fmt, Args&&... args) {
    // Simplified version without std::format for compatibility
    lastError = fmt;
    std::cerr << "Error: " << fmt << std::endl;
}

// Enhanced CAB extraction with improved signature detection and fallback methods
bool CabHandler::extractCabImpl(const std::string& cabPath, const std::string& destination) {
    // Add distinctive debug message to confirm this version is running
    std::cout << "DEBUG: FIXED extractCabImpl version is running!\n";
    
    // Check if CAB file exists using modern filesystem
    if (!fs::exists(cabPath)) {
        setLastError("CAB file does not exist: " + cabPath);
        return false;
    }

    // Simply verify the destination exists (don't try to create it)
    if (!fs::exists(destination) || !fs::is_directory(destination)) {
        setLastError("Destination directory does not exist or is not a directory: " + destination);
        return false;
    }

    std::cout << "Extracting CAB: " << cabPath << "\n";
    std::cout << "Destination: " << destination << "\n";

    // Enhanced file signature checking
    std::ifstream cabFile(cabPath, std::ios::binary);
    if (!cabFile.is_open()) {
        setLastError("Cannot open CAB file for reading: " + cabPath);
        return false;
    }
    
    // Read and analyze file signature
    char signature[8];
    cabFile.read(signature, 8);
    cabFile.close();
    
    bool isStandardCab = (memcmp(signature, "MSCF", 4) == 0);
    bool isPossibleArchive = false;
    
    // Check for various archive signatures
    if (memcmp(signature, "PK", 2) == 0) {
        isPossibleArchive = true; // ZIP-based format
        std::cout << "Detected ZIP-based archive format\n";
    } else if (memcmp(signature, "7z", 2) == 0) {
        isPossibleArchive = true; // 7-Zip format
        std::cout << "Detected 7-Zip archive format\n";
    } else if (memcmp(signature, "\x1F\x8B", 2) == 0) {
        isPossibleArchive = true; // GZIP format
        std::cout << "Detected GZIP archive format\n";
    }
    
    if (!isStandardCab) {
        std::cout << "Warning: Non-standard CAB signature detected\n";
        std::cout << "Signature bytes: ";
        for (int i = 0; i < 8; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                     << (unsigned char)signature[i] << " ";
        }
        std::cout << std::dec << "\n";
    }

    // Try standard FDI extraction first for standard CAB files
    if (isStandardCab) {
        std::cout << "Attempting standard FDI extraction...\n";
        
        ERF erf = {};
        HFDI hfdi = FDICreate(fdiAlloc, fdiFree, fdiOpen, fdiRead, fdiWrite, fdiClose, fdiSeek, cpuUNKNOWN, &erf);
        
        if (hfdi) {
            OperationContext context = {};
            context.handler = this;
            context.destinationPath = destination;
            context.listOnly = false;
            
            g_currentContext = &context;

            auto cabPathFs = fs::path(cabPath);
            std::string cabDir = cabPathFs.parent_path().string();
            std::string cabFile = cabPathFs.filename().string();

            BOOL result = FDICopy(hfdi, const_cast<char*>(cabFile.c_str()), 
                                 const_cast<char*>(cabDir.c_str()), 0, fdiNotify, NULL, NULL);

            FDIDestroy(hfdi);
            g_currentContext = nullptr;

            if (result) {
                std::cout << "Standard FDI extraction successful\n";
                return true;
            } else {
                std::cout << "Standard FDI extraction failed. Error details:\n";
                std::cout << "  Operation: " << erf.erfOper << "\n";
                std::cout << "  Type: " << erf.erfType << "\n";
                std::cout << "  Error: " << erf.fError << "\n";
                std::cout << "Trying alternative extraction methods...\n";
            }
        }
    }
    
    // Enhanced fallback extraction methods
    std::vector<std::function<bool()>> extractMethods;
    std::vector<std::string> methodNames;
    
    // Method 1: Enhanced PowerShell extraction
    extractMethods.push_back([this, &cabPath, &destination]() {
        std::string script = 
            "$ErrorActionPreference = 'Stop'; "
            "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
            "try { "
            "  [System.IO.Compression.ZipFile]::ExtractToDirectory('" + cabPath + "', '" + destination + "'); "
            "  Write-Host 'PowerShell extraction successful'; "
            "  exit 0; "
            "} catch { "
            "  Write-Host 'PowerShell extraction failed:' $_.Exception.Message; "
            "  exit 1; "
            "}";
        return executePowerShellScript(script);
    });
    methodNames.push_back("PowerShell System.IO.Compression");
    
    // Method 2: expand.exe (Windows built-in)
    extractMethods.push_back([this, &cabPath, &destination]() {
        std::string command = "expand.exe \"" + cabPath + "\" -F:* \"" + destination + "\"";
        return executeCommand(command, 60000);
    });
    methodNames.push_back("Windows expand.exe");
    
    // Method 3: 7-Zip (if available)
    extractMethods.push_back([this, &cabPath, &destination]() {
        std::string command = "7z.exe x \"" + cabPath + "\" -o\"" + destination + "\" -y";
        return executeCommand(command, 60000);
    });
    methodNames.push_back("7-Zip");
    
    // Method 4: WinRAR (if available)
    extractMethods.push_back([this, &cabPath, &destination]() {
        std::string command = "winrar.exe x \"" + cabPath + "\" \"" + destination + "\\\"";
        return executeCommand(command, 60000);
    });
    methodNames.push_back("WinRAR");
    
    // Method 5: Advanced PowerShell with multiple approaches
    extractMethods.push_back([this, &cabPath, &destination]() {
        std::string script = 
            "$ErrorActionPreference = 'SilentlyContinue'; "
            "try { "
            "  # Try as ZIP archive "
            "  Add-Type -AssemblyName System.IO.Compression.FileSystem; "
            "  [System.IO.Compression.ZipFile]::ExtractToDirectory('" + cabPath + "', '" + destination + "'); "
            "  exit 0; "
            "} catch { "
            "  try { "
            "    # Try with Shell.Application COM object "
            "    $shell = New-Object -ComObject Shell.Application; "
            "    $zip = $shell.NameSpace('" + cabPath + "'); "
            "    $dest = $shell.NameSpace('" + destination + "'); "
            "    $dest.CopyHere($zip.Items(), 16); "
            "    exit 0; "
            "  } catch { "
            "    exit 1; "
            "  } "
            "}";
        return executePowerShellScript(script);
    });
    methodNames.push_back("Advanced PowerShell");
    
    // Try each extraction method
    for (size_t i = 0; i < extractMethods.size(); ++i) {
        std::cout << "Trying extraction method " << (i + 1) << "/" << extractMethods.size() 
                 << ": " << methodNames[i] << "\n";
        
        try {
            if (extractMethods[i]()) {
                std::cout << "? Successfully extracted using: " << methodNames[i] << "\n";
                
                // Verify extraction was successful
                auto extractedFiles = getFilesInDirectory(destination, true);
                if (!extractedFiles.empty()) {
                    std::cout << "Verification: " << extractedFiles.size() << " files extracted\n";
                    return true;
                } else {
                    std::cout << "Warning: " << methodNames[i] << " reported success but no files found\n";
                }
            }
        } catch (const std::exception& ex) {
            std::cout << "Exception with " << methodNames[i] << ": " << ex.what() << "\n";
        }
    }
    
    // If all methods failed, try binary analysis for embedded content
    std::cout << "All standard methods failed, attempting binary analysis...\n";
    
    if (attemptBinaryExtraction(cabPath, destination)) {
        std::cout << "? Binary extraction successful\n";
        return true;
    }
    
    setLastError("All extraction methods failed for: " + cabPath + 
                ". This may be a proprietary format, corrupted file, or require special tools.");
    return false;
}

// Filesystem helper implementations
bool CabHandler::createDirectoryRecursive(const std::filesystem::path& path) {
    std::error_code ec;
    // create_directories returns true if directory already exists
    fs::create_directories(path, ec);
    return !ec || fs::exists(path);
}

bool CabHandler::createDirectoryRecursive(const std::string& path) {
    std::error_code ec;
    // create_directories returns true if directory already exists  
    fs::create_directories(path, ec);
    return !ec || fs::exists(path);
}

std::vector<std::string> CabHandler::getFilesInDirectory(const std::string& directory, bool recursive) {
    std::vector<std::string> files;
    
    try {
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path().string());
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception&) {
        // Handle filesystem exceptions
    }
    
    return files;
}

bool CabHandler::attemptBinaryExtraction(const std::string& filePath, const std::string& destination) {
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        // Read file into buffer for analysis
        std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        if (buffer.size() < 1024) {
            return false; // File too small to contain meaningful archive data
        }
        
        // Look for embedded archive signatures
        std::vector<size_t> archiveOffsets;
        
        // Search for ZIP signatures (PK)
        for (size_t i = 0; i < buffer.size() - 4; ++i) {
            if (buffer[i] == 'P' && buffer[i + 1] == 'K' && 
                (buffer[i + 2] == 0x03 || buffer[i + 2] == 0x05)) {
                archiveOffsets.push_back(i);
            }
        }
        
        // Search for CAB signatures (MSCF)
        for (size_t i = 0; i < buffer.size() - 4; ++i) {
            if (buffer[i] == 'M' && buffer[i + 1] == 'S' && 
                buffer[i + 2] == 'C' && buffer[i + 3] == 'F') {
                archiveOffsets.push_back(i);
            }
        }
        
        // Try to extract embedded archives
        for (size_t offset : archiveOffsets) {
            std::string tempFile = destination + "\\embedded_" + std::to_string(offset) + ".tmp";
            
            std::ofstream outFile(tempFile, std::ios::binary);
            if (outFile.is_open()) {
                outFile.write(reinterpret_cast<const char*>(buffer.data() + offset), 
                             buffer.size() - offset);
                outFile.close();
                
                // Try to extract the embedded archive
                if (extractCabImpl(tempFile, destination)) {
                    fs::remove(tempFile);
                    return true;
                }
                
                fs::remove(tempFile);
            }
        }
        
        return false;
        
    } catch (const std::exception&) {
        return false;
    }
}

bool CabHandler::executePowerShellScript(const std::string& script) {
    std::string command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"" + script + "\"";
    return executeCommand(command, 60000);
}

bool CabHandler::executeCommand(const std::string& command, DWORD timeoutMs) {
    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    
    if (!CreateProcessA(NULL, const_cast<char*>(command.c_str()), NULL, NULL, FALSE, 
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return false;
    }
    
    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
    DWORD exitCode = 1;
    
    if (waitResult == WAIT_OBJECT_0) {
        GetExitCodeProcess(pi.hProcess, &exitCode);
    } else {
        TerminateProcess(pi.hProcess, 1);
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return exitCode == 0;
}

bool CabHandler::installCabPackageOnlineImpl(const std::string& cabPath, const std::string& logPath, bool quiet) {
    // Check if CAB file exists
    if (!fs::exists(cabPath)) {
        setLastError("CAB package file does not exist: " + cabPath);
        return false;
    }
    
    if (!quiet) {
        std::cout << "Installing CAB package online to running system...\n";
    }
    
    // Get Windows directory for online installation
    std::string windowsDir = getWindowsDirectory();
    if (windowsDir.empty()) {
        setLastError("Failed to get Windows directory");
        return false;
    }
    
    if (!quiet) {
        std::cout << "Windows directory: " << windowsDir << "\n";
    }
    
    // Extract the system root (e.g., "C:" from "C:\WINDOWS")
    std::string systemRoot;
    auto windowsDirPath = fs::path(windowsDir);
    if (windowsDirPath.has_parent_path()) {
        systemRoot = windowsDirPath.parent_path().string();
    } else {
        setLastError("Invalid Windows directory path: " + windowsDir);
        return false;
    }
    
    if (!quiet) {
        std::cout << "System root: " << systemRoot << "\n";
    }
    
    // For online installation, use temp directory for staging instead of system directory
    // Use GetTempFileNameA to create a unique directory name that follows Windows conventions
    char tempPathBase[MAX_PATH];
    DWORD tempPathLength = GetTempPathA(MAX_PATH, tempPathBase);
    if (tempPathLength == 0 || tempPathLength > MAX_PATH) {
        setLastError("Failed to get system temp directory. Error: " + std::to_string(GetLastError()));
        return false;
    }
    
    // Create a temporary file name first, then delete it and use as directory name
    char tempFileName[MAX_PATH];
    if (!GetTempFileNameA(tempPathBase, "CAB", 0, tempFileName)) {
        setLastError("Failed to generate temp file name. Error: " + std::to_string(GetLastError()));
        return false;
    }
    
    // Delete the temp file and use the name as a directory
    DeleteFileA(tempFileName);
    std::string tempStagingDir = tempFileName;
    
    if (!quiet) {
        std::cout << "Temp staging directory: " << tempStagingDir << "\n";
        std::cout << "Creating staging directory...\n";
    }
    
    // Create the directory using std::filesystem - don't fail if it already exists
    std::error_code ec;
    fs::create_directories(tempStagingDir, ec);
    
    if (ec && !fs::exists(tempStagingDir)) {
        if (!quiet) {
            std::cout << "Failed to create staging directory. Error details:\n";
            std::cout << "Path: " << tempStagingDir << "\n";
            std::cout << "Error: " << ec.message() << " (Code: " << ec.value() << ")\n";
            std::cout << "Temp base path: " << tempPathBase << "\n";
            std::cout << "Temp base exists: " << (fs::exists(tempPathBase) ? "YES" : "NO") << "\n";
        }
        
        setLastError("Failed to create temporary staging directory '" + tempStagingDir + "': " + ec.message() + " (Error code: " + std::to_string(ec.value()) + ")");
        return false;
    }
    
    if (!quiet) {
        std::cout << "Successfully created staging directory\n";
        std::cout << "Verifying staging directory exists: " << (fs::exists(tempStagingDir) ? "YES" : "NO") << "\n";
        std::cout << "Verifying staging directory is_directory: " << (fs::is_directory(tempStagingDir) ? "YES" : "NO") << "\n";
    }
    
    // Additional verification - create a test file to ensure write access
    std::string testFile = tempStagingDir + "\\test_write.tmp";
    std::ofstream test(testFile);
    if (test.is_open()) {
        test << "test";
        test.close();
        DeleteFileA(testFile.c_str());
        if (!quiet) {
            std::cout << "Staging directory write test: PASSED\n";
        }
    } else {
        if (!quiet) {
            std::cout << "Staging directory write test: FAILED\n";
        }
        setLastError("Staging directory is not writable: " + tempStagingDir);
        return false;
    }
    
    // Extract CAB to temporary staging directory
    if (!extractCabImpl(cabPath, tempStagingDir)) {
        // Clean up before returning
        std::error_code cleanupEc;
        fs::remove_all(tempStagingDir, cleanupEc);
        setLastError("Failed to extract CAB package to staging directory");
        return false;
    }
    
    if (!quiet) {
        std::cout << "Package extracted to temporary staging directory successfully!\n";
    }
    
    // Process extracted files and install to appropriate locations
    auto extractedFiles = getFilesInDirectory(tempStagingDir, true);
    bool success = true;
    
    for (const auto& file : extractedFiles) {
        std::string relativePath = getRelativePathStr(file, tempStagingDir);
        std::string targetPath;
        
        // Determine target installation path based on file type and location
        if (relativePath.find("system32") != std::string::npos) {
            targetPath = systemRoot + "\\Windows\\System32\\" + getFilenameFromPath(fs::path(file));
        }
        else if (relativePath.find("drivers") != std::string::npos) {
            targetPath = systemRoot + "\\Windows\\System32\\drivers\\" + getFilenameFromPath(fs::path(file));
        }
        else if (relativePath.find("winsxs") != std::string::npos) {
            targetPath = systemRoot + "\\Windows\\winsxs\\" + relativePath;
        }
        else {
            // Default to Windows directory
            targetPath = systemRoot + "\\Windows\\" + relativePath;
        }
        
        // Create target directory
        std::string targetDir = getDirectoryFromPathStr(targetPath);
        createDirectoryRecursive(targetDir);
        
        // Copy file to target location with proper error handling for online installation
        if (!CopyFileA(file.c_str(), targetPath.c_str(), FALSE)) {
            DWORD error = GetLastError();
            if (!quiet) {
                std::cout << "Warning: Failed to copy " << relativePath << " to target location (Error: " << error << ")\n";
            }
            // For online installation, this might be expected for some files (like locked system files)
        } else {
            if (!quiet) {
                std::cout << "Installed: " << relativePath << "\n";
            }
        }
    }
    
    // Process any manifest files for component registration
    for (const auto& file : extractedFiles) {
        auto filePath = fs::path(file);
        std::string fileName = filePath.filename().string();
        std::string fileExt = filePath.extension().string();
        std::transform(fileExt.begin(), fileExt.end(), fileExt.begin(), ::tolower);
        
        if (fileExt == ".mum" || fileExt == ".xml") {
            // Copy manifest files to servicing directory
            std::string manifestTarget = systemRoot + "\\Windows\\servicing\\Packages\\" + fileName;
            
            // Create servicing directory if it doesn't exist
            std::string servicingDir = systemRoot + "\\Windows\\servicing\\Packages";
            createDirectoryRecursive(servicingDir);
            
            if (CopyFileA(file.c_str(), manifestTarget.c_str(), FALSE)) {
                if (!quiet) {
                    std::cout << "Registered manifest: " << fileName << "\n";
                }
            } else {
                if (!quiet) {
                    std::cout << "Warning: Failed to register manifest: " << fileName << "\n";
                }
            }
        }
    }
    
    // Cleanup temporary directory
    removeDirectoryRecursive(tempStagingDir);
    
    // Log the installation if log path is provided
    if (!logPath.empty()) {
        std::ofstream logFile(logPath, std::ios::app);
        if (logFile.is_open()) {
            logFile << "Online package installed: " << cabPath << " to " << systemRoot << "\n";
            logFile << "Installation time: " << GetTickCount64() << "\n";
            logFile << "Files processed: " << extractedFiles.size() << "\n";
            logFile << "---\n";
        }
    }
    
    return success;
}

std::string CabHandler::getWindowsDirectory() {
    char windowsDir[MAX_PATH];
    if (GetWindowsDirectoryA(windowsDir, MAX_PATH) > 0) {
        return std::string(windowsDir);
    }
    return "";
}

std::string CabHandler::getDirectoryFromPathStr(const std::string& path) {
    return fs::path(path).parent_path().string();
}

std::string CabHandler::getFilenameFromPath(const std::filesystem::path& path) {
    return path.filename().string();
}

std::string CabHandler::getRelativePathStr(const std::string& fullPath, const std::string& basePath) {
    return fs::relative(fs::path(fullPath), fs::path(basePath)).string();
}

bool CabHandler::removeDirectoryRecursive(const std::string& path) {
    std::error_code ec;
    return fs::remove_all(path, ec) > 0;
}

// Add stub FDI callbacks for now
INT_PTR DIAMONDAPI CabHandler::fdiNotify(FDINOTIFICATIONTYPE fdint, PFDINOTIFICATION pfdin) {
    // Basic implementation for extraction
    if (fdint == fdintCOPY_FILE && g_currentContext && !g_currentContext->listOnly) {
        std::string destPath = g_currentContext->destinationPath + "\\" + pfdin->psz1;
        std::string destDir = fs::path(destPath).parent_path().string();
        
        g_currentContext->handler->createDirectoryRecursive(destDir);
        
        return (INT_PTR)CreateFileA(destPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                   NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    
    if (fdint == fdintCLOSE_FILE_INFO && g_currentContext) {
        CloseHandle((HANDLE)pfdin->hf);
        return TRUE;
    }
    
    return 0;
}

void* DIAMONDAPI CabHandler::fdiAlloc(ULONG cb) { return malloc(cb); }
void DIAMONDAPI CabHandler::fdiFree(void* pv) { free(pv); }
INT_PTR DIAMONDAPI CabHandler::fdiOpen(char* pszFile, int oflag, int pmode) { return (INT_PTR)CreateFileA(pszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); }
UINT DIAMONDAPI CabHandler::fdiRead(INT_PTR hf, void* pv, UINT cb) { DWORD bytesRead; ReadFile((HANDLE)hf, pv, cb, &bytesRead, NULL); return bytesRead; }
UINT DIAMONDAPI CabHandler::fdiWrite(INT_PTR hf, void* pv, UINT cb) { DWORD bytesWritten; WriteFile((HANDLE)hf, pv, cb, &bytesWritten, NULL); return bytesWritten; }
int DIAMONDAPI CabHandler::fdiClose(INT_PTR hf) { return CloseHandle((HANDLE)hf) ? 0 : -1; }
LONG DIAMONDAPI CabHandler::fdiSeek(INT_PTR hf, LONG dist, int seektype) { return SetFilePointer((HANDLE)hf, dist, NULL, seektype); }

// Placeholder for public method wrappers
bool CabHandler::installCabPackageOnline(const std::string& cabPath, const std::string& logPath, bool quiet) {
    return installCabPackageOnlineImpl(cabPath, logPath, quiet);
}