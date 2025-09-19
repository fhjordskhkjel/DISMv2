#include "hips_core.h"
#include "file_monitor.h"
#include "process_monitor.h"
#include "network_monitor.h"
#include "registry_monitor.h"
#include "memory_protector.h"
#include "config_manager.h"
#include "log_manager.h"
#include "alert_manager.h"
#include <iostream>
#include <sstream>
#include <fstream>

namespace HIPS {

HIPSEngine::HIPSEngine() 
    : running_(false), initialized_(false) {
}

HIPSEngine::~HIPSEngine() {
    if (IsRunning()) {
        Stop();
    }
    if (IsInitialized()) {
        Shutdown();
    }
}

bool HIPSEngine::Initialize() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (initialized_.load()) {
        return true;
    }
    
    try {
        // Initialize logging first
        log_manager_ = std::make_unique<LogManager>();
        if (!log_manager_->Initialize()) {
            return false;
        }
        
        // Initialize configuration manager
        config_manager_ = std::make_unique<ConfigManager>();
        if (!config_manager_->Initialize()) {
            return false;
        }
        
        // Initialize alert manager
        alert_manager_ = std::make_unique<AlertManager>();
        if (!alert_manager_->Initialize()) {
            return false;
        }
        
        // Initialize monitoring components
        if (!InitializeComponents()) {
            return false;
        }
        
        // Load default rules
        LoadDefaultRules();
        
        initialized_.store(true);
        log_manager_->LogInfo("HIPS Engine initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        if (log_manager_) {
            log_manager_->LogError("Failed to initialize HIPS Engine: " + std::string(e.what()));
        }
        return false;
    }
}

bool HIPSEngine::InitializeComponents() {
    // Initialize file system monitor
    fs_monitor_ = std::make_unique<FileSystemMonitor>();
    if (!fs_monitor_->Initialize()) {
        return false;
    }
    
    // Register file system event callback
    fs_monitor_->RegisterCallback([this](const SecurityEvent& event) {
        ProcessSecurityEvent(event);
    });
    
    // Initialize process monitor
    proc_monitor_ = std::make_unique<ProcessMonitor>();
    if (!proc_monitor_->Initialize()) {
        return false;
    }
    
    // Register process event callback
    proc_monitor_->RegisterCallback([this](const SecurityEvent& event) {
        ProcessSecurityEvent(event);
    });
    
    // Initialize network monitor
    net_monitor_ = std::make_unique<NetworkMonitor>();
    if (!net_monitor_->Initialize()) {
        return false;
    }
    
    // Register network event callback
    net_monitor_->RegisterCallback([this](const SecurityEvent& event) {
        ProcessSecurityEvent(event);
    });
    
    // Initialize registry monitor
    reg_monitor_ = std::make_unique<RegistryMonitor>();
    if (!reg_monitor_->Initialize()) {
        return false;
    }
    
    // Register registry event callback
    reg_monitor_->RegisterCallback([this](const SecurityEvent& event) {
        ProcessSecurityEvent(event);
    });
    
    // Initialize memory protector
    mem_protector_ = std::make_unique<MemoryProtector>();
    if (!mem_protector_->Initialize()) {
        return false;
    }
    
    // Register memory protection event callback
    mem_protector_->RegisterCallback([this](const SecurityEvent& event) {
        ProcessSecurityEvent(event);
    });
    
    return true;
}

bool HIPSEngine::Start() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (!initialized_.load()) {
        return false;
    }
    
    if (running_.load()) {
        return true;
    }
    
    try {
        // Start all monitoring components
        if (!fs_monitor_->Start()) return false;
        if (!proc_monitor_->Start()) return false;
        if (!net_monitor_->Start()) return false;
        if (!reg_monitor_->Start()) return false;
        if (!mem_protector_->Start()) return false;
        
        running_.store(true);
        log_manager_->LogInfo("HIPS Engine started successfully");
        return true;
        
    } catch (const std::exception& e) {
        log_manager_->LogError("Failed to start HIPS Engine: " + std::string(e.what()));
        return false;
    }
}

