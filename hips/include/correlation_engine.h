/*
 * Correlation Engine for HIPS
 * 
 * Detects possibly correlated security events to identify
 * multi-stage attacks and attack chains.
 */

#ifndef CORRELATION_ENGINE_H
#define CORRELATION_ENGINE_H

#include "hips_core.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <memory>
#include <mutex>
#include <chrono>

namespace HIPS {

// Correlation types
enum class CorrelationType {
    PROCESS_BASED,      // Events from same process
    TIME_BASED,         // Events in time window
    TARGET_BASED,       // Events targeting same file/registry
    SEQUENCE_BASED,     // Events matching known attack patterns
    THREAT_ESCALATION   // Increasing threat levels
};

// Correlated event group
struct CorrelatedEventGroup {
    std::string correlation_id;
    CorrelationType type;
    std::vector<SecurityEvent> events;
    ThreatLevel combined_threat_level;
    double correlation_score;
    SYSTEMTIME first_event_time;
    SYSTEMTIME last_event_time;
    std::string description;
    std::unordered_map<std::string, std::string> metadata;
};

// Correlation configuration
struct CorrelationConfig {
    // Time window for correlation (in seconds)
    int time_window_seconds = 60;
    
    // Minimum events to trigger correlation
    int min_events_for_correlation = 3;
    
    // Minimum correlation score threshold
    double min_correlation_score = 0.6;
    
    // Maximum events to track per process
    int max_events_per_process = 100;
    
    // Maximum correlation groups to maintain
    int max_correlation_groups = 1000;
    
    // Enable specific correlation types
    bool enable_process_correlation = true;
    bool enable_time_correlation = true;
    bool enable_target_correlation = true;
    bool enable_sequence_correlation = true;
    bool enable_threat_escalation = true;
};

// Event tracking structure for correlation
struct TrackedEvent {
    SecurityEvent event;
    std::chrono::system_clock::time_point timestamp;
};

class CorrelationEngine {
public:
    CorrelationEngine();
    ~CorrelationEngine();

    // Initialization
    bool Initialize();
    bool Initialize(const CorrelationConfig& config);
    void Shutdown();

    // Event processing
    void ProcessEvent(const SecurityEvent& event);
    
    // Correlation detection
    std::vector<CorrelatedEventGroup> DetectCorrelations();
    std::vector<CorrelatedEventGroup> GetActiveCorrelations() const;
    
    // Configuration
    void SetConfiguration(const CorrelationConfig& config);
    CorrelationConfig GetConfiguration() const;
    
    // Statistics
    uint64_t GetProcessedEventCount() const;
    uint64_t GetCorrelationCount() const;
    uint64_t GetActiveCorrelationCount() const;
    
    // Callbacks for correlation alerts
    using CorrelationCallback = std::function<void(const CorrelatedEventGroup&)>;
    void RegisterCorrelationCallback(CorrelationCallback callback);
    
    // Clear old correlations
    void ClearOldCorrelations();

private:
    // Configuration
    CorrelationConfig config_;
    mutable std::mutex config_mutex_;
    
    // Event tracking
    std::unordered_map<DWORD, std::deque<TrackedEvent>> process_events_;
    std::unordered_map<std::string, std::deque<TrackedEvent>> target_events_;
    std::deque<TrackedEvent> time_window_events_;
    mutable std::mutex events_mutex_;
    
    // Correlation results
    std::vector<CorrelatedEventGroup> active_correlations_;
    mutable std::mutex correlations_mutex_;
    
    // Statistics
    std::atomic<uint64_t> processed_event_count_;
    std::atomic<uint64_t> correlation_count_;
    
    // Callback
    CorrelationCallback correlation_callback_;
    std::mutex callback_mutex_;
    
    // Correlation detection methods
    void DetectProcessBasedCorrelations();
    void DetectTimeBasedCorrelations();
    void DetectTargetBasedCorrelations();
    void DetectSequenceBasedCorrelations();
    void DetectThreatEscalation();
    
    // Helper methods
    double CalculateCorrelationScore(const std::vector<SecurityEvent>& events, CorrelationType type);
    ThreatLevel CalculateCombinedThreatLevel(const std::vector<SecurityEvent>& events);
    bool IsCorrelationSignificant(const std::vector<SecurityEvent>& events, CorrelationType type);
    void AddCorrelationGroup(const CorrelatedEventGroup& group);
    void CleanupOldEvents();
    std::string GenerateCorrelationId();
    
    // Time utilities
    bool IsWithinTimeWindow(const std::chrono::system_clock::time_point& time1,
                           const std::chrono::system_clock::time_point& time2) const;
    
    // Pattern matching for sequence detection
    bool MatchesAttackPattern(const std::vector<SecurityEvent>& events);
    std::string DescribeAttackPattern(const std::vector<SecurityEvent>& events);
};

} // namespace HIPS

#endif // CORRELATION_ENGINE_H
