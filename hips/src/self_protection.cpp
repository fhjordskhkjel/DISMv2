#include "self_protection.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <aclapi.h>
#include <wintrust.h>
#include <softpub.h>
#include <mscat.h>
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

namespace HIPS {

SelfProtectionEngine::SelfProtectionEngine() 
    : running_(false), initialized_(false), protection_event_count_(0), blocked_attacks_count_(0) {
}

SelfProtectionEngine::~SelfProtectionEngine() {
    if (IsRunning()) {
        Stop();
    }
    if (IsInitialized()) {
        Shutdown();
    }
}

bool SelfProtectionEngine::Initialize() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (initialized_.load()) {
        return true;
    }
    
    try {
        // Initialize default configuration
        config_.process_protection_enabled = true;
        config_.file_protection_enabled = true;
        config_.registry_protection_enabled = true;
        config_.memory_protection_enabled = true;
        config_.debug_protection_enabled = true;
        config_.service_protection_enabled = true;
        config_.config_protection_enabled = true;
        
        // Add default protected resources
        AddDefaultProtectedResources();
        
        // Load default protection rules
        LoadDefaultProtectionRules();
        
        initialized_.store(true);
        return true;
        
    } catch (const std::exception& e) {
        return false;
    }
}

bool SelfProtectionEngine::Start() {
    if (!initialized_.load() || running_.load()) {
        return false;
    }
    
    try {
        // Setup protection mechanisms based on configuration
        if (config_.process_protection_enabled) {
            SetupProcessProtection();
        }
        
        if (config_.file_protection_enabled) {
            SetupFileProtection();
        }
        
        if (config_.registry_protection_enabled) {
            SetupRegistryProtection();
        }
        
        if (config_.memory_protection_enabled) {
            SetupMemoryProtection();
        }
        
        if (config_.debug_protection_enabled) {
            SetupDebugProtection();
        }
        
        if (config_.service_protection_enabled) {
            SetupServiceProtection();
        }
        
        running_.store(true);
        return true;
        
    } catch (const std::exception& e) {
        return false;
    }
}

bool SelfProtectionEngine::Stop() {
    if (!running_.load()) {
        return true;
    }
    
    running_.store(false);
    return true;
}

bool SelfProtectionEngine::Shutdown() {
    if (running_.load()) {
        Stop();
    }
    
    initialized_.store(false);
    return true;
}

bool SelfProtectionEngine::LoadConfiguration(const SelfProtectionConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
    return true;
}

SelfProtectionConfig SelfProtectionEngine::GetConfiguration() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

bool SelfProtectionEngine::AddRule(const SelfProtectionRule& rule) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    rules_.push_back(rule);
    return true;
}

bool SelfProtectionEngine::RemoveRule(const std::string& rule_name) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    auto it = std::remove_if(rules_.begin(), rules_.end(),
        [&rule_name](const SelfProtectionRule& rule) {
            return rule.name == rule_name;
        });
    
    if (it != rules_.end()) {
        rules_.erase(it, rules_.end());
        return true;
    }
    return false;
}

std::vector<SelfProtectionRule> SelfProtectionEngine::GetRules() const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    return rules_;
}

bool SelfProtectionEngine::AddProtectedFile(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.protected_files.push_back(file_path);
    return true;
}

bool SelfProtectionEngine::AddProtectedDirectory(const std::string& directory_path) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.protected_directories.push_back(directory_path);
    return true;
}

bool SelfProtectionEngine::AddProtectedRegistryKey(const std::string& registry_key) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.protected_registry_keys.push_back(registry_key);
    return true;
}

bool SelfProtectionEngine::AddProtectedProcess(const std::string& process_name) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.protected_processes.push_back(process_name);
    return true;
}

bool SelfProtectionEngine::AddProtectedService(const std::string& service_name) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.protected_services.push_back(service_name);
    return true;
}

bool SelfProtectionEngine::AddTrustedProcess(const std::string& process_path) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.trusted_processes.insert(process_path);
    return true;
}

