#ifndef HIPS_CORE_H
#define HIPS_CORE_H

#include <windows.h>
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