#ifndef PROCESS_MONITOR_H
#define PROCESS_MONITOR_H

#include "hips_core.h"
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <vector>

namespace HIPS {

struct ProcessInfo {
    DWORD pid;
    std::string name;
    std::string path;
    std::string command_line;
    DWORD parent_pid;
    SYSTEMTIME creation_time;
    DWORD thread_count;
    SIZE_T memory_usage;
    bool is_system_process;
    ThreatLevel threat_level;
};

class ProcessMonitor {
public:
    ProcessMonitor();
    ~ProcessMonitor();

    bool Initialize();
    bool Start();
    bool Stop();
    bool Shutdown();

    // Configuration
    void SetScanInterval(DWORD interval_ms);
    void AddSuspiciousProcess(const std::string& process_name);
    void RemoveSuspiciousProcess(const std::string& process_name);
    void SetMemoryThreshold(SIZE_T threshold);

    // Callback registration
    void RegisterCallback(std::function<void(const SecurityEvent&)> callback);

    // Process management
    bool TerminateProcess(DWORD pid);
    bool SuspendProcess(DWORD pid);
    bool ResumeProcess(DWORD pid);
    
    // Information gathering
    std::vector<ProcessInfo> GetRunningProcesses();
    ProcessInfo GetProcessInfo(DWORD pid);
    
    // Status
    bool IsRunning() const { return running_.load(); }
    bool IsInitialized() const { return initialized_.load(); }

private:
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    std::thread monitor_thread_;
    std::function<void(const SecurityEvent&)> event_callback_;
    
    // Configuration
    DWORD scan_interval_;
    std::unordered_map<std::string, bool> suspicious_processes_;
    SIZE_T memory_threshold_;
    
    // Process tracking
    std::unordered_map<DWORD, ProcessInfo> known_processes_;
    mutable std::mutex processes_mutex_;
    
    // Monitoring methods
    void MonitoringThreadFunction();
    void ScanForNewProcesses();
    void ScanForTerminatedProcesses();
    void CheckProcessBehavior();
    
    // Process analysis
    ProcessInfo CreateProcessInfo(DWORD pid);
    ThreatLevel EvaluateProcessThreat(const ProcessInfo& process);
    bool IsProcessSuspicious(const ProcessInfo& process);
    bool IsSystemProcess(const ProcessInfo& process);
    
    // Event creation
    SecurityEvent CreateProcessEvent(const ProcessInfo& process, EventType type);
    
    // Helper methods
    std::string GetProcessName(DWORD pid);
    std::string GetProcessPath(DWORD pid);
    std::string GetProcessCommandLine(DWORD pid);
    SIZE_T GetProcessMemoryUsage(DWORD pid);
    DWORD GetProcessThreadCount(DWORD pid);
    DWORD GetParentProcessId(DWORD pid);
    
    // Signature-based detection
    bool CheckProcessSignature(const std::string& process_path);
    bool CheckProcessBehaviorSignatures(const ProcessInfo& process);
};

} // namespace HIPS

#endif // PROCESS_MONITOR_H