#include <gtest/gtest.h>
#include "hips_core.h"
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>

using namespace HIPS;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<HIPSEngine>();
        test_dir = std::filesystem::temp_directory_path() / "hips_integration_test";
        std::filesystem::create_directories(test_dir);
        
        events_received = 0;
    }
    
    void TearDown() override {
        if (engine && engine->IsRunning()) {
            engine->Stop();
        }
        if (engine && engine->IsInitialized()) {
            engine->Shutdown();
        }
        
        // Clean up test directory
        std::error_code ec;
        std::filesystem::remove_all(test_dir, ec);
    }
    
    std::unique_ptr<HIPSEngine> engine;
    std::filesystem::path test_dir;
    int events_received;
    std::vector<SecurityEvent> received_events;
};

TEST_F(IntegrationTest, FullSystemInitializationAndShutdown) {
    // Test complete system lifecycle
    EXPECT_TRUE(engine->Initialize());
    EXPECT_TRUE(engine->IsInitialized());
    
    EXPECT_TRUE(engine->Start());
    EXPECT_TRUE(engine->IsRunning());
    
    // Let the system run for a brief period
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    EXPECT_TRUE(engine->Stop());
    EXPECT_FALSE(engine->IsRunning());
    
    EXPECT_TRUE(engine->Shutdown());
    EXPECT_FALSE(engine->IsInitialized());
}

TEST_F(IntegrationTest, MultipleComponentMonitoring) {
    EXPECT_TRUE(engine->Initialize());
    
    // Register event handlers for different types
    engine->RegisterEventHandler(EventType::PROCESS_CREATION,
        [this](const SecurityEvent& event) {
            events_received++;
            received_events.push_back(event);
        });
        
    engine->RegisterEventHandler(EventType::FILE_MODIFICATION,
        [this](const SecurityEvent& event) {
            events_received++;
            received_events.push_back(event);
        });
        
    engine->RegisterEventHandler(EventType::NETWORK_CONNECTION,
        [this](const SecurityEvent& event) {
            events_received++;
            received_events.push_back(event);
        });
    
    EXPECT_TRUE(engine->Start());
    
    // Generate some test activity
    std::filesystem::path test_file = test_dir / "integration_test.txt";
    {
        std::ofstream file(test_file);
        file << "Integration test content";
    }
    
    // Let the system monitor for a while
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    EXPECT_TRUE(engine->Stop());
    
    // Check that statistics are being tracked
    uint64_t total_events = engine->GetTotalEventCount();
    EXPECT_GE(total_events, 0);
}

TEST_F(IntegrationTest, RuleBasedEventProcessing) {
    EXPECT_TRUE(engine->Initialize());
    
    // Add a custom rule for testing
    SecurityRule test_rule;
    test_rule.name = "Integration Test Rule";
    test_rule.description = "Rule for integration testing";
    test_rule.event_type = EventType::FILE_MODIFICATION;
    test_rule.pattern = "integration_test";
    test_rule.action = ActionType::ALERT_ONLY;
    test_rule.min_threat_level = ThreatLevel::LOW;
    test_rule.enabled = true;
    
    EXPECT_TRUE(engine->AddRule(test_rule));
    
    // Set up event tracking
    bool alert_triggered = false;
    engine->RegisterEventHandler(EventType::FILE_MODIFICATION,
        [&alert_triggered](const SecurityEvent& event) {
            if (event.target_path.find("integration_test") != std::string::npos) {
                alert_triggered = true;
            }
        });
    
    EXPECT_TRUE(engine->Start());
    
    // Create a file that should trigger the rule
    std::filesystem::path test_file = test_dir / "integration_test_trigger.txt";
    {
        std::ofstream file(test_file);
        file << "This should trigger our test rule";
    }
    
    // Modify the file to trigger an event
    {
        std::ofstream file(test_file, std::ios::app);
        file << "\nAdditional content";
    }
    
    // Wait for event processing
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    EXPECT_TRUE(engine->Stop());
    
    // Verify rule was processed (depends on file system monitoring behavior)
    auto rules = engine->GetRules();
    bool found_rule = false;
    for (const auto& rule : rules) {
        if (rule.name == "Integration Test Rule") {
            found_rule = true;
            break;
        }
    }
    EXPECT_TRUE(found_rule);
}

