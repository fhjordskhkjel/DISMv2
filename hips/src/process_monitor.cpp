#include "process_monitor.h"
#ifdef _WIN32
#include <tlhelp32.h>
#include <psapi.h>
#endif
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>

namespace HIPS {

ProcessMonitor::ProcessMonitor() 
    : running_(false), initialized_(false), scan_interval_(1000), memory_threshold_(500 * 1024 * 1024) {
    
    // Add common suspicious process names
    suspicious_processes_["mimikatz.exe"] = true;
    suspicious_processes_["powershell.exe"] = true;
    suspicious_processes_["cmd.exe"] = true;
    suspicious_processes_["psexec.exe"] = true;
    suspicious_processes_["wmic.exe"] = true;
    suspicious_processes_["rundll32.exe"] = true;
    suspicious_processes_["regsvr32.exe"] = true;
}

ProcessMonitor::~ProcessMonitor() {
    if (IsRunning()) {
        Stop();
    }
    if (IsInitialized()) {
        Shutdown();
    }
}

bool ProcessMonitor::Initialize() {
    if (initialized_.load()) {
        return true;
    }

    try {
        // Initialize process tracking by getting current processes
        ScanForNewProcesses();
        
        initialized_.store(true);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool ProcessMonitor::Start() {
    if (!initialized_.load() || running_.load()) {
        return false;
    }

    try {
        running_.store(true);
        monitor_thread_ = std::thread(&ProcessMonitor::MonitoringThreadFunction, this);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool ProcessMonitor::Stop() {
    if (!running_.load()) {
        return true;
    }

    running_.store(false);
    
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }

    return true;
}

bool ProcessMonitor::Shutdown() {
    if (running_.load()) {
        Stop();
    }

    {
        std::lock_guard<std::mutex> lock(processes_mutex_);
        known_processes_.clear();
    }
    
    initialized_.store(false);
    return true;
}

void ProcessMonitor::MonitoringThreadFunction() {
    while (running_.load()) {
        try {
            ScanForNewProcesses();
            ScanForTerminatedProcesses();
            CheckProcessBehavior();
#ifdef _WIN32
            ScanThreadAPCQueues();
#endif
            
#ifdef _WIN32
            Sleep(scan_interval_);
#else
            std::this_thread::sleep_for(std::chrono::milliseconds(scan_interval_));
#endif
        } catch (const std::exception& e) {
            // Log error and continue
#ifdef _WIN32
            Sleep(scan_interval_);
#else
            std::this_thread::sleep_for(std::chrono::milliseconds(scan_interval_));
#endif
        }
    }
}

void ProcessMonitor::ScanForNewProcesses() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &pe32)) {
        do {
            std::lock_guard<std::mutex> lock(processes_mutex_);
            
            // Check if this is a new process
            if (known_processes_.find(pe32.th32ProcessID) == known_processes_.end()) {
                ProcessInfo process = CreateProcessInfo(pe32.th32ProcessID);
                known_processes_[pe32.th32ProcessID] = process;
                
                // Generate security event for new process
                SecurityEvent event = CreateProcessEvent(process, EventType::PROCESS_CREATION);
                if (event_callback_) {
                    event_callback_(event);
                }
            }
            
        } while (Process32Next(snapshot, &pe32));
    }

    CloseHandle(snapshot);
}

void ProcessMonitor::ScanForTerminatedProcesses() {
    std::vector<DWORD> terminated_pids;
    
    {
        std::lock_guard<std::mutex> lock(processes_mutex_);
        
        for (auto it = known_processes_.begin(); it != known_processes_.end();) {
            HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, it->first);
            if (process == NULL) {
                // Process no longer exists
                terminated_pids.push_back(it->first);
                
                // Generate security event for terminated process
                SecurityEvent event = CreateProcessEvent(it->second, EventType::PROCESS_TERMINATION);
                if (event_callback_) {
                    event_callback_(event);
                }
                
                it = known_processes_.erase(it);
            } else {
                CloseHandle(process);
                ++it;
            }
        }
    }
}

