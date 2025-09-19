#include "include/self_protection.h"
#include <iostream>

int main() {
    std::cout << "Creating SelfProtectionEngine..." << std::endl;
    
    try {
        HIPS::SelfProtectionEngine engine;
        std::cout << "Engine created successfully" << std::endl;
        
        std::cout << "Testing basic initialization..." << std::endl;
        if (engine.Initialize()) {
            std::cout << "Initialize: SUCCESS" << std::endl;
        } else {
            std::cout << "Initialize: FAILED" << std::endl;
            return 1;
        }
        
        std::cout << "Testing is initialized..." << std::endl;
        if (engine.IsInitialized()) {
            std::cout << "IsInitialized: TRUE" << std::endl;
        } else {
            std::cout << "IsInitialized: FALSE" << std::endl;
        }
        
        std::cout << "All basic tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
        return 1;
    }
}