bool HIPSEngine::Stop() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (!running_.load()) {
        return true;
    }
    
    try {
        // Stop all monitoring components
        if (mem_protector_) mem_protector_->Stop();
        if (reg_monitor_) reg_monitor_->Stop();
        if (net_monitor_) net_monitor_->Stop();
        if (proc_monitor_) proc_monitor_->Stop();
        if (fs_monitor_) fs_monitor_->Stop();
        
        running_.store(false);
        log_manager_->LogInfo("HIPS Engine stopped successfully");
        return true;
        
    } catch (const std::exception& e) {
        if (log_manager_) {
            log_manager_->LogError("Error stopping HIPS Engine: " + std::string(e.what()));
        }
        return false;
    }
}

bool HIPSEngine::Shutdown() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (running_.load()) {
        Stop();
    }
    
    try {
        ShutdownComponents();
        initialized_.store(false);
        
        if (log_manager_) {
            log_manager_->LogInfo("HIPS Engine shutdown successfully");
        }
        
        return true;
        
    } catch (const std::exception& e) {
        return false;
    }
}

void HIPSEngine::ShutdownComponents() {
    mem_protector_.reset();
    reg_monitor_.reset();
    net_monitor_.reset();
    proc_monitor_.reset();
    fs_monitor_.reset();
    alert_manager_.reset();
    config_manager_.reset();
    log_manager_.reset();
}

void HIPSEngine::ProcessSecurityEvent(const SecurityEvent& event) {
    // Update statistics
    UpdateStatistics(event);
    
    // Log the event
    if (log_manager_) {
        std::ostringstream oss;
        oss << "Security Event: " << EventTypeToString(event.type)
            << " | Threat Level: " << ThreatLevelToString(event.threat_level)
            << " | Process: " << event.process_path
            << " | Target: " << event.target_path;
        log_manager_->LogInfo(oss.str());
    }
    
    // Evaluate event against rules
    ActionType action = EvaluateEvent(event);
    
    // Apply the determined action
    ApplyAction(event, action);
    
    // Call registered event handlers
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        auto it = event_handlers_.find(event.type);
        if (it != event_handlers_.end() && it->second) {
            it->second(event);
        }
    }
}

ActionType HIPSEngine::EvaluateEvent(const SecurityEvent& event) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    
    for (const auto& rule : rules_) {
        if (!rule.enabled) continue;
        
        if (rule.event_type == event.type || rule.event_type == EventType::FILE_ACCESS) {
            // Check threat level threshold
            if (static_cast<int>(event.threat_level) >= static_cast<int>(rule.min_threat_level)) {
                // Check pattern match
                bool pattern_match = false;
                if (rule.pattern.empty()) {
                    pattern_match = true;
                } else {
                    // Simple pattern matching (could be enhanced with regex)
                    if (event.target_path.find(rule.pattern) != std::string::npos ||
                        event.process_path.find(rule.pattern) != std::string::npos) {
                        pattern_match = true;
                    }
                }
                
                // Check custom condition if provided
                bool custom_match = true;
                if (rule.custom_condition) {
                    custom_match = rule.custom_condition(event);
                }
                
                if (pattern_match && custom_match) {
                    return rule.action;
                }
            }
        }
    }
    
    // Default action for unmatched events
    return ActionType::ALLOW;
}

bool HIPSEngine::ApplyAction(const SecurityEvent& event, ActionType action) {
    switch (action) {
        case ActionType::ALLOW:
            // Event is allowed, no action needed
            return true;
            
        case ActionType::DENY:
            // Block the action
            if (alert_manager_) {
                alert_manager_->SendAlert(event, "Action denied by HIPS rule");
            }
            return true;
            
        case ActionType::QUARANTINE:
            // Quarantine the file/process
            if (alert_manager_) {
                alert_manager_->SendAlert(event, "File/Process quarantined by HIPS");
            }
            // Implementation would go here
            return true;
            
        case ActionType::ALERT_ONLY:
            // Just send an alert
            if (alert_manager_) {
                alert_manager_->SendAlert(event, "HIPS alert - suspicious activity detected");
            }
            return true;
            
        case ActionType::CUSTOM:
            // Custom action handling
            return true;
            
        default:
            return false;
    }
}