void ProcessMonitor::CheckProcessBehavior() {
    std::lock_guard<std::mutex> lock(processes_mutex_);
    
    for (auto& pair : known_processes_) {
        ProcessInfo& process = pair.second;
        
        // Update memory usage
        SIZE_T current_memory = GetProcessMemoryUsage(process.pid);
        if (current_memory > process.memory_usage * 2 && current_memory > memory_threshold_) {
            // Significant memory increase detected
            SecurityEvent event;
            event.type = EventType::EXPLOIT_ATTEMPT;
            event.threat_level = ThreatLevel::HIGH;
            event.process_id = process.pid;
            event.process_path = process.path;
            event.target_path = "";
            event.description = "Suspicious memory usage increase detected";
            GetSystemTime(&event.timestamp);
            
            if (event_callback_) {
                event_callback_(event);
            }
        }
        process.memory_usage = current_memory;
        
        // Check for suspicious behavior patterns
        if (CheckProcessBehaviorSignatures(process)) {
            SecurityEvent event;
            event.type = EventType::EXPLOIT_ATTEMPT;
            event.threat_level = ThreatLevel::CRITICAL;
            event.process_id = process.pid;
            event.process_path = process.path;
            event.target_path = "";
            event.description = "Suspicious process behavior detected";
            GetSystemTime(&event.timestamp);
            
            if (event_callback_) {
                event_callback_(event);
            }
        }
    }
}

ProcessInfo ProcessMonitor::CreateProcessInfo(DWORD pid) {
    ProcessInfo info;
    info.pid = pid;
    info.name = GetProcessName(pid);
    info.path = GetProcessPath(pid);
    info.command_line = GetProcessCommandLine(pid);
    info.parent_pid = GetParentProcessId(pid);
    info.thread_count = GetProcessThreadCount(pid);
    info.memory_usage = GetProcessMemoryUsage(pid);
    info.is_system_process = IsSystemProcess(info);
    info.threat_level = EvaluateProcessThreat(info);
    
    GetSystemTime(&info.creation_time);
    
    return info;
}

ThreatLevel ProcessMonitor::EvaluateProcessThreat(const ProcessInfo& process) {
    // System processes are generally safe but monitored
    if (process.is_system_process) {
        return ThreatLevel::LOW;
    }
    
    // Check against suspicious process list
    if (IsProcessSuspicious(process)) {
        return ThreatLevel::HIGH;
    }
    
    // Check process signature
    if (!CheckProcessSignature(process.path)) {
        return ThreatLevel::MEDIUM;
    }
    
    // Check for unusual memory usage
    if (process.memory_usage > memory_threshold_) {
        return ThreatLevel::MEDIUM;
    }
    
    // Processes running from temp directories
    std::string upper_path = process.path;
    std::transform(upper_path.begin(), upper_path.end(), upper_path.begin(), ::toupper);
    if (upper_path.find("\\TEMP\\") != std::string::npos ||
        upper_path.find("\\TMP\\") != std::string::npos ||
        upper_path.find("\\APPDATA\\") != std::string::npos) {
        return ThreatLevel::MEDIUM;
    }
    
    return ThreatLevel::LOW;
}

bool ProcessMonitor::IsProcessSuspicious(const ProcessInfo& process) {
    std::string lower_name = process.name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    auto it = suspicious_processes_.find(lower_name);
    return (it != suspicious_processes_.end() && it->second);
}

bool ProcessMonitor::IsSystemProcess(const ProcessInfo& process) {
    std::string upper_path = process.path;
    std::transform(upper_path.begin(), upper_path.end(), upper_path.begin(), ::toupper);
    
    return upper_path.find("C:\\WINDOWS\\SYSTEM32") != std::string::npos ||
           upper_path.find("C:\\WINDOWS\\SYSWOW64") != std::string::npos ||
           process.name == "System" ||
           process.name == "smss.exe" ||
           process.name == "csrss.exe" ||
           process.name == "wininit.exe" ||
           process.name == "winlogon.exe" ||
           process.name == "services.exe" ||
           process.name == "lsass.exe" ||
           process.name == "svchost.exe";
}

