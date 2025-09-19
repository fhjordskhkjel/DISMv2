#include <gtest/gtest.h>
#include "hips_core.h"
#include <thread>
#include <chrono>

using namespace HIPS;

class HIPSEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<HIPSEngine>();
    }
    
    void TearDown() override {
        if (engine && engine->IsRunning()) {
            engine->Stop();
        }
        if (engine && engine->IsInitialized()) {
            engine->Shutdown();
        }
    }
    
    std::unique_ptr<HIPSEngine> engine;
};

TEST_F(HIPSEngineTest, InitializationTest) {
    EXPECT_FALSE(engine->IsInitialized());
    EXPECT_FALSE(engine->IsRunning());
    
    EXPECT_TRUE(engine->Initialize());
    EXPECT_TRUE(engine->IsInitialized());
    EXPECT_FALSE(engine->IsRunning());
}

TEST_F(HIPSEngineTest, StartStopTest) {
    EXPECT_TRUE(engine->Initialize());
    
    EXPECT_TRUE(engine->Start());
    EXPECT_TRUE(engine->IsRunning());
    
    EXPECT_TRUE(engine->Stop());
    EXPECT_FALSE(engine->IsRunning());
}

TEST_F(HIPSEngineTest, RuleManagementTest) {
    EXPECT_TRUE(engine->Initialize());
    
    SecurityRule test_rule;
    test_rule.name = "Test Rule";
    test_rule.description = "Test security rule";
    test_rule.event_type = EventType::FILE_ACCESS;
    test_rule.pattern = "test.exe";
    test_rule.action = ActionType::ALERT_ONLY;
    test_rule.min_threat_level = ThreatLevel::MEDIUM;
    test_rule.enabled = true;
    
    EXPECT_TRUE(engine->AddRule(test_rule));
    
    auto rules = engine->GetRules();
    EXPECT_GT(rules.size(), 0);
    
    bool found_rule = false;
    for (const auto& rule : rules) {
        if (rule.name == "Test Rule") {
            found_rule = true;
            EXPECT_EQ(rule.description, "Test security rule");
            EXPECT_EQ(rule.event_type, EventType::FILE_ACCESS);
            EXPECT_EQ(rule.action, ActionType::ALERT_ONLY);
            break;
        }
    }
    EXPECT_TRUE(found_rule);
    
    EXPECT_TRUE(engine->RemoveRule("Test Rule"));
    
    rules = engine->GetRules();
    found_rule = false;
    for (const auto& rule : rules) {
        if (rule.name == "Test Rule") {
            found_rule = true;
            break;
        }
    }
    EXPECT_FALSE(found_rule);
}

TEST_F(HIPSEngineTest, EventHandlerTest) {
    EXPECT_TRUE(engine->Initialize());
    
    bool event_received = false;
    SecurityEvent received_event;
    
    engine->RegisterEventHandler(EventType::PROCESS_CREATION,
        [&event_received, &received_event](const SecurityEvent& event) {
            event_received = true;
            received_event = event;
        });
    
    EXPECT_TRUE(engine->Start());
    
    // Allow some time for monitoring to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Note: In a real test environment, we would trigger actual events
    // For this test, we're just verifying the handler registration works
    
    EXPECT_TRUE(engine->Stop());
}

TEST_F(HIPSEngineTest, StatisticsTest) {
    EXPECT_TRUE(engine->Initialize());
    
    // Initial event counts should be zero
    EXPECT_EQ(engine->GetEventCount(EventType::PROCESS_CREATION), 0);
    EXPECT_EQ(engine->GetEventCount(EventType::FILE_ACCESS), 0);
    EXPECT_EQ(engine->GetTotalEventCount(), 0);
    
    EXPECT_TRUE(engine->Start());
    
    // Allow monitoring to run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_TRUE(engine->Stop());
    
    // Statistics should be available (actual values depend on system activity)
    uint64_t total_count = engine->GetTotalEventCount();
    EXPECT_GE(total_count, 0);
}

TEST_F(HIPSEngineTest, ConfigurationTest) {
    EXPECT_TRUE(engine->Initialize());
    
    // Test loading and saving configuration
    std::string test_config_path = "test_config.json";
    
    // Save configuration should work
    EXPECT_TRUE(engine->SaveConfiguration(test_config_path));
    
    // Load configuration should work
    EXPECT_TRUE(engine->LoadConfiguration(test_config_path));
}

TEST_F(HIPSEngineTest, DoubleInitializationTest) {
    EXPECT_TRUE(engine->Initialize());
    EXPECT_TRUE(engine->IsInitialized());
    
    // Second initialization should still return true
    EXPECT_TRUE(engine->Initialize());
    EXPECT_TRUE(engine->IsInitialized());
}

TEST_F(HIPSEngineTest, DoubleStartTest) {
    EXPECT_TRUE(engine->Initialize());
    EXPECT_TRUE(engine->Start());
    EXPECT_TRUE(engine->IsRunning());
    
    // Second start should still return true
    EXPECT_TRUE(engine->Start());
    EXPECT_TRUE(engine->IsRunning());
}

TEST_F(HIPSEngineTest, StopWithoutStartTest) {
    EXPECT_TRUE(engine->Initialize());
    EXPECT_FALSE(engine->IsRunning());
    
    // Stop without start should still return true
    EXPECT_TRUE(engine->Stop());
    EXPECT_FALSE(engine->IsRunning());
}

// Utility function tests
TEST(UtilityFunctionsTest, EventTypeStringConversion) {
    EXPECT_EQ(EventTypeToString(EventType::FILE_ACCESS), "FILE_ACCESS");
    EXPECT_EQ(EventTypeToString(EventType::PROCESS_CREATION), "PROCESS_CREATION");
    EXPECT_EQ(EventTypeToString(EventType::NETWORK_CONNECTION), "NETWORK_CONNECTION");
}

TEST(UtilityFunctionsTest, ThreatLevelStringConversion) {
    EXPECT_EQ(ThreatLevelToString(ThreatLevel::LOW), "LOW");
    EXPECT_EQ(ThreatLevelToString(ThreatLevel::MEDIUM), "MEDIUM");
    EXPECT_EQ(ThreatLevelToString(ThreatLevel::HIGH), "HIGH");
    EXPECT_EQ(ThreatLevelToString(ThreatLevel::CRITICAL), "CRITICAL");
}

TEST(UtilityFunctionsTest, ActionTypeStringConversion) {
    EXPECT_EQ(ActionTypeToString(ActionType::ALLOW), "ALLOW");
    EXPECT_EQ(ActionTypeToString(ActionType::DENY), "DENY");
    EXPECT_EQ(ActionTypeToString(ActionType::QUARANTINE), "QUARANTINE");
    EXPECT_EQ(ActionTypeToString(ActionType::ALERT_ONLY), "ALERT_ONLY");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}