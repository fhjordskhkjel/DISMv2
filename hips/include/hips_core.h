#ifndef HIPS_CORE_H
#define HIPS_CORE_H

#ifdef _WIN32
#include <windows.h>
#else
// Cross-platform compatibility definitions for development
#include <chrono>
#include <ctime>
#define DWORD unsigned long
#define BOOL int
#define TRUE 1
#define FALSE 0
#define BYTE unsigned char
#define SIZE_T size_t
#define HANDLE void*
#define HKEY void*
typedef struct {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME;
typedef struct {
    unsigned short wYear;
    unsigned short wMonth;
    unsigned short wDayOfWeek;
    unsigned short wDay;
    unsigned short wHour;
    unsigned short wMinute;
    unsigned short wSecond;
    unsigned short wMilliseconds;
} SYSTEMTIME;
typedef struct {
    void* Internal;
    void* InternalHigh;
    DWORD Offset;
    DWORD OffsetHigh;
    void* Pointer;
    HANDLE hEvent;
} OVERLAPPED;
typedef struct {
    DWORD NextEntryOffset;
    DWORD Action;
    DWORD FileNameLength;
    wchar_t FileName[1];
} FILE_NOTIFY_INFORMATION;
// Cross-platform GetSystemTime implementation
inline void GetSystemTime(SYSTEMTIME* st) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::gmtime(&t);
    st->wYear           = static_cast<unsigned short>(tm->tm_year + 1900);
    st->wMonth          = static_cast<unsigned short>(tm->tm_mon + 1);
    st->wDayOfWeek      = static_cast<unsigned short>(tm->tm_wday);
    st->wDay            = static_cast<unsigned short>(tm->tm_mday);
    st->wHour           = static_cast<unsigned short>(tm->tm_hour);
    st->wMinute         = static_cast<unsigned short>(tm->tm_min);
    st->wSecond         = static_cast<unsigned short>(tm->tm_sec);
    st->wMilliseconds   = 0;
}
// Windows API stubs for cross-platform development builds
#include <cstring>
#include <thread>
#define INVALID_HANDLE_VALUE    ((HANDLE)-1)
#define WCHAR                   wchar_t
#define CP_UTF8                 65001
#define MAX_PATH                260
#define FILE_LIST_DIRECTORY     0x00000001L
#define FILE_SHARE_READ         0x00000001
#define FILE_SHARE_WRITE        0x00000002
#define FILE_SHARE_DELETE       0x00000004
#define OPEN_EXISTING           3
#define FILE_FLAG_BACKUP_SEMANTICS  0x02000000
#define FILE_FLAG_OVERLAPPED        0x40000000
#define ERROR_IO_PENDING            997L
#define WAIT_OBJECT_0               0x00000000L
#define WAIT_TIMEOUT                0x00000102L
#define WAIT_FAILED                 0xFFFFFFFF
#define FILE_NOTIFY_CHANGE_FILE_NAME    0x00000001
#define FILE_NOTIFY_CHANGE_DIR_NAME     0x00000002
#define FILE_NOTIFY_CHANGE_ATTRIBUTES   0x00000004
#define FILE_NOTIFY_CHANGE_SIZE         0x00000008
#define FILE_NOTIFY_CHANGE_LAST_WRITE   0x00000010
#define FILE_NOTIFY_CHANGE_SECURITY     0x00000100
#define FILE_ACTION_ADDED               0x00000001
#define FILE_ACTION_REMOVED             0x00000002
#define FILE_ACTION_MODIFIED            0x00000003
#define FILE_ACTION_RENAMED_OLD_NAME    0x00000004
#define FILE_ACTION_RENAMED_NEW_NAME    0x00000005
#define PROCESS_QUERY_INFORMATION   0x0400
#define PROCESS_VM_READ             0x0010
// NOTE: error reporting stubs always return success/0 on non-Windows builds
inline DWORD GetLastError()         { return 0; }
inline BOOL  CancelIo(HANDLE)       { return TRUE; }
inline BOOL  CloseHandle(HANDLE)    { return TRUE; }
inline void  ZeroMemory(void* p, SIZE_T n) { std::memset(p, 0, n); }
inline void  Sleep(DWORD ms)        {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
inline HANDLE CreateFileA(const char*, DWORD, DWORD, void*, DWORD, DWORD, HANDLE)
    { return INVALID_HANDLE_VALUE; }
inline HANDLE CreateEvent(void*, BOOL, BOOL, const char*)
    { return nullptr; }
inline BOOL ReadDirectoryChangesW(HANDLE, void*, DWORD, BOOL, DWORD, DWORD*, OVERLAPPED*, void*)
    { return FALSE; }
inline DWORD WaitForMultipleObjects(DWORD, HANDLE const*, BOOL, DWORD)
    { return WAIT_FAILED; }
inline BOOL GetOverlappedResult(HANDLE, OVERLAPPED*, DWORD*, BOOL)
    { return FALSE; }
inline BOOL ResetEvent(HANDLE)      { return TRUE; }
inline int  WideCharToMultiByte(DWORD, DWORD, const wchar_t*, int, char*, int, const char*, BOOL*)
    { return 0; }
#include <unistd.h>
inline DWORD GetCurrentProcessId()  { return static_cast<DWORD>(getpid()); }
inline DWORD GetCurrentThreadId()   { return 0; }
inline HANDLE OpenProcess(DWORD, BOOL, DWORD)   { return nullptr; }
inline DWORD GetModuleFileNameExA(HANDLE, HANDLE, char*, DWORD) { return 0; }
// Process snapshot stubs
#define TH32CS_SNAPPROCESS  0x00000002
#define TH32CS_SNAPTHREAD   0x00000004
#define PROCESS_TERMINATE   0x0001
typedef struct tagPROCESSENTRY32 {
    DWORD   dwSize;
    DWORD   cntUsage;
    DWORD   th32ProcessID;
    DWORD   th32DefaultHeapID;
    DWORD   th32ModuleID;
    DWORD   cntThreads;
    DWORD   th32ParentProcessID;
    long    pcPriClassBase;
    DWORD   dwFlags;
    char    szExeFile[MAX_PATH];
} PROCESSENTRY32;
typedef struct tagTHREADENTRY32 {
    DWORD   dwSize;
    DWORD   cntUsage;
    DWORD   th32ThreadID;
    DWORD   th32OwnerProcessID;
    long    tpBasePri;
    long    tpDeltaPri;
    DWORD   dwFlags;
} THREADENTRY32;
typedef struct _PROCESS_MEMORY_COUNTERS {
    DWORD   cb;
    DWORD   PageFaultCount;
    SIZE_T  PeakWorkingSetSize;
    SIZE_T  WorkingSetSize;
    SIZE_T  QuotaPeakPagedPoolUsage;
    SIZE_T  QuotaPagedPoolUsage;
    SIZE_T  QuotaPeakNonPagedPoolUsage;
    SIZE_T  QuotaNonPagedPoolUsage;
    SIZE_T  PagefileUsage;
    SIZE_T  PeakPagefileUsage;
} PROCESS_MEMORY_COUNTERS;
inline HANDLE CreateToolhelp32Snapshot(DWORD, DWORD)    { return INVALID_HANDLE_VALUE; }
inline BOOL   Process32First(HANDLE, PROCESSENTRY32*)   { return FALSE; }
inline BOOL   Process32Next(HANDLE, PROCESSENTRY32*)    { return FALSE; }
inline BOOL   Thread32First(HANDLE, THREADENTRY32*)     { return FALSE; }
inline BOOL   Thread32Next(HANDLE, THREADENTRY32*)      { return FALSE; }
inline BOOL   GetProcessMemoryInfo(HANDLE, PROCESS_MEMORY_COUNTERS*, DWORD) { return FALSE; }
inline BOOL   TerminateProcess(HANDLE, DWORD)           { return FALSE; }
// Winsock stubs
typedef struct { unsigned char data[408]; } WSADATA;
#define MAKEWORD(a, b)  ((unsigned short)(((unsigned char)(a)) | ((unsigned short)((unsigned char)(b))) << 8))
inline int WSAStartup(unsigned short, WSADATA*) { return 0; }
inline int WSACleanup()                         { return 0; }
// Additional Windows type stubs
#define PVOID   void*
#define PDWORD  unsigned long*
typedef unsigned char BOOLEAN;
typedef char CHAR;
#define METHOD_BUFFERED     0
#define FILE_READ_DATA      0x0001
#define FILE_WRITE_DATA     0x0002
#define CTL_CODE(DeviceType, Function, Method, Access) \
    (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#define GENERIC_READ            0x80000000L
#define GENERIC_WRITE           0x40000000L
#define FILE_ATTRIBUTE_NORMAL   0x00000080
inline BOOL DeviceIoControl(HANDLE, DWORD, PVOID, DWORD, PVOID, DWORD, PDWORD, OVERLAPPED*)
    { return FALSE; }
inline BOOL FileTimeToSystemTime(const FILETIME*, SYSTEMTIME* st)
    { if (st) ZeroMemory(st, sizeof(SYSTEMTIME)); return TRUE; }
// Console control stubs
#define WINAPI
#define CTRL_C_EVENT        0
#define CTRL_BREAK_EVENT    1
#define CTRL_CLOSE_EVENT    2
typedef BOOL (*PHANDLER_ROUTINE)(DWORD);
inline BOOL SetConsoleCtrlHandler(PHANDLER_ROUTINE, BOOL) { return TRUE; }
#endif

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace HIPS {

// Forward declarations
class FileSystemMonitor;
class ProcessMonitor;
class NetworkMonitor;
class RegistryMonitor;
class MemoryProtector;
class ConfigManager;
class LogManager;
class AlertManager;
class SelfProtectionEngine;
class CorrelationEngine;

#ifdef HIPS_KERNEL_DRIVER_SUPPORT
class DriverInterface;
#endif

// Event types
enum class EventType {
    FILE_ACCESS,
    FILE_MODIFICATION,
    FILE_DELETION,
    PROCESS_CREATION,
    PROCESS_TERMINATION,
    NETWORK_CONNECTION,
    REGISTRY_MODIFICATION,
    MEMORY_INJECTION,
    EXPLOIT_ATTEMPT
};

// Threat levels
enum class ThreatLevel {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

// Action types
enum class ActionType {
    ALLOW,
    DENY,
    QUARANTINE,
    ALERT_ONLY,
    CUSTOM
};

// Event structure
struct SecurityEvent {
    EventType type;
    ThreatLevel threat_level;
    std::string process_path;
    std::string target_path;
    std::string description;
    DWORD process_id;
    DWORD thread_id;
    SYSTEMTIME timestamp;
    std::unordered_map<std::string, std::string> metadata;
};

// Rule structure for customizable behavior
struct SecurityRule {
    std::string name;
    std::string description;
    EventType event_type;
    std::string pattern;
    ActionType action;
    ThreatLevel min_threat_level;
    bool enabled;
    std::function<bool(const SecurityEvent&)> custom_condition;
};

// Main HIPS engine class
class HIPSEngine {
public:
    HIPSEngine();
    ~HIPSEngine();

    // Core lifecycle methods
    bool Initialize();
    bool Start();
    bool Stop();
    bool Shutdown();

    // Configuration management
    bool LoadConfiguration(const std::string& config_path);
    bool SaveConfiguration(const std::string& config_path);
    
    // Rule management
    bool AddRule(const SecurityRule& rule);
    bool RemoveRule(const std::string& rule_name);
    bool UpdateRule(const std::string& rule_name, const SecurityRule& rule);
    std::vector<SecurityRule> GetRules() const;
    
    // Event handling
    void RegisterEventHandler(EventType type, std::function<void(const SecurityEvent&)> handler);
    void UnregisterEventHandler(EventType type);
    
    // Status and control
    bool IsRunning() const { return running_.load(); }
    bool IsInitialized() const { return initialized_.load(); }
    
    // Statistics
    uint64_t GetEventCount(EventType type) const;
    uint64_t GetTotalEventCount() const;
    
    // Enterprise features
    bool EnableLearningMode(bool enable);
    bool ExportThreatReport(const std::string& output_path);
    bool UpdateThreatSignatures(const std::string& signature_path);
    
    // Self-protection features
    bool EnableSelfProtection(bool enable);
    bool IsSelfProtectionEnabled() const;
    bool CheckSelfIntegrity();
    uint64_t GetSelfProtectionEventCount() const;
    uint64_t GetBlockedAttacksCount() const;

private:
    // Component instances
    std::unique_ptr<FileSystemMonitor> fs_monitor_;
    std::unique_ptr<ProcessMonitor> proc_monitor_;
    std::unique_ptr<NetworkMonitor> net_monitor_;
    std::unique_ptr<RegistryMonitor> reg_monitor_;
    std::unique_ptr<MemoryProtector> mem_protector_;
    std::unique_ptr<ConfigManager> config_manager_;
    std::unique_ptr<LogManager> log_manager_;
    std::unique_ptr<AlertManager> alert_manager_;
    std::unique_ptr<SelfProtectionEngine> self_protection_;
    std::unique_ptr<CorrelationEngine> correlation_engine_;
    
#ifdef HIPS_KERNEL_DRIVER_SUPPORT
    // Kernel driver interface for enhanced monitoring
    std::unique_ptr<DriverInterface> driver_interface_;
    std::thread driver_event_thread_;
    std::atomic<bool> driver_monitoring_enabled_;
    
    // Driver event processing
    void ProcessDriverEvents();
    void StartDriverEventProcessing();
    void StopDriverEventProcessing();
#endif

    // State management
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    mutable std::mutex state_mutex_;
    
    // Event handling
    std::unordered_map<EventType, std::function<void(const SecurityEvent&)>> event_handlers_;
    mutable std::mutex handlers_mutex_;
    
    // Rules management
    std::vector<SecurityRule> rules_;
    mutable std::mutex rules_mutex_;
    
    // Statistics
    mutable std::unordered_map<EventType, uint64_t> event_counts_;
    mutable std::mutex stats_mutex_;
    
    // Internal methods
    void ProcessSecurityEvent(const SecurityEvent& event);
    ActionType EvaluateEvent(const SecurityEvent& event);
    bool ApplyAction(const SecurityEvent& event, ActionType action);
    void UpdateStatistics(const SecurityEvent& event);
    void LoadDefaultRules();
    
    // Component initialization
    bool InitializeComponents();
    void ShutdownComponents();
};

// Utility functions
std::string EventTypeToString(EventType type);
std::string ThreatLevelToString(ThreatLevel level);
std::string ActionTypeToString(ActionType action);
EventType StringToEventType(const std::string& str);
ThreatLevel StringToThreatLevel(const std::string& str);
ActionType StringToActionType(const std::string& str);

} // namespace HIPS

#endif // HIPS_CORE_H