bool SelfProtectionEngine::RemoveTrustedProcess(const std::string& process_path) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_.trusted_processes.erase(process_path) > 0;
}

bool SelfProtectionEngine::IsTrustedProcess(const std::string& process_path) const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_.trusted_processes.find(process_path) != config_.trusted_processes.end();
}

void SelfProtectionEngine::RegisterEventHandler(std::function<void(const SelfProtectionEvent&)> handler) {
    event_handler_ = handler;
}

uint64_t SelfProtectionEngine::GetProtectionEventCount() const {
    return protection_event_count_.load();
}

uint64_t SelfProtectionEngine::GetBlockedAttacksCount() const {
    return blocked_attacks_count_.load();
}

void SelfProtectionEngine::ProcessProtectionEvent(const SelfProtectionEvent& event) {
    protection_event_count_++;
    
    // Evaluate the event against protection rules
    SelfProtectionAction action = EvaluateProtectionEvent(event);
    
    // Apply the determined action
    if (ApplyProtectionAction(event, action)) {
        if (action != SelfProtectionAction::ALERT_ONLY) {
            blocked_attacks_count_++;
        }
    }
    
    // Call registered event handler
    if (event_handler_) {
        event_handler_(event);
    }
}

SelfProtectionAction SelfProtectionEngine::EvaluateProtectionEvent(const SelfProtectionEvent& event) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    
    for (const auto& rule : rules_) {
        if (!rule.enabled) continue;
        
        if (rule.event_type == event.type) {
            // Check threat level threshold
            if (static_cast<int>(event.threat_level) >= static_cast<int>(rule.min_threat_level)) {
                // Check custom condition if provided
                bool custom_match = true;
                if (rule.custom_condition) {
                    custom_match = rule.custom_condition(event);
                }
                
                if (custom_match) {
                    return rule.action;
                }
            }
        }
    }
    
    // Default action for unmatched events
    return SelfProtectionAction::BLOCK_AND_ALERT;
}

bool SelfProtectionEngine::ApplyProtectionAction(const SelfProtectionEvent& event, SelfProtectionAction action) {
    switch (action) {
        case SelfProtectionAction::BLOCK_AND_ALERT:
            // Block the action and generate alert
            return true;
            
        case SelfProtectionAction::ALERT_ONLY:
            // Generate alert but allow the action
            return true;
            
        case SelfProtectionAction::BLOCK_SILENTLY:
            // Block the action without alert
            return true;
            
        case SelfProtectionAction::TERMINATE_ATTACKER:
            // Terminate the attacking process
#ifdef _WIN32
            if (event.attacker_pid != 0 && !IsCurrentProcess(event.attacker_pid)) {
                HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, event.attacker_pid);
                if (process) {
                    TerminateProcess(process, 1);
                    CloseHandle(process);
                }
            }
#endif
            return true;
            
        case SelfProtectionAction::QUARANTINE_ATTACKER:
            // Quarantine the attacking process/file
            return true;
            
        default:
            return false;
    }
}