void HIPSEngine::UpdateStatistics(const SecurityEvent& event) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    event_counts_[event.type]++;
}

bool HIPSEngine::LoadConfiguration(const std::string& config_path) {
    if (!config_manager_) return false;
    return config_manager_->LoadConfiguration(config_path);
}

bool HIPSEngine::SaveConfiguration(const std::string& config_path) {
    if (!config_manager_) return false;
    return config_manager_->SaveConfiguration(config_path);
}

bool HIPSEngine::AddRule(const SecurityRule& rule) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    rules_.push_back(rule);
    return true;
}

bool HIPSEngine::RemoveRule(const std::string& rule_name) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    auto it = std::remove_if(rules_.begin(), rules_.end(),
        [&rule_name](const SecurityRule& rule) {
            return rule.name == rule_name;
        });
    
    if (it != rules_.end()) {
        rules_.erase(it, rules_.end());
        return true;
    }
    return false;
}

std::vector<SecurityRule> HIPSEngine::GetRules() const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    return rules_;
}

uint64_t HIPSEngine::GetEventCount(EventType type) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = event_counts_.find(type);
    return (it != event_counts_.end()) ? it->second : 0;
}

uint64_t HIPSEngine::GetTotalEventCount() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    uint64_t total = 0;
    for (const auto& pair : event_counts_) {
        total += pair.second;
    }
    return total;
}

void HIPSEngine::LoadDefaultRules() {
    // Add some default security rules
    SecurityRule suspicious_process_rule;
    suspicious_process_rule.name = "Suspicious Process Execution";
    suspicious_process_rule.description = "Detect execution of suspicious processes";
    suspicious_process_rule.event_type = EventType::PROCESS_CREATION;
    suspicious_process_rule.pattern = "";
    suspicious_process_rule.action = ActionType::ALERT_ONLY;
    suspicious_process_rule.min_threat_level = ThreatLevel::MEDIUM;
    suspicious_process_rule.enabled = true;
    AddRule(suspicious_process_rule);
    
    SecurityRule critical_file_access;
    critical_file_access.name = "Critical File Access";
    critical_file_access.description = "Monitor access to critical system files";
    critical_file_access.event_type = EventType::FILE_ACCESS;
    critical_file_access.pattern = "System32";
    critical_file_access.action = ActionType::ALERT_ONLY;
    critical_file_access.min_threat_level = ThreatLevel::HIGH;
    critical_file_access.enabled = true;
    AddRule(critical_file_access);
}

// Utility function implementations
std::string EventTypeToString(EventType type) {
    switch (type) {
        case EventType::FILE_ACCESS: return "FILE_ACCESS";
        case EventType::FILE_MODIFICATION: return "FILE_MODIFICATION";
        case EventType::FILE_DELETION: return "FILE_DELETION";
        case EventType::PROCESS_CREATION: return "PROCESS_CREATION";
        case EventType::PROCESS_TERMINATION: return "PROCESS_TERMINATION";
        case EventType::NETWORK_CONNECTION: return "NETWORK_CONNECTION";
        case EventType::REGISTRY_MODIFICATION: return "REGISTRY_MODIFICATION";
        case EventType::MEMORY_INJECTION: return "MEMORY_INJECTION";
        case EventType::EXPLOIT_ATTEMPT: return "EXPLOIT_ATTEMPT";
        default: return "UNKNOWN";
    }
}

std::string ThreatLevelToString(ThreatLevel level) {
    switch (level) {
        case ThreatLevel::LOW: return "LOW";
        case ThreatLevel::MEDIUM: return "MEDIUM";
        case ThreatLevel::HIGH: return "HIGH";
        case ThreatLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

std::string ActionTypeToString(ActionType action) {
    switch (action) {
        case ActionType::ALLOW: return "ALLOW";
        case ActionType::DENY: return "DENY";
        case ActionType::QUARANTINE: return "QUARANTINE";
        case ActionType::ALERT_ONLY: return "ALERT_ONLY";
        case ActionType::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

} // namespace HIPS