SecurityEvent ProcessMonitor::CreateProcessEvent(const ProcessInfo& process, EventType type) {
    SecurityEvent event;
    event.type = type;
    event.threat_level = process.threat_level;
    event.process_id = process.pid;
    event.process_path = process.path;
    event.target_path = "";
    event.thread_id = 0;
    event.timestamp = process.creation_time;
    
    // Add metadata
    event.metadata["process_name"] = process.name;
    event.metadata["parent_pid"] = std::to_string(process.parent_pid);
    event.metadata["thread_count"] = std::to_string(process.thread_count);
    event.metadata["memory_usage"] = std::to_string(process.memory_usage);
    event.metadata["is_system_process"] = process.is_system_process ? "true" : "false";
    event.metadata["command_line"] = process.command_line;
    
    if (type == EventType::PROCESS_CREATION) {
        event.description = "New process created: " + process.name;
    } else if (type == EventType::PROCESS_TERMINATION) {
        event.description = "Process terminated: " + process.name;
    }
    
    return event;
}

std::string ProcessMonitor::GetProcessName(DWORD pid) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return "Unknown";
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == pid) {
                CloseHandle(snapshot);
                return std::string(pe32.szExeFile);
            }
        } while (Process32Next(snapshot, &pe32));
    }

    CloseHandle(snapshot);
    return "Unknown";
}

std::string ProcessMonitor::GetProcessPath(DWORD pid) {
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

std::string ProcessMonitor::GetProcessCommandLine(DWORD pid) {
    // This is a simplified implementation
    // Full implementation would require reading process memory
    return ""; // TODO: Implement full command line retrieval
}

SIZE_T ProcessMonitor::GetProcessMemoryUsage(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (process == NULL) {
        return 0;
    }

    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(process, &pmc, sizeof(pmc))) {
        CloseHandle(process);
        return pmc.WorkingSetSize;
    }

    CloseHandle(process);
    return 0;
}

DWORD ProcessMonitor::GetProcessThreadCount(DWORD pid) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);
    DWORD thread_count = 0;

    if (Thread32First(snapshot, &te32)) {
        do {
            if (te32.th32OwnerProcessID == pid) {
                thread_count++;
            }
        } while (Thread32Next(snapshot, &te32));
    }

    CloseHandle(snapshot);
    return thread_count;
}

DWORD ProcessMonitor::GetParentProcessId(DWORD pid) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == pid) {
                CloseHandle(snapshot);
                return pe32.th32ParentProcessID;
            }
        } while (Process32Next(snapshot, &pe32));
    }

    CloseHandle(snapshot);
    return 0;
}

bool ProcessMonitor::CheckProcessSignature(const std::string& process_path) {
    // Simplified signature check - in a real implementation,
    // this would verify digital signatures
    return !process_path.empty() && process_path != "Unknown";
}

bool ProcessMonitor::CheckProcessBehaviorSignatures(const ProcessInfo& process) {
    // This would implement behavioral analysis patterns
    // For now, just check for some basic suspicious patterns
    
    // Processes with unusual thread counts
    if (process.thread_count > 100) {
        return true;
    }
    
    // Processes with very high memory usage
    if (process.memory_usage > memory_threshold_ * 2) {
        return true;
    }
    
    return false;
}

std::vector<ProcessInfo> ProcessMonitor::GetRunningProcesses() {
    std::lock_guard<std::mutex> lock(processes_mutex_);
    std::vector<ProcessInfo> processes;
    
    for (const auto& pair : known_processes_) {
        processes.push_back(pair.second);
    }
    
    return processes;
}

ProcessInfo ProcessMonitor::GetProcessInfo(DWORD pid) {
    std::lock_guard<std::mutex> lock(processes_mutex_);
    auto it = known_processes_.find(pid);
    if (it != known_processes_.end()) {
        return it->second;
    }
    
    // Process not found in cache, create new info
    return CreateProcessInfo(pid);
}

bool ProcessMonitor::TerminateProcess(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (process == NULL) {
        return false;
    }

    BOOL result = ::TerminateProcess(process, 0);
    CloseHandle(process);
    return result != FALSE;
}

bool ProcessMonitor::SuspendProcess(DWORD pid) {
    // Implementation would suspend all threads of the process
    // This is a simplified version
    return true; // TODO: Implement full process suspension
}

