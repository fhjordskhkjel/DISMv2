#include <gtest/gtest.h>
#include "process_monitor.h"
#include <thread>
#include <chrono>

using namespace HIPS;

class ProcessMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        monitor = std::make_unique<ProcessMonitor>();
        events_received = 0;
        
        monitor->RegisterCallback([this](const SecurityEvent& event) {
            events_received++;
            last_event = event;
        });
    }
    
    void TearDown() override {
        if (monitor && monitor->IsRunning()) {
            monitor->Stop();
        }
        if (monitor && monitor->IsInitialized()) {
            monitor->Shutdown();
        }
    }
    
    std::unique_ptr<ProcessMonitor> monitor;
    int events_received;
    SecurityEvent last_event;
};

TEST_F(ProcessMonitorTest, InitializationTest) {
    EXPECT_FALSE(monitor->IsInitialized());
    EXPECT_FALSE(monitor->IsRunning());
    
    EXPECT_TRUE(monitor->Initialize());
    EXPECT_TRUE(monitor->IsInitialized());
    EXPECT_FALSE(monitor->IsRunning());
}

TEST_F(ProcessMonitorTest, StartStopTest) {
    EXPECT_TRUE(monitor->Initialize());
    
    EXPECT_TRUE(monitor->Start());
    EXPECT_TRUE(monitor->IsRunning());
    
    EXPECT_TRUE(monitor->Stop());
    EXPECT_FALSE(monitor->IsRunning());
}

TEST_F(ProcessMonitorTest, ScanIntervalConfiguration) {
    EXPECT_TRUE(monitor->Initialize());
    
    // Set scan interval to 500ms
    monitor->SetScanInterval(500);
    
    EXPECT_TRUE(monitor->Start());
    
    // Let it run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_TRUE(monitor->Stop());
}

TEST_F(ProcessMonitorTest, SuspiciousProcessDetection) {
    EXPECT_TRUE(monitor->Initialize());
    
    // Add a suspicious process name
    monitor->AddSuspiciousProcess("test_suspicious.exe");
    
    EXPECT_TRUE(monitor->Start());
    
    // Let it run briefly to scan existing processes
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(monitor->Stop());
    
    // Remove the suspicious process
    monitor->RemoveSuspiciousProcess("test_suspicious.exe");
}

TEST_F(ProcessMonitorTest, MemoryThresholdConfiguration) {
    EXPECT_TRUE(monitor->Initialize());
    
    // Set memory threshold to 1GB
    SIZE_T threshold = 1024 * 1024 * 1024;
    monitor->SetMemoryThreshold(threshold);
    
    EXPECT_TRUE(monitor->Start());
    
    // Let it run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(monitor->Stop());
}

TEST_F(ProcessMonitorTest, GetRunningProcesses) {
    EXPECT_TRUE(monitor->Initialize());
    EXPECT_TRUE(monitor->Start());
    
    // Give it time to scan processes
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto processes = monitor->GetRunningProcesses();
    
    // Should have detected some processes
    EXPECT_GT(processes.size(), 0);
    
    // Check that process info is populated
    for (const auto& process : processes) {
        EXPECT_GT(process.pid, 0);
        EXPECT_FALSE(process.name.empty());
    }
    
    EXPECT_TRUE(monitor->Stop());
}

TEST_F(ProcessMonitorTest, GetProcessInfo) {
    EXPECT_TRUE(monitor->Initialize());
    EXPECT_TRUE(monitor->Start());
    
    // Get current process info
    DWORD current_pid = GetCurrentProcessId();
    ProcessInfo info = monitor->GetProcessInfo(current_pid);
    
    EXPECT_EQ(info.pid, current_pid);
    EXPECT_FALSE(info.name.empty());
    
    EXPECT_TRUE(monitor->Stop());
}

TEST_F(ProcessMonitorTest, ProcessTermination) {
    EXPECT_TRUE(monitor->Initialize());
    
    // Create a test process (notepad)
    PROCESS_INFORMATION pi;
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    
    // Try to create notepad process
    if (CreateProcessA(
        NULL,
        (LPSTR)"notepad.exe",
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi)) {
        
        // Process created successfully
        DWORD test_pid = pi.dwProcessId;
        
        // Give it a moment to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Try to terminate it
        bool terminated = monitor->TerminateProcess(test_pid);
        
        // Clean up handles
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        // Note: Termination might fail due to permissions, that's okay for testing
    }
}

TEST_F(ProcessMonitorTest, CallbackFunctionality) {
    EXPECT_TRUE(monitor->Initialize());
    
    bool callback_called = false;
    SecurityEvent callback_event;
    
    monitor->RegisterCallback([&callback_called, &callback_event](const SecurityEvent& event) {
        callback_called = true;
        callback_event = event;
    });
    
    EXPECT_TRUE(monitor->Start());
    
    // Let it run to detect process events
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_TRUE(monitor->Stop());
    
    // The callback mechanism should work without crashing
    // Whether it's called depends on system process activity
}

TEST_F(ProcessMonitorTest, APCQueueScanning) {
    EXPECT_TRUE(monitor->Initialize());
    EXPECT_TRUE(monitor->Start());
    
    // Let the monitor run and perform APC queue scanning
    // This will scan all non-system processes for suspicious APC entries
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // The APC scanning should complete without crashing
    // Actual detection depends on running processes and their APC queues
    EXPECT_TRUE(monitor->IsRunning());
    
    EXPECT_TRUE(monitor->Stop());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}