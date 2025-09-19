#ifndef SELF_PROTECTION_H
#define SELF_PROTECTION_H

#include "hips_core.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <atomic>
#include <mutex>

namespace HIPS {

// Self-protection event types
enum class SelfProtectionEventType {
    PROCESS_TERMINATION_ATTEMPT,
    PROCESS_INJECTION_ATTEMPT,
    FILE_TAMPERING_ATTEMPT,
    REGISTRY_TAMPERING_ATTEMPT,
    SERVICE_STOP_ATTEMPT,
    DEBUG_ATTEMPT,
    MEMORY_MANIPULATION_ATTEMPT,
    CONFIG_MODIFICATION_ATTEMPT,
    DRIVER_UNLOAD_ATTEMPT
};

// Self-protection action types
enum class SelfProtectionAction {
    BLOCK_AND_ALERT,
    ALERT_ONLY,
    BLOCK_SILENTLY,
    TERMINATE_ATTACKER,
    QUARANTINE_ATTACKER
};

// Self-protection event structure
struct SelfProtectionEvent {
    SelfProtectionEventType type;
    ThreatLevel threat_level;
    std::string attacker_process_path;
    std::string target_resource;
    std::string description;
    DWORD attacker_pid;
    DWORD target_pid;
    SYSTEMTIME timestamp;
    std::unordered_map<std::string, std::string> metadata;
};

// Self-protection rule structure
struct SelfProtectionRule {
    std::string name;
    std::string description;
    SelfProtectionEventType event_type;
    SelfProtectionAction action;
    ThreatLevel min_threat_level;
    bool enabled;
    std::function<bool(const SelfProtectionEvent&)> custom_condition;
};

// Self-protection configuration
struct SelfProtectionConfig {
    bool process_protection_enabled = true;
    bool file_protection_enabled = true;
    bool registry_protection_enabled = true;
    bool memory_protection_enabled = true;
    bool debug_protection_enabled = true;
    bool service_protection_enabled = true;
    bool config_protection_enabled = true;
    
    // Protected resources
    std::vector<std::string> protected_files;
    std::vector<std::string> protected_directories;
    std::vector<std::string> protected_registry_keys;
    std::vector<std::string> protected_processes;
    std::vector<std::string> protected_services;
    
    // Allowed processes (whitelist)
    std::unordered_set<std::string> trusted_processes;
    
    // Response settings
    bool auto_quarantine_attackers = false;
    bool terminate_attacking_process = false;
    uint32_t max_protection_events_per_minute = 100;
};

// Main self-protection engine
class SelfProtectionEngine {
public:
    SelfProtectionEngine();
    ~SelfProtectionEngine();

    // Core lifecycle methods
    bool Initialize();
    bool Start();
    bool Stop();
    bool Shutdown();

    // Configuration
    bool LoadConfiguration(const SelfProtectionConfig& config);
    SelfProtectionConfig GetConfiguration() const;
    
    // Rule management
    bool AddRule(const SelfProtectionRule& rule);
    bool RemoveRule(const std::string& rule_name);
    std::vector<SelfProtectionRule> GetRules() const;
    
    // Protection management
    bool EnableProcessProtection(bool enable);
    bool EnableFileProtection(bool enable);
    bool EnableRegistryProtection(bool enable);
    bool EnableMemoryProtection(bool enable);
    bool EnableDebugProtection(bool enable);
    bool EnableServiceProtection(bool enable);
    
    // Resource protection
    bool AddProtectedFile(const std::string& file_path);
    bool AddProtectedDirectory(const std::string& directory_path);
    bool AddProtectedRegistryKey(const std::string& registry_key);
    bool AddProtectedProcess(const std::string& process_name);
    bool AddProtectedService(const std::string& service_name);
    
    // Trust management
    bool AddTrustedProcess(const std::string& process_path);
    bool RemoveTrustedProcess(const std::string& process_path);
    bool IsTrustedProcess(const std::string& process_path) const;
    
    // Event handling
    void RegisterEventHandler(std::function<void(const SelfProtectionEvent&)> handler);
    
    // Status and monitoring
    bool IsRunning() const { return running_.load(); }
    bool IsInitialized() const { return initialized_.load(); }
    uint64_t GetProtectionEventCount() const;
    uint64_t GetBlockedAttacksCount() const;
    
    // Manual protection checks
    bool CheckProcessIntegrity();
    bool CheckFileIntegrity();
    bool CheckRegistryIntegrity();
    bool CheckServiceIntegrity();

private:
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    SelfProtectionConfig config_;
    std::vector<SelfProtectionRule> rules_;
    std::function<void(const SelfProtectionEvent&)> event_handler_;
    
    mutable std::mutex config_mutex_;
    mutable std::mutex rules_mutex_;
    mutable std::mutex stats_mutex_;
    
    // Statistics
    std::atomic<uint64_t> protection_event_count_;
    std::atomic<uint64_t> blocked_attacks_count_;
    
    // Internal protection methods
    void SetupProcessProtection();
    void SetupFileProtection();
    void SetupRegistryProtection();
    void SetupMemoryProtection();
    void SetupDebugProtection();
    void SetupServiceProtection();
    
    // Event processing
    void ProcessProtectionEvent(const SelfProtectionEvent& event);
    SelfProtectionAction EvaluateProtectionEvent(const SelfProtectionEvent& event);
    bool ApplyProtectionAction(const SelfProtectionEvent& event, SelfProtectionAction action);
    
    // Protection helpers
    bool IsProtectedResource(const std::string& resource_path) const;
    bool IsCurrentProcess(DWORD pid) const;
    SelfProtectionEvent CreateProtectionEvent(SelfProtectionEventType type, 
                                            const std::string& attacker_path,
                                            const std::string& target_resource,
                                            const std::string& description);
    
    // Default rules setup
    void LoadDefaultProtectionRules();
    void AddDefaultProtectedResources();
    
    // Integrity verification
    bool VerifyCodeSignature(const std::string& file_path);
    bool VerifyFileHash(const std::string& file_path, const std::string& expected_hash);
    
#ifdef _WIN32
    // Windows-specific protection
    void SetupWindowsProcessProtection();
    void SetupWindowsFileProtection();
    void SetupWindowsRegistryProtection();
    void SetupWindowsDebugProtection();
    void SetupWindowsServiceProtection();
#endif
};

// Utility functions
std::string SelfProtectionEventTypeToString(SelfProtectionEventType type);
std::string SelfProtectionActionToString(SelfProtectionAction action);

} // namespace HIPS

#endif // SELF_PROTECTION_H