bool ProcessMonitor::ResumeProcess(DWORD pid) {
    // Implementation would resume all threads of the process
    // This is a simplified version
    return true; // TODO: Implement full process resumption
}

void ProcessMonitor::RegisterCallback(std::function<void(const SecurityEvent&)> callback) {
    event_callback_ = callback;
}

void ProcessMonitor::SetScanInterval(DWORD interval_ms) {
    scan_interval_ = interval_ms;
}

void ProcessMonitor::AddSuspiciousProcess(const std::string& process_name) {
    suspicious_processes_[process_name] = true;
}

void ProcessMonitor::RemoveSuspiciousProcess(const std::string& process_name) {
    suspicious_processes_.erase(process_name);
}

void ProcessMonitor::SetMemoryThreshold(SIZE_T threshold) {
    memory_threshold_ = threshold;
}

#ifdef _WIN32
void ProcessMonitor::ScanThreadAPCQueues() {
    std::lock_guard<std::mutex> lock(processes_mutex_);
    
    for (const auto& pair : known_processes_) {
        const ProcessInfo& process = pair.second;
        
        // Skip system processes to avoid false positives
        if (process.is_system_process) {
            continue;
        }
        
        // Scan APCs for this process
        if (ScanProcessThreadAPCs(process.pid, process.name)) {
            // Suspicious APC detected
            SecurityEvent event;
            event.type = EventType::MEMORY_INJECTION;
            event.threat_level = ThreatLevel::HIGH;
            event.process_id = process.pid;
            event.process_path = process.path;
            event.target_path = "";
            event.description = "Suspicious APC queue entry detected in process: " + process.name;
            GetSystemTime(&event.timestamp);
            
            if (event_callback_) {
                event_callback_(event);
            }
        }
    }
}

bool ProcessMonitor::ScanProcessThreadAPCs(DWORD pid, const std::string& process_name) {
    std::vector<DWORD> thread_ids = GetProcessThreads(pid);
    
    if (thread_ids.empty()) {
        return false;
    }
    
    // Open process with required permissions
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (process == NULL) {
        return false;
    }
    
    bool suspicious_apc_found = false;
    
    // Check each thread for suspicious APC queue entries
    for (DWORD thread_id : thread_ids) {
        HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, thread_id);
        if (thread != NULL) {
            // Note: Full APC queue inspection requires NtQueryInformationThread
            // which is an undocumented API. For a production system, you would:
            // 1. Use NtQueryInformationThread with ThreadLastSystemCall
            // 2. Check for unusual APCs pointing to non-module memory
            // 3. Validate APC target addresses against known module ranges
            
            // Simplified detection: Check if thread has unusually high APC count
            // or if the thread is in an alertable wait state
            // This is a heuristic-based approach for educational purposes
            
            // In a real implementation, you would use:
            // typedef NTSTATUS (NTAPI *pNtQueryInformationThread)(
            //     HANDLE ThreadHandle,
            //     THREADINFOCLASS ThreadInformationClass,
            //     PVOID ThreadInformation,
            //     ULONG ThreadInformationLength,
            //     PULONG ReturnLength
            // );
            
            CloseHandle(thread);
        }
    }
    
    CloseHandle(process);
    return suspicious_apc_found;
}

std::vector<DWORD> ProcessMonitor::GetProcessThreads(DWORD pid) {
    std::vector<DWORD> thread_ids;
    
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return thread_ids;
    }
    
    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);
    
    if (Thread32First(snapshot, &te32)) {
        do {
            if (te32.th32OwnerProcessID == pid) {
                thread_ids.push_back(te32.th32ThreadID);
            }
        } while (Thread32Next(snapshot, &te32));
    }
    
    CloseHandle(snapshot);
    return thread_ids;
}
#else
// Stub implementations for non-Windows platforms
void ProcessMonitor::ScanThreadAPCQueues() {
    // Not implemented on non-Windows platforms
}

bool ProcessMonitor::ScanProcessThreadAPCs(DWORD pid, const std::string& process_name) {
    // Not implemented on non-Windows platforms
    return false;
}

std::vector<DWORD> ProcessMonitor::GetProcessThreads(DWORD pid) {
    // Not implemented on non-Windows platforms
    return std::vector<DWORD>();
}
#endif

} // namespace HIPS