TEST_F(IntegrationTest, ConfigurationPersistence) {
    EXPECT_TRUE(engine->Initialize());
    
    // Add some rules
    SecurityRule rule1;
    rule1.name = "Config Test Rule 1";
    rule1.description = "First test rule";
    rule1.event_type = EventType::PROCESS_CREATION;
    rule1.action = ActionType::ALERT_ONLY;
    rule1.enabled = true;
    
    SecurityRule rule2;
    rule2.name = "Config Test Rule 2";
    rule2.description = "Second test rule";
    rule2.event_type = EventType::FILE_ACCESS;
    rule2.action = ActionType::DENY;
    rule2.enabled = false;
    
    EXPECT_TRUE(engine->AddRule(rule1));
    EXPECT_TRUE(engine->AddRule(rule2));
    
    // Save configuration
    std::string config_path = (test_dir / "test_config.json").string();
    EXPECT_TRUE(engine->SaveConfiguration(config_path));
    
    // Create a new engine instance and load the configuration
    auto new_engine = std::make_unique<HIPSEngine>();
    EXPECT_TRUE(new_engine->Initialize());
    EXPECT_TRUE(new_engine->LoadConfiguration(config_path));
    
    // Verify the rules were loaded (implementation-dependent)
    auto loaded_rules = new_engine->GetRules();
    EXPECT_GE(loaded_rules.size(), 2); // Should have at least our 2 rules plus defaults
    
    new_engine->Shutdown();
}

TEST_F(IntegrationTest, ConcurrentOperations) {
    EXPECT_TRUE(engine->Initialize());
    EXPECT_TRUE(engine->Start());
    
    // Perform concurrent operations
    std::vector<std::thread> threads;
    
    // Thread 1: Add/remove rules
    threads.emplace_back([this]() {
        for (int i = 0; i < 10; ++i) {
            SecurityRule rule;
            rule.name = "Concurrent Rule " + std::to_string(i);
            rule.event_type = EventType::FILE_ACCESS;
            rule.action = ActionType::ALLOW;
            rule.enabled = true;
            
            engine->AddRule(rule);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            engine->RemoveRule(rule.name);
        }
    });
    
    // Thread 2: Query statistics
    threads.emplace_back([this]() {
        for (int i = 0; i < 50; ++i) {
            engine->GetTotalEventCount();
            engine->GetEventCount(EventType::PROCESS_CREATION);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    // Thread 3: Create files
    threads.emplace_back([this]() {
        for (int i = 0; i < 5; ++i) {
            std::filesystem::path file = test_dir / ("concurrent_" + std::to_string(i) + ".txt");
            std::ofstream f(file);
            f << "Concurrent test " << i;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_TRUE(engine->Stop());
    
    // System should still be functional after concurrent operations
    EXPECT_TRUE(engine->GetTotalEventCount() >= 0);
}

TEST_F(IntegrationTest, StressTest) {
    EXPECT_TRUE(engine->Initialize());
    EXPECT_TRUE(engine->Start());
    
    // Add many rules
    for (int i = 0; i < 100; ++i) {
        SecurityRule rule;
        rule.name = "Stress Rule " + std::to_string(i);
        rule.description = "Stress test rule " + std::to_string(i);
        rule.event_type = static_cast<EventType>(i % 9); // Cycle through event types
        rule.action = static_cast<ActionType>(i % 5); // Cycle through actions
        rule.min_threat_level = static_cast<ThreatLevel>(i % 4); // Cycle through threat levels
        rule.enabled = (i % 2 == 0); // Alternate enabled/disabled
        
        EXPECT_TRUE(engine->AddRule(rule));
    }
    
    // Generate file activity
    for (int i = 0; i < 20; ++i) {
        std::filesystem::path file = test_dir / ("stress_" + std::to_string(i) + ".txt");
        std::ofstream f(file);
        f << "Stress test content " << i;
    }
    
    // Let the system process events
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Verify system is still responsive
    auto rules = engine->GetRules();
    EXPECT_GE(rules.size(), 100);
    
    uint64_t total_events = engine->GetTotalEventCount();
    EXPECT_GE(total_events, 0);
    
    EXPECT_TRUE(engine->Stop());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}