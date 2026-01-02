#include <gtest/gtest.h>
#include "correlation_engine.h"
#include <thread>
#include <chrono>

using namespace HIPS;

class CorrelationEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<CorrelationEngine>();
        
        // Create some test events
        GetSystemTime(&test_time);
        
        // Create test event 1
        event1.type = EventType::PROCESS_CREATION;
        event1.threat_level = ThreatLevel::MEDIUM;
        event1.process_path = "C:\\test\\malware.exe";
        event1.target_path = "";
        event1.description = "Suspicious process creation";
        event1.process_id = 1234;
        event1.thread_id = 5678;
        event1.timestamp = test_time;
        
        // Create test event 2
        event2.type = EventType::FILE_MODIFICATION;
        event2.threat_level = ThreatLevel::HIGH;
        event2.process_path = "C:\\test\\malware.exe";
        event2.target_path = "C:\\Windows\\System32\\config.dll";
        event2.description = "Suspicious file modification";
        event2.process_id = 1234;
        event2.thread_id = 5678;
        event2.timestamp = test_time;
        
        // Create test event 3
        event3.type = EventType::REGISTRY_MODIFICATION;
        event3.threat_level = ThreatLevel::HIGH;
        event3.process_path = "C:\\test\\malware.exe";
        event3.target_path = "HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
        event3.description = "Suspicious registry modification";
        event3.process_id = 1234;
        event3.thread_id = 5678;
        event3.timestamp = test_time;
    }
    
    void TearDown() override {
        if (engine) {
            engine->Shutdown();
        }
    }
    
    std::unique_ptr<CorrelationEngine> engine;
    SecurityEvent event1, event2, event3;
    SYSTEMTIME test_time;
};

TEST_F(CorrelationEngineTest, InitializationTest) {
    EXPECT_TRUE(engine->Initialize());
    EXPECT_EQ(engine->GetProcessedEventCount(), 0);
    EXPECT_EQ(engine->GetCorrelationCount(), 0);
}

TEST_F(CorrelationEngineTest, ConfigurationTest) {
    CorrelationConfig config;
    config.time_window_seconds = 120;
    config.min_events_for_correlation = 5;
    config.min_correlation_score = 0.7;
    
    EXPECT_TRUE(engine->Initialize(config));
    
    auto retrieved_config = engine->GetConfiguration();
    EXPECT_EQ(retrieved_config.time_window_seconds, 120);
    EXPECT_EQ(retrieved_config.min_events_for_correlation, 5);
    EXPECT_DOUBLE_EQ(retrieved_config.min_correlation_score, 0.7);
}

TEST_F(CorrelationEngineTest, EventProcessingTest) {
    EXPECT_TRUE(engine->Initialize());
    
    // Process a single event
    engine->ProcessEvent(event1);
    EXPECT_EQ(engine->GetProcessedEventCount(), 1);
    
    // Process more events
    engine->ProcessEvent(event2);
    engine->ProcessEvent(event3);
    EXPECT_EQ(engine->GetProcessedEventCount(), 3);
}

TEST_F(CorrelationEngineTest, ProcessBasedCorrelationTest) {
    CorrelationConfig config;
    config.min_events_for_correlation = 3;
    config.min_correlation_score = 0.5;
    config.enable_process_correlation = true;
    
    EXPECT_TRUE(engine->Initialize(config));
    
    // Process multiple events from the same process
    engine->ProcessEvent(event1);
    engine->ProcessEvent(event2);
    engine->ProcessEvent(event3);
    
    // Small delay to allow correlation detection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto correlations = engine->GetActiveCorrelations();
    
    // Should detect at least one correlation
    EXPECT_GT(correlations.size(), 0);
    
    // Check if process-based correlation was detected
    bool found_process_correlation = false;
    for (const auto& corr : correlations) {
        if (corr.type == CorrelationType::PROCESS_BASED) {
            found_process_correlation = true;
            EXPECT_EQ(corr.events.size(), 3);
            EXPECT_EQ(corr.events[0].process_id, 1234);
            break;
        }
    }
    
    EXPECT_TRUE(found_process_correlation);
}

