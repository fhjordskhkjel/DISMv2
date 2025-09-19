#include "hips_core.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace HIPS;

class HIPSApplication {
public:
    HIPSApplication() : hips_engine_(std::make_unique<HIPSEngine>()) {}
    
    bool Initialize() {
        std::cout << "Initializing Advanced HIPS System..." << std::endl;
        
        if (!hips_engine_->Initialize()) {
            std::cerr << "Failed to initialize HIPS engine" << std::endl;
            return false;
        }
        
        std::cout << "HIPS engine initialized successfully" << std::endl;
        return true;
    }
    
    bool Start() {
        std::cout << "Starting HIPS monitoring..." << std::endl;
        
        if (!hips_engine_->Start()) {
            std::cerr << "Failed to start HIPS engine" << std::endl;
            return false;
        }
        
        std::cout << "HIPS monitoring started successfully" << std::endl;
        return true;
    }
    
    void Run() {
        std::cout << "HIPS System is now running. Press Ctrl+C to stop." << std::endl;
        
        // Register event handlers
        hips_engine_->RegisterEventHandler(EventType::PROCESS_CREATION,
            [](const SecurityEvent& event) {
                std::cout << "NEW PROCESS: " << event.process_path 
                          << " (PID: " << event.process_id << ")" << std::endl;
            });
            
        hips_engine_->RegisterEventHandler(EventType::FILE_MODIFICATION,
            [](const SecurityEvent& event) {
                std::cout << "FILE MODIFIED: " << event.target_path 
                          << " by " << event.process_path << std::endl;
            });
        
        // Main monitoring loop
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Print statistics every 30 seconds
            static int counter = 0;
            if (++counter % 30 == 0) {
                PrintStatistics();
            }
        }
    }
    
    void Stop() {
        std::cout << "Stopping HIPS system..." << std::endl;
        hips_engine_->Stop();
        hips_engine_->Shutdown();
        std::cout << "HIPS system stopped" << std::endl;
    }
    
private:
    std::unique_ptr<HIPSEngine> hips_engine_;
    
    void PrintStatistics() {
        std::cout << "\n--- HIPS Statistics ---" << std::endl;
        std::cout << "Total Events: " << hips_engine_->GetTotalEventCount() << std::endl;
        std::cout << "Process Events: " << hips_engine_->GetEventCount(EventType::PROCESS_CREATION) << std::endl;
        std::cout << "File Events: " << hips_engine_->GetEventCount(EventType::FILE_MODIFICATION) << std::endl;
        std::cout << "Network Events: " << hips_engine_->GetEventCount(EventType::NETWORK_CONNECTION) << std::endl;
        std::cout << "Memory Events: " << hips_engine_->GetEventCount(EventType::MEMORY_INJECTION) << std::endl;
        std::cout << "Registry Events: " << hips_engine_->GetEventCount(EventType::REGISTRY_MODIFICATION) << std::endl;
        std::cout << "----------------------\n" << std::endl;
    }
};

// Global application instance for signal handling
std::unique_ptr<HIPSApplication> g_app;

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    switch (dwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            if (g_app) {
                g_app->Stop();
            }
            return TRUE;
        default:
            return FALSE;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "Advanced HIPS (Host Intrusion Prevention System) v1.0" << std::endl;
    std::cout << "Windows Enterprise Security Solution" << std::endl;
    std::cout << "=====================================================\n" << std::endl;
    
    // Set up console control handler
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    
    try {
        g_app = std::make_unique<HIPSApplication>();
        
        if (!g_app->Initialize()) {
            std::cerr << "Failed to initialize HIPS application" << std::endl;
            return 1;
        }
        
        if (!g_app->Start()) {
            std::cerr << "Failed to start HIPS application" << std::endl;
            return 1;
        }
        
        g_app->Run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}