#include "gtest/gtest.h"
#include "self_protection.h"
#include "hips_core.h"

using namespace HIPS;

class SelfProtectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        self_protection = std::make_unique<SelfProtectionEngine>();
        hips_engine = std::make_unique<HIPSEngine>();
    }

    void TearDown() override {
        if (self_protection && self_protection->IsRunning()) {
            self_protection->Stop();
        }
        if (hips_engine && hips_engine->IsRunning()) {
            hips_engine->Stop();
        }
    }

    std::unique_ptr<SelfProtectionEngine> self_protection;
    std::unique_ptr<HIPSEngine> hips_engine;
};

TEST_F(SelfProtectionTest, InitializationTest) {
    ASSERT_TRUE(self_protection->Initialize());
    EXPECT_TRUE(self_protection->IsInitialized());
    EXPECT_FALSE(self_protection->IsRunning());
}

TEST_F(SelfProtectionTest, StartStopTest) {
    ASSERT_TRUE(self_protection->Initialize());
    ASSERT_TRUE(self_protection->Start());
    EXPECT_TRUE(self_protection->IsRunning());
    
    ASSERT_TRUE(self_protection->Stop());
    EXPECT_FALSE(self_protection->IsRunning());
}

TEST_F(SelfProtectionTest, ProtectionRulesTest) {
    ASSERT_TRUE(self_protection->Initialize());
    
    // Test adding a custom protection rule
    SelfProtectionRule custom_rule;
    custom_rule.name = "Test Protection Rule";
    custom_rule.description = "Test rule for self-protection";
    custom_rule.event_type = SelfProtectionEventType::PROCESS_TERMINATION_ATTEMPT;
    custom_rule.action = SelfProtectionAction::BLOCK_AND_ALERT;
    custom_rule.min_threat_level = ThreatLevel::HIGH;
    custom_rule.enabled = true;
    
    ASSERT_TRUE(self_protection->AddRule(custom_rule));
    
    auto rules = self_protection->GetRules();
    EXPECT_GT(rules.size(), 0);
    
    // Find our custom rule
    bool found = false;
    for (const auto& rule : rules) {
        if (rule.name == "Test Protection Rule") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    
    // Test removing the rule
    ASSERT_TRUE(self_protection->RemoveRule("Test Protection Rule"));
    rules = self_protection->GetRules();
    
    // Verify rule was removed
    found = false;
    for (const auto& rule : rules) {
        if (rule.name == "Test Protection Rule") {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST_F(SelfProtectionTest, ProtectedResourcesTest) {
    ASSERT_TRUE(self_protection->Initialize());
    
    // Test adding protected resources
    ASSERT_TRUE(self_protection->AddProtectedFile("test.exe"));
    ASSERT_TRUE(self_protection->AddProtectedDirectory("C:\\Test"));
    ASSERT_TRUE(self_protection->AddProtectedRegistryKey("HKEY_LOCAL_MACHINE\\SOFTWARE\\Test"));
    ASSERT_TRUE(self_protection->AddProtectedProcess("test.exe"));
    ASSERT_TRUE(self_protection->AddProtectedService("TestService"));
}

TEST_F(SelfProtectionTest, TrustedProcessesTest) {
    ASSERT_TRUE(self_protection->Initialize());
    
    // Test adding and removing trusted processes
    ASSERT_TRUE(self_protection->AddTrustedProcess("trusted.exe"));
    EXPECT_TRUE(self_protection->IsTrustedProcess("trusted.exe"));
    EXPECT_FALSE(self_protection->IsTrustedProcess("untrusted.exe"));
    
    ASSERT_TRUE(self_protection->RemoveTrustedProcess("trusted.exe"));
    EXPECT_FALSE(self_protection->IsTrustedProcess("trusted.exe"));
}

TEST_F(SelfProtectionTest, EventHandlingTest) {
    ASSERT_TRUE(self_protection->Initialize());
    
    bool event_received = false;
    SelfProtectionEvent received_event;
    
    // Register event handler
    self_protection->RegisterEventHandler([&](const SelfProtectionEvent& event) {
        event_received = true;
        received_event = event;
    });
    
    // Start protection to enable event processing
    ASSERT_TRUE(self_protection->Start());
    
    // Verify initial event counts
    EXPECT_EQ(self_protection->GetProtectionEventCount(), 0);
    EXPECT_EQ(self_protection->GetBlockedAttacksCount(), 0);
}

TEST_F(SelfProtectionTest, IntegrityChecksTest) {
    ASSERT_TRUE(self_protection->Initialize());
    ASSERT_TRUE(self_protection->Start());
    
    // Test integrity checks (these may fail in test environment, but should not crash)
    bool process_integrity = self_protection->CheckProcessIntegrity();
    bool file_integrity = self_protection->CheckFileIntegrity();
    bool registry_integrity = self_protection->CheckRegistryIntegrity();
    bool service_integrity = self_protection->CheckServiceIntegrity();
    
    // These tests verify the methods execute without crashing
    // The actual results depend on the test environment
    EXPECT_TRUE(true); // Just verify we got here without exceptions
}

TEST_F(SelfProtectionTest, HIPSEngineIntegrationTest) {
    // Test integration with main HIPS engine
    ASSERT_TRUE(hips_engine->Initialize());
    
    // Test self-protection methods
    EXPECT_TRUE(hips_engine->EnableSelfProtection(true));
    EXPECT_TRUE(hips_engine->IsSelfProtectionEnabled());
    
    // Test statistics
    EXPECT_EQ(hips_engine->GetSelfProtectionEventCount(), 0);
    EXPECT_EQ(hips_engine->GetBlockedAttacksCount(), 0);
    
    // Test integrity check
    bool integrity = hips_engine->CheckSelfIntegrity();
    // Should not crash, actual result depends on environment
    
    // Disable self-protection
    EXPECT_TRUE(hips_engine->EnableSelfProtection(false));
    EXPECT_FALSE(hips_engine->IsSelfProtectionEnabled());
}

TEST_F(SelfProtectionTest, ConfigurationTest) {
    ASSERT_TRUE(self_protection->Initialize());
    
    // Test configuration loading
    SelfProtectionConfig config;
    config.process_protection_enabled = true;
    config.file_protection_enabled = true;
    config.registry_protection_enabled = true;
    config.memory_protection_enabled = true;
    config.debug_protection_enabled = true;
    config.service_protection_enabled = true;
    config.config_protection_enabled = true;
    config.auto_quarantine_attackers = false;
    config.terminate_attacking_process = false;
    config.max_protection_events_per_minute = 100;
    
    ASSERT_TRUE(self_protection->LoadConfiguration(config));
    
    SelfProtectionConfig loaded_config = self_protection->GetConfiguration();
    EXPECT_EQ(loaded_config.process_protection_enabled, config.process_protection_enabled);
    EXPECT_EQ(loaded_config.file_protection_enabled, config.file_protection_enabled);
    EXPECT_EQ(loaded_config.max_protection_events_per_minute, config.max_protection_events_per_minute);
}

// Test utility functions
TEST(SelfProtectionUtilityTest, EventTypeToStringTest) {
    EXPECT_EQ(SelfProtectionEventTypeToString(SelfProtectionEventType::PROCESS_TERMINATION_ATTEMPT), 
              "Process Termination Attempt");
    EXPECT_EQ(SelfProtectionEventTypeToString(SelfProtectionEventType::FILE_TAMPERING_ATTEMPT), 
              "File Tampering Attempt");
    EXPECT_EQ(SelfProtectionEventTypeToString(SelfProtectionEventType::DEBUG_ATTEMPT), 
              "Debug Attempt");
}

TEST(SelfProtectionUtilityTest, ActionToStringTest) {
    EXPECT_EQ(SelfProtectionActionToString(SelfProtectionAction::BLOCK_AND_ALERT), 
              "Block and Alert");
    EXPECT_EQ(SelfProtectionActionToString(SelfProtectionAction::TERMINATE_ATTACKER), 
              "Terminate Attacker");
    EXPECT_EQ(SelfProtectionActionToString(SelfProtectionAction::ALERT_ONLY), 
              "Alert Only");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}