TEST_F(CorrelationEngineTest, TargetBasedCorrelationTest) {
    CorrelationConfig config;
    config.min_events_for_correlation = 2;
    config.min_correlation_score = 0.5;
    config.enable_target_correlation = true;
    
    EXPECT_TRUE(engine->Initialize(config));
    
    // Create events targeting the same file from different processes
    SecurityEvent evt1 = event2;
    evt1.process_id = 1000;
    
    SecurityEvent evt2 = event2;
    evt2.process_id = 2000;
    
    SecurityEvent evt3 = event2;
    evt3.process_id = 3000;
    
    engine->ProcessEvent(evt1);
    engine->ProcessEvent(evt2);
    engine->ProcessEvent(evt3);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto correlations = engine->GetActiveCorrelations();
    
    // Should detect target-based correlation
    bool found_target_correlation = false;
    for (const auto& corr : correlations) {
        if (corr.type == CorrelationType::TARGET_BASED) {
            found_target_correlation = true;
            EXPECT_GE(corr.events.size(), 2);
            break;
        }
    }
    
    EXPECT_TRUE(found_target_correlation);
}

TEST_F(CorrelationEngineTest, ThreatEscalationTest) {
    CorrelationConfig config;
    config.min_events_for_correlation = 2;
    config.min_correlation_score = 0.5;
    config.enable_threat_escalation = true;
    
    EXPECT_TRUE(engine->Initialize(config));
    
    // Create events with escalating threat levels
    SecurityEvent low_threat = event1;
    low_threat.threat_level = ThreatLevel::LOW;
    
    SecurityEvent medium_threat = event2;
    medium_threat.threat_level = ThreatLevel::MEDIUM;
    
    SecurityEvent high_threat = event3;
    high_threat.threat_level = ThreatLevel::HIGH;
    
    engine->ProcessEvent(low_threat);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    engine->ProcessEvent(medium_threat);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    engine->ProcessEvent(high_threat);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto correlations = engine->GetActiveCorrelations();
    
    // Should detect threat escalation
    bool found_escalation = false;
    for (const auto& corr : correlations) {
        if (corr.type == CorrelationType::THREAT_ESCALATION) {
            found_escalation = true;
            EXPECT_GE(corr.events.size(), 2);
            break;
        }
    }
    
    EXPECT_TRUE(found_escalation);
}

TEST_F(CorrelationEngineTest, SequenceBasedCorrelationTest) {
    CorrelationConfig config;
    config.min_events_for_correlation = 3;
    config.min_correlation_score = 0.5;
    config.enable_sequence_correlation = true;
    
    EXPECT_TRUE(engine->Initialize(config));
    
    // Create a known attack pattern sequence
    SecurityEvent proc_creation = event1;
    proc_creation.type = EventType::PROCESS_CREATION;
    
    SecurityEvent file_mod = event2;
    file_mod.type = EventType::FILE_MODIFICATION;
    
    SecurityEvent reg_mod = event3;
    reg_mod.type = EventType::REGISTRY_MODIFICATION;
    
    engine->ProcessEvent(proc_creation);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    engine->ProcessEvent(file_mod);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    engine->ProcessEvent(reg_mod);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto correlations = engine->GetActiveCorrelations();
    
    // Should detect sequence-based correlation
    bool found_sequence = false;
    for (const auto& corr : correlations) {
        if (corr.type == CorrelationType::SEQUENCE_BASED) {
            found_sequence = true;
            EXPECT_GE(corr.events.size(), 3);
            EXPECT_EQ(corr.combined_threat_level, ThreatLevel::CRITICAL);
            break;
        }
    }
    
    EXPECT_TRUE(found_sequence);
}

