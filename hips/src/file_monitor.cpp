#include "file_monitor.h"
#include <iostream>
#include <sstream>
#ifdef _WIN32
#include <psapi.h>
#endif
#include <algorithm>
#include <cctype>

namespace HIPS {

FileSystemMonitor::FileSystemMonitor() 
    : running_(false), initialized_(false), scan_depth_(5) {
    
    // Default excluded extensions (typically safe files)
    excluded_extensions_ = {
        ".log", ".tmp", ".temp", ".bak", ".cache", ".txt", ".doc", ".docx",
        ".pdf", ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".mp3", ".mp4", 
        ".avi", ".mov", ".wav"
    };
}

FileSystemMonitor::~FileSystemMonitor() {
    if (IsRunning()) {
        Stop();
    }
    if (IsInitialized()) {
        Shutdown();
    }
}

bool FileSystemMonitor::Initialize() {
    if (initialized_.load()) {
        return true;
    }

    try {
        // Add default watch paths
        AddWatchPath("C:\\Windows\\System32");
        AddWatchPath("C:\\Windows\\SysWOW64");
        AddWatchPath("C:\\Program Files");
        AddWatchPath("C:\\Program Files (x86)");
        AddWatchPath("C:\\Users");
        
        initialized_.store(true);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool FileSystemMonitor::Start() {
    if (!initialized_.load() || running_.load()) {
        return false;
    }

    try {
        // Setup directory watches
        for (auto& watch_dir : watch_dirs_) {
            if (!SetupDirectoryWatch(watch_dir)) {
                return false;
            }
        }

        running_.store(true);
        monitor_thread_ = std::thread(&FileSystemMonitor::MonitoringThreadFunction, this);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool FileSystemMonitor::Stop() {
    if (!running_.load()) {
        return true;
    }

    running_.store(false);
    
    // Close all directory handles
    for (auto& watch_dir : watch_dirs_) {
        if (watch_dir.handle != INVALID_HANDLE_VALUE) {
            CancelIo(watch_dir.handle);
            CloseHandle(watch_dir.handle);
            watch_dir.handle = INVALID_HANDLE_VALUE;
        }
    }

    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }

    return true;
}

bool FileSystemMonitor::Shutdown() {
    if (running_.load()) {
        Stop();
    }

    watch_dirs_.clear();
    initialized_.store(false);
    return true;
}

void FileSystemMonitor::AddWatchPath(const std::string& path) {
    WatchDirectory watch_dir;
    watch_dir.path = path;
    watch_dir.handle = INVALID_HANDLE_VALUE;
    watch_dir.active = false;
    watch_dir.buffer.resize(64 * 1024); // 64KB buffer
    ZeroMemory(&watch_dir.overlapped, sizeof(OVERLAPPED));
    
    watch_dirs_.push_back(watch_dir);
}

void FileSystemMonitor::RemoveWatchPath(const std::string& path) {
    auto it = std::remove_if(watch_dirs_.begin(), watch_dirs_.end(),
        [&path](const WatchDirectory& wd) {
            return wd.path == path;
        });
    
    if (it != watch_dirs_.end()) {
        if (it->handle != INVALID_HANDLE_VALUE) {
            CloseHandle(it->handle);
        }
        watch_dirs_.erase(it, watch_dirs_.end());
    }
}

bool FileSystemMonitor::SetupDirectoryWatch(WatchDirectory& watch_dir) {
    watch_dir.handle = CreateFileA(
        watch_dir.path.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (watch_dir.handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    watch_dir.overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (watch_dir.overlapped.hEvent == NULL) {
        CloseHandle(watch_dir.handle);
        watch_dir.handle = INVALID_HANDLE_VALUE;
        return false;
    }

    DWORD bytes_returned;
    BOOL result = ReadDirectoryChangesW(
        watch_dir.handle,
        watch_dir.buffer.data(),
        static_cast<DWORD>(watch_dir.buffer.size()),
        TRUE, // Watch subdirectories
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
        FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SECURITY,
        &bytes_returned,
        &watch_dir.overlapped,
        NULL
    );

    if (!result && GetLastError() != ERROR_IO_PENDING) {
        CloseHandle(watch_dir.overlapped.hEvent);
        CloseHandle(watch_dir.handle);
        watch_dir.handle = INVALID_HANDLE_VALUE;
        return false;
    }

    watch_dir.active = true;
    return true;
}

void FileSystemMonitor::MonitoringThreadFunction() {
    std::vector<HANDLE> events;
    
    // Collect all event handles
    for (const auto& watch_dir : watch_dirs_) {
        if (watch_dir.active && watch_dir.overlapped.hEvent) {
            events.push_back(watch_dir.overlapped.hEvent);
        }
    }

    while (running_.load()) {
        if (events.empty()) {
            Sleep(100);
            continue;
        }

        DWORD wait_result = WaitForMultipleObjects(
            static_cast<DWORD>(events.size()),
            events.data(),
            FALSE,
            100 // 100ms timeout
        );

        if (wait_result >= WAIT_OBJECT_0 && wait_result < WAIT_OBJECT_0 + events.size()) {
            size_t index = wait_result - WAIT_OBJECT_0;
            if (index < watch_dirs_.size()) {
                auto& watch_dir = watch_dirs_[index];
                
                DWORD bytes_transferred;
                if (GetOverlappedResult(watch_dir.handle, &watch_dir.overlapped, &bytes_transferred, FALSE)) {
                    // Process the file system events
                    auto* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(watch_dir.buffer.data());
                    ProcessFileSystemEvent(fni, watch_dir.path);
                }

                // Reset the event and start watching again
                ResetEvent(watch_dir.overlapped.hEvent);
                DWORD bytes_returned;
                ReadDirectoryChangesW(
                    watch_dir.handle,
                    watch_dir.buffer.data(),
                    static_cast<DWORD>(watch_dir.buffer.size()),
                    TRUE,
                    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                    FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
                    FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SECURITY,
                    &bytes_returned,
                    &watch_dir.overlapped,
                    NULL
                );
            }
        }
    }
}

void FileSystemMonitor::ProcessFileSystemEvent(const FILE_NOTIFY_INFORMATION* fni, const std::string& directory) {
    const FILE_NOTIFY_INFORMATION* current = fni;
    
    while (current != nullptr) {
        // Convert wide string to narrow string
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, current->FileName, 
            current->FileNameLength / sizeof(WCHAR), NULL, 0, NULL, NULL);
        std::string filename(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, current->FileName, 
            current->FileNameLength / sizeof(WCHAR), &filename[0], size_needed, NULL, NULL);

        std::string full_path = directory + "\\" + filename;

        // Check if this file type should be monitored
        if (IsFileTypeIncluded(full_path)) {
            SecurityEvent event = CreateSecurityEvent(full_path, current->Action);
            if (event_callback_) {
                event_callback_(event);
            }
        }

        // Move to next notification
        if (current->NextEntryOffset == 0) {
            break;
        }
        current = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
            reinterpret_cast<const BYTE*>(current) + current->NextEntryOffset);
    }
}

SecurityEvent FileSystemMonitor::CreateSecurityEvent(const std::string& file_path, DWORD action) {
    SecurityEvent event;
    
    // Determine event type based on action
    switch (action) {
        case FILE_ACTION_ADDED:
        case FILE_ACTION_RENAMED_NEW_NAME:
            event.type = EventType::FILE_ACCESS;
            break;
        case FILE_ACTION_REMOVED:
        case FILE_ACTION_RENAMED_OLD_NAME:
            event.type = EventType::FILE_DELETION;
            break;
        case FILE_ACTION_MODIFIED:
            event.type = EventType::FILE_MODIFICATION;
            break;
        default:
            event.type = EventType::FILE_ACCESS;
            break;
    }

    event.target_path = file_path;
    event.threat_level = EvaluateThreatLevel(file_path, action);
    event.process_id = GetCurrentProcessId(); // This should be enhanced to get actual process
    event.thread_id = GetCurrentThreadId();
    event.process_path = GetProcessPathFromPID(event.process_id);
    
    GetSystemTime(&event.timestamp);
    
    // Add metadata
    event.metadata["action"] = std::to_string(action);
    event.metadata["file_extension"] = GetFileExtension(file_path);
    event.metadata["is_system_file"] = IsSystemFile(file_path) ? "true" : "false";
    
    event.description = "File system activity detected: " + file_path;
    
    return event;
}

ThreatLevel FileSystemMonitor::EvaluateThreatLevel(const std::string& file_path, DWORD action) {
    // Critical system files
    if (IsSystemFile(file_path)) {
        return ThreatLevel::CRITICAL;
    }

    // Check if it's in a critical directory
    if (IsCriticalDirectory(file_path)) {
        return ThreatLevel::HIGH;
    }

    // Executable files
    std::string ext = GetFileExtension(file_path);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".exe" || ext == ".dll" || ext == ".sys" || ext == ".bat" || 
        ext == ".cmd" || ext == ".ps1" || ext == ".vbs" || ext == ".scr") {
        return ThreatLevel::HIGH;
    }

    // File deletion is generally more suspicious
    if (action == FILE_ACTION_REMOVED) {
        return ThreatLevel::MEDIUM;
    }

    return ThreatLevel::LOW;
}

bool FileSystemMonitor::IsFileTypeIncluded(const std::string& file_path) {
    std::string ext = GetFileExtension(file_path);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // If included extensions are specified, only monitor those
    if (!included_extensions_.empty()) {
        return included_extensions_.find(ext) != included_extensions_.end();
    }

    // Otherwise, monitor all except excluded
    return excluded_extensions_.find(ext) == excluded_extensions_.end();
}

std::string FileSystemMonitor::GetProcessPathFromPID(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (process == NULL) {
        return "Unknown";
    }

    char path[MAX_PATH];
    DWORD size = GetModuleFileNameExA(process, NULL, path, MAX_PATH);
    CloseHandle(process);

    if (size > 0) {
        return std::string(path);
    }
    return "Unknown";
}

std::string FileSystemMonitor::GetFileExtension(const std::string& file_path) {
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        return file_path.substr(dot_pos);
    }
    return "";
}

bool FileSystemMonitor::IsSystemFile(const std::string& file_path) {
    std::string upper_path = file_path;
    std::transform(upper_path.begin(), upper_path.end(), upper_path.begin(), ::toupper);
    
    return upper_path.find("C:\\WINDOWS\\SYSTEM32") != std::string::npos ||
           upper_path.find("C:\\WINDOWS\\SYSWOW64") != std::string::npos ||
           upper_path.find("NTOSKRNL.EXE") != std::string::npos ||
           upper_path.find("KERNEL32.DLL") != std::string::npos ||
           upper_path.find("NTDLL.DLL") != std::string::npos;
}

bool FileSystemMonitor::IsCriticalDirectory(const std::string& directory) {
    std::string upper_dir = directory;
    std::transform(upper_dir.begin(), upper_dir.end(), upper_dir.begin(), ::toupper);
    
    return upper_dir.find("C:\\WINDOWS") != std::string::npos ||
           upper_dir.find("C:\\PROGRAM FILES") != std::string::npos;
}

void FileSystemMonitor::RegisterCallback(std::function<void(const SecurityEvent&)> callback) {
    event_callback_ = callback;
}

void FileSystemMonitor::SetScanDepth(int depth) {
    scan_depth_ = depth;
}

void FileSystemMonitor::SetExcludedExtensions(const std::vector<std::string>& extensions) {
    excluded_extensions_.clear();
    for (const auto& ext : extensions) {
        excluded_extensions_.insert(ext);
    }
}

void FileSystemMonitor::SetIncludedExtensions(const std::vector<std::string>& extensions) {
    included_extensions_.clear();
    for (const auto& ext : extensions) {
        included_extensions_.insert(ext);
    }
}

} // namespace HIPS