void SelfProtectionEngine::LoadDefaultProtectionRules() {
    // Process termination protection
    SelfProtectionRule process_term_rule;
    process_term_rule.name = "Process Termination Protection";
    process_term_rule.description = "Prevent unauthorized termination of HIPS processes";
    process_term_rule.event_type = SelfProtectionEventType::PROCESS_TERMINATION_ATTEMPT;
    process_term_rule.action = SelfProtectionAction::BLOCK_AND_ALERT;
    process_term_rule.min_threat_level = ThreatLevel::HIGH;
    process_term_rule.enabled = true;
    AddRule(process_term_rule);
    
    // Process injection protection
    SelfProtectionRule process_inj_rule;
    process_inj_rule.name = "Process Injection Protection";
    process_inj_rule.description = "Prevent code injection into HIPS processes";
    process_inj_rule.event_type = SelfProtectionEventType::PROCESS_INJECTION_ATTEMPT;
    process_inj_rule.action = SelfProtectionAction::TERMINATE_ATTACKER;
    process_inj_rule.min_threat_level = ThreatLevel::CRITICAL;
    process_inj_rule.enabled = true;
    AddRule(process_inj_rule);
    
    // File tampering protection
    SelfProtectionRule file_tamper_rule;
    file_tamper_rule.name = "File Tampering Protection";
    file_tamper_rule.description = "Prevent unauthorized modification of HIPS files";
    file_tamper_rule.event_type = SelfProtectionEventType::FILE_TAMPERING_ATTEMPT;
    file_tamper_rule.action = SelfProtectionAction::BLOCK_AND_ALERT;
    file_tamper_rule.min_threat_level = ThreatLevel::HIGH;
    file_tamper_rule.enabled = true;
    AddRule(file_tamper_rule);
    
    // Registry tampering protection
    SelfProtectionRule reg_tamper_rule;
    reg_tamper_rule.name = "Registry Tampering Protection";
    reg_tamper_rule.description = "Prevent unauthorized modification of HIPS registry keys";
    reg_tamper_rule.event_type = SelfProtectionEventType::REGISTRY_TAMPERING_ATTEMPT;
    reg_tamper_rule.action = SelfProtectionAction::BLOCK_AND_ALERT;
    reg_tamper_rule.min_threat_level = ThreatLevel::HIGH;
    reg_tamper_rule.enabled = true;
    AddRule(reg_tamper_rule);
    
    // Debug protection
    SelfProtectionRule debug_rule;
    debug_rule.name = "Debug Protection";
    debug_rule.description = "Prevent debugging of HIPS processes";
    debug_rule.event_type = SelfProtectionEventType::DEBUG_ATTEMPT;
    debug_rule.action = SelfProtectionAction::TERMINATE_ATTACKER;
    debug_rule.min_threat_level = ThreatLevel::CRITICAL;
    debug_rule.enabled = true;
    AddRule(debug_rule);
    
    // Service protection
    SelfProtectionRule service_rule;
    service_rule.name = "Service Protection";
    service_rule.description = "Prevent unauthorized stopping of HIPS services";
    service_rule.event_type = SelfProtectionEventType::SERVICE_STOP_ATTEMPT;
    service_rule.action = SelfProtectionAction::BLOCK_AND_ALERT;
    service_rule.min_threat_level = ThreatLevel::HIGH;
    service_rule.enabled = true;
    AddRule(service_rule);
}

void SelfProtectionEngine::AddDefaultProtectedResources() {
    // Protect HIPS executables
    AddProtectedFile("hips.exe");
    AddProtectedFile("HipsGui.exe");
    AddProtectedFile("HipsDriver.sys");
    AddProtectedFile("hips_lib.dll");
    
    // Protect HIPS directories
    AddProtectedDirectory("C:\\Program Files\\HIPS");
    AddProtectedDirectory("C:\\Windows\\System32\\drivers\\HipsDriver.sys");
    
    // Protect configuration files
    AddProtectedFile("hips_config.json");
    AddProtectedFile("protection_rules.json");
    
    // Protect registry keys
    AddProtectedRegistryKey("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\HipsDriver");
    AddProtectedRegistryKey("HKEY_LOCAL_MACHINE\\SOFTWARE\\HIPS");
    
    // Protect processes
    AddProtectedProcess("hips.exe");
    AddProtectedProcess("HipsGui.exe");
    
    // Protect services
    AddProtectedService("HipsDriver");
    AddProtectedService("HipsService");
    
    // Add trusted system processes
    AddTrustedProcess("C:\\Windows\\System32\\services.exe");
    AddTrustedProcess("C:\\Windows\\System32\\winlogon.exe");
    AddTrustedProcess("C:\\Windows\\System32\\csrss.exe");
}

