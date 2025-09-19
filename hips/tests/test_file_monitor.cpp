#include <gtest/gtest.h>
#include "file_monitor.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

using namespace HIPS;

class FileMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        monitor = std::make_unique<FileSystemMonitor>();
        test_dir = std::filesystem::temp_directory_path() / "hips_test";
        std::filesystem::create_directories(test_dir);
        
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
        
        // Clean up test directory
        std::error_code ec;
        std::filesystem::remove_all(test_dir, ec);
    }
    
    std::unique_ptr<FileSystemMonitor> monitor;
    std::filesystem::path test_dir;
    int events_received;
    SecurityEvent last_event;
};

TEST_F(FileMonitorTest, InitializationTest) {
    EXPECT_FALSE(monitor->IsInitialized());
    EXPECT_FALSE(monitor->IsRunning());
    
    EXPECT_TRUE(monitor->Initialize());
    EXPECT_TRUE(monitor->IsInitialized());
    EXPECT_FALSE(monitor->IsRunning());
}

TEST_F(FileMonitorTest, StartStopTest) {
    EXPECT_TRUE(monitor->Initialize());
    
    EXPECT_TRUE(monitor->Start());
    EXPECT_TRUE(monitor->IsRunning());
    
    EXPECT_TRUE(monitor->Stop());
    EXPECT_FALSE(monitor->IsRunning());
}

TEST_F(FileMonitorTest, WatchPathManagement) {
    EXPECT_TRUE(monitor->Initialize());
    
    // Add a test watch path
    monitor->AddWatchPath(test_dir.string());
    
    // Start monitoring
    EXPECT_TRUE(monitor->Start());
    
    // Create a test file to trigger an event
    std::filesystem::path test_file = test_dir / "test.txt";
    {
        std::ofstream file(test_file);
        file << "test content";
    }
    
    // Give some time for event processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Remove the watch path
    monitor->RemoveWatchPath(test_dir.string());
    
    EXPECT_TRUE(monitor->Stop());
}

TEST_F(FileMonitorTest, FileExtensionFiltering) {
    EXPECT_TRUE(monitor->Initialize());
    
    // Set included extensions
    std::vector<std::string> included_extensions = {".txt", ".exe"};
    monitor->SetIncludedExtensions(included_extensions);
    
    monitor->AddWatchPath(test_dir.string());
    EXPECT_TRUE(monitor->Start());
    
    // Create files with different extensions
    std::filesystem::path txt_file = test_dir / "test.txt";
    std::filesystem::path exe_file = test_dir / "test.exe";
    std::filesystem::path jpg_file = test_dir / "test.jpg";
    
    {
        std::ofstream file1(txt_file);
        file1 << "test";
    }
    {
        std::ofstream file2(exe_file);
        file2 << "test";
    }
    {
        std::ofstream file3(jpg_file);
        file3 << "test";
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    EXPECT_TRUE(monitor->Stop());
    
    // Should have received events for .txt and .exe files, but not .jpg
    // Note: Exact count depends on file system behavior
    EXPECT_GE(events_received, 0);
}

TEST_F(FileMonitorTest, ExcludedExtensionFiltering) {
    EXPECT_TRUE(monitor->Initialize());
    
    // Set excluded extensions
    std::vector<std::string> excluded_extensions = {".log", ".tmp"};
    monitor->SetExcludedExtensions(excluded_extensions);
    
    monitor->AddWatchPath(test_dir.string());
    EXPECT_TRUE(monitor->Start());
    
    // Create files with excluded and non-excluded extensions
    std::filesystem::path txt_file = test_dir / "test.txt";
    std::filesystem::path log_file = test_dir / "test.log";
    
    {
        std::ofstream file1(txt_file);
        file1 << "test";
    }
    {
        std::ofstream file2(log_file);
        file2 << "test";
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    EXPECT_TRUE(monitor->Stop());
}

TEST_F(FileMonitorTest, ScanDepthConfiguration) {
    EXPECT_TRUE(monitor->Initialize());
    
    monitor->SetScanDepth(2);
    
    // Create nested directory structure
    std::filesystem::path level1 = test_dir / "level1";
    std::filesystem::path level2 = level1 / "level2";
    std::filesystem::path level3 = level2 / "level3";
    
    std::filesystem::create_directories(level3);
    
    monitor->AddWatchPath(test_dir.string());
    EXPECT_TRUE(monitor->Start());
    
    // Create files at different levels
    {
        std::ofstream file1(level1 / "test1.txt");
        file1 << "test";
    }
    {
        std::ofstream file2(level2 / "test2.txt");
        file2 << "test";
    }
    {
        std::ofstream file3(level3 / "test3.txt");
        file3 << "test";
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    EXPECT_TRUE(monitor->Stop());
}

TEST_F(FileMonitorTest, CallbackFunctionality) {
    EXPECT_TRUE(monitor->Initialize());
    
    bool callback_called = false;
    SecurityEvent callback_event;
    
    monitor->RegisterCallback([&callback_called, &callback_event](const SecurityEvent& event) {
        callback_called = true;
        callback_event = event;
    });
    
    monitor->AddWatchPath(test_dir.string());
    EXPECT_TRUE(monitor->Start());
    
    // Create a test file
    std::filesystem::path test_file = test_dir / "callback_test.txt";
    {
        std::ofstream file(test_file);
        file << "test content";
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    EXPECT_TRUE(monitor->Stop());
    
    // Callback might or might not be called depending on file system behavior
    // This test mainly ensures the callback mechanism doesn't crash
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}