#ifndef FILE_MONITOR_H
#define FILE_MONITOR_H

#include "hips_core.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <unordered_set>

namespace HIPS {

class FileSystemMonitor {
public:
    FileSystemMonitor();
    ~FileSystemMonitor();

    bool Initialize();
    bool Start();
    bool Stop();
    bool Shutdown();

    // Configuration methods
    void AddWatchPath(const std::string& path);
    void RemoveWatchPath(const std::string& path);
    void SetScanDepth(int depth);
    void SetExcludedExtensions(const std::vector<std::string>& extensions);
    void SetIncludedExtensions(const std::vector<std::string>& extensions);

    // Callback registration
    void RegisterCallback(std::function<void(const SecurityEvent&)> callback);

    // Status
    bool IsRunning() const { return running_.load(); }
    bool IsInitialized() const { return initialized_.load(); }

private:
    struct WatchDirectory {
        std::string path;
        HANDLE handle;
        OVERLAPPED overlapped;
        std::vector<BYTE> buffer;
        bool active;
    };

    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    std::vector<WatchDirectory> watch_dirs_;
    std::thread monitor_thread_;
    std::function<void(const SecurityEvent&)> event_callback_;
    
    // Configuration
    int scan_depth_;
    std::unordered_set<std::string> excluded_extensions_;
    std::unordered_set<std::string> included_extensions_;
    
    // Monitoring methods
    void MonitoringThreadFunction();
    bool SetupDirectoryWatch(WatchDirectory& watch_dir);
    void ProcessFileSystemEvent(const FILE_NOTIFY_INFORMATION* fni, const std::string& directory);
    SecurityEvent CreateSecurityEvent(const std::string& file_path, DWORD action);
    ThreatLevel EvaluateThreatLevel(const std::string& file_path, DWORD action);
    bool IsFileTypeIncluded(const std::string& file_path);
    std::string GetProcessPathFromPID(DWORD pid);
    
    // Helper methods
    std::string GetFileExtension(const std::string& file_path);
    bool IsSystemFile(const std::string& file_path);
    bool IsCriticalDirectory(const std::string& directory);
};

} // namespace HIPS

#endif // FILE_MONITOR_H