bool SelfProtectionEngine::IsProtectedResource(const std::string& resource_path) const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // Check protected files
    for (const auto& file : config_.protected_files) {
        if (resource_path.find(file) != std::string::npos) {
            return true;
        }
    }
    
    // Check protected directories
    for (const auto& dir : config_.protected_directories) {
        if (resource_path.find(dir) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

bool SelfProtectionEngine::IsCurrentProcess(DWORD pid) const {
#ifdef _WIN32
    return GetCurrentProcessId() == pid;
#else
    return false;
#endif
}

SelfProtectionEvent SelfProtectionEngine::CreateProtectionEvent(SelfProtectionEventType type,
                                                              const std::string& attacker_path,
                                                              const std::string& target_resource,
                                                              const std::string& description) {
    SelfProtectionEvent event;
    event.type = type;
    event.threat_level = ThreatLevel::HIGH;
    event.attacker_process_path = attacker_path;
    event.target_resource = target_resource;
    event.description = description;
    event.attacker_pid = 0;
    event.target_pid = 0;
    
#ifdef _WIN32
    GetSystemTime(&event.timestamp);
    event.attacker_pid = GetCurrentProcessId(); // This would be replaced with actual attacker PID
#endif
    
    return event;
}

void SelfProtectionEngine::SetupProcessProtection() {
#ifdef _WIN32
    SetupWindowsProcessProtection();
#endif
}

void SelfProtectionEngine::SetupFileProtection() {
#ifdef _WIN32
    SetupWindowsFileProtection();
#endif
}

void SelfProtectionEngine::SetupRegistryProtection() {
#ifdef _WIN32
    SetupWindowsRegistryProtection();
#endif
}

void SelfProtectionEngine::SetupMemoryProtection() {
    // Memory protection setup - monitor for injection attempts
    // This would integrate with existing MemoryProtector
}

void SelfProtectionEngine::SetupDebugProtection() {
#ifdef _WIN32
    SetupWindowsDebugProtection();
#endif
}

void SelfProtectionEngine::SetupServiceProtection() {
#ifdef _WIN32
    SetupWindowsServiceProtection();
#endif
}

#ifdef _WIN32
void SelfProtectionEngine::SetupWindowsProcessProtection() {
    // Enable process protection flags
    HANDLE currentProcess = GetCurrentProcess();
    
    // Set process mitigation policies
    PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY signaturePolicy = {};
    signaturePolicy.MicrosoftSignedOnly = 1;
    
    SetProcessMitigationPolicy(ProcessSignaturePolicy, &signaturePolicy, sizeof(signaturePolicy));
    
    // Set DEP policy
    PROCESS_MITIGATION_DEP_POLICY depPolicy = {};
    depPolicy.Enable = 1;
    depPolicy.Permanent = 1;
    
    SetProcessMitigationPolicy(ProcessDEPPolicy, &depPolicy, sizeof(depPolicy));
}

void SelfProtectionEngine::SetupWindowsFileProtection() {
    // Setup file system monitoring for protected files
    // This would integrate with the existing FileSystemMonitor
}

void SelfProtectionEngine::SetupWindowsRegistryProtection() {
    // Setup registry monitoring for protected keys
    // This would integrate with the existing RegistryMonitor
}

void SelfProtectionEngine::SetupWindowsDebugProtection() {
    // Enable debug protection
    HANDLE currentProcess = GetCurrentProcess();
    
    // Prevent debugging
    PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY systemCallPolicy = {};
    systemCallPolicy.DisallowWin32kSystemCalls = 1;
    
    SetProcessMitigationPolicy(ProcessSystemCallDisablePolicy, &systemCallPolicy, sizeof(systemCallPolicy));
}

void SelfProtectionEngine::SetupWindowsServiceProtection() {
    // Setup service monitoring for protected services
    // This would monitor service control manager operations
}
#endif

bool SelfProtectionEngine::CheckProcessIntegrity() {
    // Verify current process integrity
#ifdef _WIN32
    char processPath[MAX_PATH];
    if (GetModuleFileNameA(NULL, processPath, MAX_PATH)) {
        return VerifyCodeSignature(processPath);
    }
#endif
    return false;
}

bool SelfProtectionEngine::CheckFileIntegrity() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    for (const auto& file : config_.protected_files) {
        // Check if file exists and verify integrity
        std::ifstream fileStream(file);
        if (!fileStream.good()) {
            // File missing or inaccessible
            SelfProtectionEvent event = CreateProtectionEvent(
                SelfProtectionEventType::FILE_TAMPERING_ATTEMPT,
                "unknown",
                file,
                "Protected file missing or inaccessible: " + file
            );
            ProcessProtectionEvent(event);
            return false;
        }
    }
    
    return true;
}

bool SelfProtectionEngine::CheckRegistryIntegrity() {
    // Check protected registry keys
    return true; // Placeholder implementation
}

bool SelfProtectionEngine::CheckServiceIntegrity() {
    // Check protected services status
    return true; // Placeholder implementation
}

bool SelfProtectionEngine::VerifyCodeSignature(const std::string& file_path) {
#ifdef _WIN32
    WINTRUST_FILE_INFO fileData;
    memset(&fileData, 0, sizeof(fileData));
    fileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
    
    // Convert to wide string
    std::wstring wideFilePath(file_path.begin(), file_path.end());
    fileData.pcwszFilePath = wideFilePath.c_str();
    fileData.hFile = NULL;
    fileData.pgKnownSubject = NULL;
    
    WINTRUST_DATA winTrustData;
    memset(&winTrustData, 0, sizeof(winTrustData));
    winTrustData.cbStruct = sizeof(winTrustData);
    winTrustData.pPolicyCallbackData = NULL;
    winTrustData.pSIPClientData = NULL;
    winTrustData.dwUIChoice = WTD_UI_NONE;
    winTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    winTrustData.dwUnionChoice = WTD_CHOICE_FILE;
    winTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
    winTrustData.hWVTStateData = NULL;
    winTrustData.pwszURLReference = NULL;
    winTrustData.dwUIContext = 0;
    winTrustData.pFile = &fileData;
    
    GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    
    LONG status = WinVerifyTrust(NULL, &policyGUID, &winTrustData);
    
    // Clean up
    winTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(NULL, &policyGUID, &winTrustData);
    
    return status == ERROR_SUCCESS;
#else
    return true; // Placeholder for non-Windows platforms
#endif
}

bool SelfProtectionEngine::VerifyFileHash(const std::string& file_path, const std::string& expected_hash) {
    // Placeholder implementation for file hash verification
    return true;
}

// Utility functions
std::string SelfProtectionEventTypeToString(SelfProtectionEventType type) {
    switch (type) {
        case SelfProtectionEventType::PROCESS_TERMINATION_ATTEMPT:
            return "Process Termination Attempt";
        case SelfProtectionEventType::PROCESS_INJECTION_ATTEMPT:
            return "Process Injection Attempt";
        case SelfProtectionEventType::FILE_TAMPERING_ATTEMPT:
            return "File Tampering Attempt";
        case SelfProtectionEventType::REGISTRY_TAMPERING_ATTEMPT:
            return "Registry Tampering Attempt";
        case SelfProtectionEventType::SERVICE_STOP_ATTEMPT:
            return "Service Stop Attempt";
        case SelfProtectionEventType::DEBUG_ATTEMPT:
            return "Debug Attempt";
        case SelfProtectionEventType::MEMORY_MANIPULATION_ATTEMPT:
            return "Memory Manipulation Attempt";
        case SelfProtectionEventType::CONFIG_MODIFICATION_ATTEMPT:
            return "Configuration Modification Attempt";
        case SelfProtectionEventType::DRIVER_UNLOAD_ATTEMPT:
            return "Driver Unload Attempt";
        default:
            return "Unknown";
    }
}

std::string SelfProtectionActionToString(SelfProtectionAction action) {
    switch (action) {
        case SelfProtectionAction::BLOCK_AND_ALERT:
            return "Block and Alert";
        case SelfProtectionAction::ALERT_ONLY:
            return "Alert Only";
        case SelfProtectionAction::BLOCK_SILENTLY:
            return "Block Silently";
        case SelfProtectionAction::TERMINATE_ATTACKER:
            return "Terminate Attacker";
        case SelfProtectionAction::QUARANTINE_ATTACKER:
            return "Quarantine Attacker";
        default:
            return "Unknown";
    }
}

} // namespace HIPS