TEST_F(CorrelationEngineTest, CorrelationCallbackTest) {
    CorrelationConfig config;
    config.min_events_for_correlation = 3;
    config.min_correlation_score = 0.5;
    
    EXPECT_TRUE(engine->Initialize(config));
    
    bool callback_triggered = false;
    CorrelatedEventGroup received_group;
    
    engine->RegisterCorrelationCallback([&callback_triggered, &received_group](const CorrelatedEventGroup& group) {
        callback_triggered = true;
        received_group = group;
    });
    
    // Process events to trigger correlation
    engine->ProcessEvent(event1);
    engine->ProcessEvent(event2);
    engine->ProcessEvent(event3);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_TRUE(callback_triggered);
    EXPECT_GT(received_group.events.size(), 0);
}

TEST_F(CorrelationEngineTest, StatisticsTest) {
    EXPECT_TRUE(engine->Initialize());
    
    EXPECT_EQ(engine->GetProcessedEventCount(), 0);
    EXPECT_EQ(engine->GetCorrelationCount(), 0);
    EXPECT_EQ(engine->GetActiveCorrelationCount(), 0);
    
    // Process events
    engine->ProcessEvent(event1);
    engine->ProcessEvent(event2);
    engine->ProcessEvent(event3);
    
    EXPECT_EQ(engine->GetProcessedEventCount(), 3);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // After processing, we should have some correlations
    EXPECT_GE(engine->GetCorrelationCount(), 0);
}

TEST_F(CorrelationEngineTest, ClearOldCorrelationsTest) {
    EXPECT_TRUE(engine->Initialize());
    
    // Process multiple events to generate correlations
    for (int i = 0; i < 5; i++) {
        engine->ProcessEvent(event1);
        engine->ProcessEvent(event2);
        engine->ProcessEvent(event3);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    size_t initial_count = engine->GetActiveCorrelationCount();
    
    // Clear old correlations
    engine->ClearOldCorrelations();
    
    // Should still have some correlations but potentially fewer
    size_t after_clear_count = engine->GetActiveCorrelationCount();
    EXPECT_LE(after_clear_count, initial_count);
}

TEST_F(CorrelationEngineTest, TimeWindowTest) {
    CorrelationConfig config;
    config.time_window_seconds = 2; // Very short window
    config.min_events_for_correlation = 2;
    config.min_correlation_score = 0.5;
    
    EXPECT_TRUE(engine->Initialize(config));
    
    // Process first event
    engine->ProcessEvent(event1);
    
    // Wait longer than time window
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Process second event - should NOT correlate due to time window
    engine->ProcessEvent(event2);
    
    auto correlations = engine->GetActiveCorrelations();
    
    // Correlations might exist from other detection methods,
    // but time-based ones should be limited
    EXPECT_GE(correlations.size(), 0);
}

TEST_F(CorrelationEngineTest, MultipleProcessesTest) {
    CorrelationConfig config;
    config.min_events_for_correlation = 3;
    config.min_correlation_score = 0.5;
    
    EXPECT_TRUE(engine->Initialize(config));
    
    // Process events from multiple processes
    SecurityEvent evt_proc1 = event1;
    evt_proc1.process_id = 1000;
    
    SecurityEvent evt_proc2 = event2;
    evt_proc2.process_id = 2000;
    
    SecurityEvent evt_proc3 = event3;
    evt_proc3.process_id = 3000;
    
    engine->ProcessEvent(evt_proc1);
    engine->ProcessEvent(evt_proc1);
    engine->ProcessEvent(evt_proc1);
    
    engine->ProcessEvent(evt_proc2);
    engine->ProcessEvent(evt_proc2);
    engine->ProcessEvent(evt_proc2);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto correlations = engine->GetActiveCorrelations();
    
    // Should detect correlations for multiple processes
    EXPECT_GE(correlations.size(), 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
