// Simple build test to verify PSF/WIM integration compiles
#include "CabHandler.h"
#include <iostream>

int test_main() {
    try {
        CabHandler handler;
        
        // Test package type detection
        std::string packageType;
        if (handler.detectPackageType("test.appx", packageType)) {
            std::cout << "Package type: " << packageType << std::endl;
        }
        
        // Test PSF support
        std::cout << "PSF support: " << (handler.isPsfFile("test.appx") ? "Yes" : "No") << std::endl;
        
        // Test WIM support
        std::cout << "WIM support: " << (handler.isWimFile("test.wim") ? "Yes" : "No") << std::endl;
        
        std::cout << "? PSF/WIM integration compiled successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}