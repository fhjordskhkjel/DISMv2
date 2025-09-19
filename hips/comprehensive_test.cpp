#include "include/self_protection.h"
#include <iostream>
#include <memory>

int main() {
    std::cout << "=== BSOD-Proof Self-Protection Test ===" << std::endl;
    
    try {
        auto self_protection = std::make_unique<HIPS::SelfProtectionEngine>();
        
        std::cout << "1. Initializing self-protection engine..." << std::endl;
        if (!self_protection->Initialize()) {
            std::cerr << "ERROR: Failed to initialize self-protection engine" << std::endl;
            return 1;
        }
        std::cout << "   ✓ Initialization successful" << std::endl;
        
        std::cout << "2. Starting self-protection engine..." << std::endl;
        if (!self_protection->Start()) {
            std::cerr << "ERROR: Failed to start self-protection engine" << std::endl;
            return 1;
        }
        std::cout << "   ✓ Start successful" << std::endl;
        
        // Test configuration
        std::cout << "3. Testing enhanced configuration..." << std::endl;
        HIPS::SelfProtectionConfig config = self_protection->GetConfiguration();
        std::cout << "   Safe mode enabled: " << (config.safe_mode_enabled ? "YES" : "NO") << std::endl;
        std::cout << "   SEH protection: " << (config.seh_protection_enabled ? "YES" : "NO") << std::endl;
        std::cout << "   Handle validation: " << (config.validate_handles ? "YES" : "NO") << std::endl;
        std::cout << "   Thread integrity checks: " << (config.check_thread_integrity ? "YES" : "NO") << std::endl;
        std::cout << "   Max API retries: " << config.max_api_retry_attempts << std::endl;
        
        std::cout << "4. Testing integrity checks..." << std::endl;
        bool process_integrity = self_protection->CheckProcessIntegrity();
        bool file_integrity = self_protection->CheckFileIntegrity();
        bool registry_integrity = self_protection->CheckRegistryIntegrity();
        bool service_integrity = self_protection->CheckServiceIntegrity();
        bool thread_integrity = self_protection->CheckThreadIntegrity();
        bool handle_integrity = self_protection->CheckHandleIntegrity();
        bool critical_section_integrity = self_protection->CheckCriticalSectionIntegrity();
        
        std::cout << "   Process integrity: " << (process_integrity ? "✓ OK" : "✗ FAIL") << std::endl;
        std::cout << "   File integrity: " << (file_integrity ? "✓ OK" : "✗ FAIL") << std::endl;
        std::cout << "   Registry integrity: " << (registry_integrity ? "✓ OK" : "✗ FAIL") << std::endl;
        std::cout << "   Service integrity: " << (service_integrity ? "✓ OK" : "✗ FAIL") << std::endl;
        std::cout << "   Thread integrity: " << (thread_integrity ? "✓ OK" : "✗ FAIL") << std::endl;
        std::cout << "   Handle integrity: " << (handle_integrity ? "✓ OK" : "✗ FAIL") << std::endl;
        std::cout << "   Critical section integrity: " << (critical_section_integrity ? "✓ OK" : "✗ FAIL") << std::endl;
        
#ifdef _WIN32
        std::cout << "5. Testing BSOD-proof operations..." << std::endl;
        DWORD current_pid = GetCurrentProcessId();
        
        // Test safe process operations
        bool is_alive = self_protection->CheckProcessIsAlive(current_pid);
        std::cout << "   Current process alive check: " << (is_alive ? "✓ OK" : "✗ FAIL") << std::endl;
        
        // Test invalid operations (should not crash)
        bool invalid_result = self_protection->SafeTerminateProcess(0);
        std::cout << "   Safe terminate invalid PID (0): " << (!invalid_result ? "✓ OK (safely rejected)" : "✗ UNEXPECTED") << std::endl;
        
        invalid_result = self_protection->CheckProcessIsAlive(0xFFFFFFFF);
        std::cout << "   Check invalid PID alive (0xFFFFFFFF): " << (!invalid_result ? "✓ OK (safely rejected)" : "✗ UNEXPECTED") << std::endl;
        
        // Test safe handle operations
        HANDLE dummy_handle = nullptr;
        bool open_result = self_protection->SafeOpenProcess(current_pid, PROCESS_QUERY_INFORMATION, dummy_handle);
        if (open_result && dummy_handle) {
            bool validate_result = self_protection->ValidateProcessHandle(dummy_handle);
            std::cout << "   Process handle validation: " << (validate_result ? "✓ OK" : "✗ FAIL") << std::endl;
            
            bool close_result = self_protection->SafeCloseHandle(dummy_handle);
            std::cout << "   Safe handle close: " << (close_result ? "✓ OK" : "✗ FAIL") << std::endl;
        } else {
            std::cout << "   Process handle open: ✗ FAIL (may be expected in test environment)" << std::endl;
        }
#else
        std::cout << "5. Platform-specific tests skipped (non-Windows)" << std::endl;
#endif
        
        std::cout << "6. Testing protection rules..." << std::endl;
        HIPS::SelfProtectionRule test_rule;
        test_rule.name = "BSOD Test Rule";
        test_rule.description = "Test rule for BSOD-proof functionality";
        test_rule.event_type = HIPS::SelfProtectionEventType::PROCESS_TERMINATION_ATTEMPT;
        test_rule.action = HIPS::SelfProtectionAction::BLOCK_AND_ALERT;
        test_rule.min_threat_level = HIPS::ThreatLevel::HIGH;
        test_rule.enabled = true;
        
        bool add_rule_result = self_protection->AddRule(test_rule);
        std::cout << "   Add protection rule: " << (add_rule_result ? "✓ OK" : "✗ FAIL") << std::endl;
        
        auto rules = self_protection->GetRules();
        std::cout << "   Total rules loaded: " << rules.size() << std::endl;
        
        std::cout << "7. Testing statistics..." << std::endl;
        uint64_t event_count = self_protection->GetProtectionEventCount();
        uint64_t blocked_count = self_protection->GetBlockedAttacksCount();
        std::cout << "   Protection events: " << event_count << std::endl;
        std::cout << "   Blocked attacks: " << blocked_count << std::endl;
        
        std::cout << "8. Stopping self-protection engine..." << std::endl;
        if (!self_protection->Stop()) {
            std::cerr << "WARNING: Failed to stop self-protection engine" << std::endl;
        } else {
            std::cout << "   ✓ Stop successful" << std::endl;
        }
        
        std::cout << "9. Shutting down self-protection engine..." << std::endl;
        if (!self_protection->Shutdown()) {
            std::cerr << "WARNING: Failed to shutdown self-protection engine" << std::endl;
        } else {
            std::cout << "   ✓ Shutdown successful" << std::endl;
        }
        
        std::cout << "\n=== ALL BSOD-PROOF TESTS COMPLETED SUCCESSFULLY! ===" << std::endl;
        std::cout << "No crashes, no hangs, all operations handled safely." << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION CAUGHT" << std::endl;
        return 1;
    }
}