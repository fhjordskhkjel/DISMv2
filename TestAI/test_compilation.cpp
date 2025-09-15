#include "WimgApiWrapper.h"
#include "PsfWimHandler.h"
#include <iostream>

// Simple test to validate that the headers compile correctly
int main() {
    std::cout << "Testing WIMGAPI Integration Compilation..." << std::endl;
    
    // Test WimgApiWrapper instantiation
    WimgApiWrapper wrapper;
    std::cout << "WimgApiWrapper instantiated successfully" << std::endl;
    
    // Test PsfWimHandler instantiation
    PsfWimHandler handler;
    std::cout << "PsfWimHandler instantiated successfully" << std::endl;
    
    // Test static method
    bool available = PsfWimHandler::isWimgapiAvailable();
    std::cout << "WIMGAPI available: " << (available ? "Yes" : "No") << std::endl;
    
    // Test package type detection
    PackageType type;
    bool detected = PsfWimHandler::detectPackageType("test.wim", type);
    std::cout << "Package type detection: " << (detected ? "Working" : "Failed") << std::endl;
    
    std::cout << "All compilation tests passed!" << std::endl;
    return 0;
}