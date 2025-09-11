// Build test file to identify CBS integration issues
#include "CabHandler.h"
#include "CbsManager.h"
#include "CbsIntegratedHandler.h"

int main() {
    // Test basic CBS functionality
    CabHandler handler;
    
    // Test CBS integration
    handler.enableCbsIntegration(true);
    
    // Test basic functionality
    std::string packageType;
    handler.detectPackageType("test.msu", packageType);
    
    return 0;
}