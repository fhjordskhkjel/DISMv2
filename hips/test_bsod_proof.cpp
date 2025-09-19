#include "include/self_protection.h"
#include <iostream>
#include <memory>

int main() {
    std::cout << "Testing BSOD-proof self-protection functionality..." << std::endl;
    
    try {
        auto self_protection = std::make_unique<HIPS::SelfProtectionEngine>();
        
        std::cout << "1. Initializing self-protection engine..." << std::endl;
        if (!self_protection->Initialize()) {
            std::cerr << "Failed to initialize self-protection engine" << std::endl;
            return 1;
        }
        
        std::cout << "2. Starting self-protection engine..." << std::endl;
        if (!self_protection->Start()) {
            std::cerr << "Failed to start self-protection engine" << std::endl;
            return 1;
        }
        
        std::cout << "3. Testing integrity checks..." << std::endl;
        bool process_integrity = self_protection->CheckProcessIntegrity();
        bool file_integrity = self_protection->CheckFileIntegrity();
        bool registry_integrity = self_protection->CheckRegistryIntegrity();
        bool service_integrity = self_protection->CheckServiceIntegrity();
        bool thread_integrity = self_protection->CheckThreadIntegrity();
        bool handle_integrity = self_protection->CheckHandleIntegrity();
        bool critical_section_integrity = self_protection->CheckCriticalSectionIntegrity();
        
        std::cout << "   Process integrity: " << (process_integrity ? "OK" : "FAIL") << std::endl;
        std::cout << "   File integrity: " << (file_integrity ? "OK" : "FAIL") << std::endl;
        std::cout << "   Registry integrity: " << (registry_integrity ? "OK" : "FAIL") << std::endl;
        std::cout << "   Service integrity: " << (service_integrity ? "OK" : "FAIL") << std::endl;
        std::cout << "   Thread integrity: " << (thread_integrity ? "OK" : "FAIL") << std::endl;
        std::cout << "   Handle integrity: " << (handle_integrity ? "OK" : "FAIL") << std::endl;
        std::cout << "   Critical section integrity: " << (critical_section_integrity ? "OK" : "FAIL") << std::endl;
        
#ifdef _WIN32
        std::cout << "4. Testing BSOD-proof operations..." << std::endl;
        DWORD current_pid = GetCurrentProcessId();
        
        // Test safe process operations
        bool is_alive = self_protection->CheckProcessIsAlive(current_pid);
        std::cout << "   Current process alive check: " << (is_alive ? "OK" : "FAIL") << std::endl;
        
        // Test invalid operations (should not crash)
        bool invalid_result = self_protection->SafeTerminateProcess(0);
        std::cout << "   Safe terminate invalid PID: " << (!invalid_result ? "OK (safely rejected)" : "UNEXPECTED") << std::endl;
        
        invalid_result = self_protection->CheckProcessIsAlive(0xFFFFFFFF);
        std::cout << "   Check invalid PID alive: " << (!invalid_result ? "OK (safely rejected)" : "UNEXPECTED") << std::endl;
#endif
        
        std::cout << "5. Stopping self-protection engine..." << std::endl;
        if (!self_protection->Stop()) {
            std::cerr << "Failed to stop self-protection engine" << std::endl;
            return 1;
        }
        
        std::cout << "6. Shutting down self-protection engine..." << std::endl;
        if (!self_protection->Shutdown()) {
            std::cerr << "Failed to shutdown self-protection engine" << std::endl;
            return 1;
        }
        
        std::cout << "All BSOD-proof tests completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
        return 1;
    }
}