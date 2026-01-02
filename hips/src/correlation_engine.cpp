/*
 * Correlation Engine Implementation
 * 
 * Detects and tracks correlated security events to identify
 * potential attack chains and multi-stage threats.
 */

#include "correlation_engine.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace HIPS {

// Helper function to convert SYSTEMTIME to chrono time_point
static std::chrono::system_clock::time_point SystemTimeToTimePoint(const SYSTEMTIME& st) {
    std::tm tm = {};
    tm.tm_year = st.wYear - 1900;
    tm.tm_mon = st.wMonth - 1;
    tm.tm_mday = st.wDay;
    tm.tm_hour = st.wHour;
    tm.tm_min = st.wMinute;
    tm.tm_sec = st.wSecond;
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// Helper function to convert chrono time_point to SYSTEMTIME
static SYSTEMTIME TimePointToSystemTime(const std::chrono::system_clock::time_point& tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm* tm = std::localtime(&time);
    
    SYSTEMTIME st = {};
    st.wYear = static_cast<unsigned short>(tm->tm_year + 1900);
    st.wMonth = static_cast<unsigned short>(tm->tm_mon + 1);
    st.wDay = static_cast<unsigned short>(tm->tm_mday);
    st.wHour = static_cast<unsigned short>(tm->tm_hour);
    st.wMinute = static_cast<unsigned short>(tm->tm_min);
    st.wSecond = static_cast<unsigned short>(tm->tm_sec);
    st.wMilliseconds = 0;
    return st;
}

CorrelationEngine::CorrelationEngine()
    : processed_event_count_(0), correlation_count_(0) {
}

CorrelationEngine::~CorrelationEngine() {
    Shutdown();
}

bool CorrelationEngine::Initialize() {
    return Initialize(CorrelationConfig());
}

bool CorrelationEngine::Initialize(const CorrelationConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
    
    // Clear any existing data
    {
        std::lock_guard<std::mutex> events_lock(events_mutex_);
        process_events_.clear();
        target_events_.clear();
        time_window_events_.clear();
    }
    
    {
        std::lock_guard<std::mutex> corr_lock(correlations_mutex_);
        active_correlations_.clear();
    }
    
    processed_event_count_ = 0;
    correlation_count_ = 0;
    
    return true;
}

void CorrelationEngine::Shutdown() {
    std::lock_guard<std::mutex> events_lock(events_mutex_);
    std::lock_guard<std::mutex> corr_lock(correlations_mutex_);
    
    process_events_.clear();
    target_events_.clear();
    time_window_events_.clear();
    active_correlations_.clear();
}

void CorrelationEngine::ProcessEvent(const SecurityEvent& event) {
    auto now = std::chrono::system_clock::now();
    TrackedEvent tracked;
    tracked.event = event;
    tracked.timestamp = now;
    
    {
        std::lock_guard<std::mutex> lock(events_mutex_);
        
        // Add to time window
        time_window_events_.push_back(tracked);
        
        // Add to process-specific tracking
        auto& proc_events = process_events_[event.process_id];
        proc_events.push_back(tracked);
        
        // Limit events per process
        if (proc_events.size() > static_cast<size_t>(config_.max_events_per_process)) {
            proc_events.pop_front();
        }
        
        // Add to target-specific tracking
        if (!event.target_path.empty()) {
            auto& target_events = target_events_[event.target_path];
            target_events.push_back(tracked);
            
            // Limit events per target
            if (target_events.size() > static_cast<size_t>(config_.max_events_per_process)) {
                target_events.pop_front();
            }
        }
        
        // Cleanup old events from time window
        CleanupOldEvents();
    }
    
    processed_event_count_++;
    
    // Detect correlations after each event
    // Note: This is called synchronously for real-time detection. In high-volume
    // environments, consider implementing asynchronous batch processing or
    // periodic detection in a background thread. The current approach ensures
    // immediate correlation detection which is critical for security monitoring.
    // Performance impact: O(n*m) where n is tracked events per type and m is number
    // of active processes/targets. Typically <100 events * <50 processes = ~5000
    // operations, resulting in minimal overhead (~1-5ms on modern hardware).
    DetectCorrelations();
}

std::vector<CorrelatedEventGroup> CorrelationEngine::DetectCorrelations() {
    std::vector<CorrelatedEventGroup> new_correlations;
    
    // Run different correlation detection algorithms
    if (config_.enable_process_correlation) {
        DetectProcessBasedCorrelations();
    }
    
    if (config_.enable_time_correlation) {
        DetectTimeBasedCorrelations();
    }
    
    if (config_.enable_target_correlation) {
        DetectTargetBasedCorrelations();
    }
    
    if (config_.enable_sequence_correlation) {
        DetectSequenceBasedCorrelations();
    }
    
    if (config_.enable_threat_escalation) {
        DetectThreatEscalation();
    }
    
    // Return new correlations
    {
        std::lock_guard<std::mutex> lock(correlations_mutex_);
        new_correlations = active_correlations_;
    }
    
    return new_correlations;
}

void CorrelationEngine::DetectProcessBasedCorrelations() {
    std::lock_guard<std::mutex> lock(events_mutex_);
    
    for (auto& [process_id, events] : process_events_) {
        if (events.size() < static_cast<size_t>(config_.min_events_for_correlation)) {
            continue;
        }
        
        // Extract recent events within time window
        auto now = std::chrono::system_clock::now();
        std::vector<SecurityEvent> recent_events;
        
        for (const auto& tracked : events) {
            if (IsWithinTimeWindow(tracked.timestamp, now)) {
                recent_events.push_back(tracked.event);
            }
        }
        
        if (recent_events.size() >= static_cast<size_t>(config_.min_events_for_correlation) &&
            IsCorrelationSignificant(recent_events, CorrelationType::PROCESS_BASED)) {
            
            CorrelatedEventGroup group;
            group.correlation_id = GenerateCorrelationId();
            group.type = CorrelationType::PROCESS_BASED;
            group.events = recent_events;
            group.combined_threat_level = CalculateCombinedThreatLevel(recent_events);
            group.correlation_score = CalculateCorrelationScore(recent_events, CorrelationType::PROCESS_BASED);
            group.first_event_time = recent_events.front().timestamp;
            group.last_event_time = recent_events.back().timestamp;
            
            std::ostringstream desc;
            desc << "Multiple correlated events (" << recent_events.size() 
                 << ") detected from process " << process_id;
            group.description = desc.str();
            
            group.metadata["process_id"] = std::to_string(process_id);
            group.metadata["event_count"] = std::to_string(recent_events.size());
            
            AddCorrelationGroup(group);
        }
    }
}

void CorrelationEngine::DetectTimeBasedCorrelations() {
    std::lock_guard<std::mutex> lock(events_mutex_);
    
    if (time_window_events_.size() < static_cast<size_t>(config_.min_events_for_correlation)) {
        return;
    }
    
    // Look for bursts of high-threat events
    std::vector<SecurityEvent> high_threat_events;
    
    for (const auto& tracked : time_window_events_) {
        if (tracked.event.threat_level == ThreatLevel::HIGH || 
            tracked.event.threat_level == ThreatLevel::CRITICAL) {
            high_threat_events.push_back(tracked.event);
        }
    }
    
    if (high_threat_events.size() >= static_cast<size_t>(config_.min_events_for_correlation) &&
        IsCorrelationSignificant(high_threat_events, CorrelationType::TIME_BASED)) {
        
        CorrelatedEventGroup group;
        group.correlation_id = GenerateCorrelationId();
        group.type = CorrelationType::TIME_BASED;
        group.events = high_threat_events;
        group.combined_threat_level = CalculateCombinedThreatLevel(high_threat_events);
        group.correlation_score = CalculateCorrelationScore(high_threat_events, CorrelationType::TIME_BASED);
        group.first_event_time = high_threat_events.front().timestamp;
        group.last_event_time = high_threat_events.back().timestamp;
        
        std::ostringstream desc;
        desc << "Burst of " << high_threat_events.size() 
             << " high-threat events detected in time window";
        group.description = desc.str();
        
        group.metadata["event_count"] = std::to_string(high_threat_events.size());
        group.metadata["time_window"] = std::to_string(config_.time_window_seconds);
        
        AddCorrelationGroup(group);
    }
}

void CorrelationEngine::DetectTargetBasedCorrelations() {
    std::lock_guard<std::mutex> lock(events_mutex_);
    
    for (auto& [target, events] : target_events_) {
        if (events.size() < static_cast<size_t>(config_.min_events_for_correlation)) {
            continue;
        }
        
        // Extract recent events within time window
        auto now = std::chrono::system_clock::now();
        std::vector<SecurityEvent> recent_events;
        
        for (const auto& tracked : events) {
            if (IsWithinTimeWindow(tracked.timestamp, now)) {
                recent_events.push_back(tracked.event);
            }
        }
        
        if (recent_events.size() >= static_cast<size_t>(config_.min_events_for_correlation) &&
            IsCorrelationSignificant(recent_events, CorrelationType::TARGET_BASED)) {
            
            CorrelatedEventGroup group;
            group.correlation_id = GenerateCorrelationId();
            group.type = CorrelationType::TARGET_BASED;
            group.events = recent_events;
            group.combined_threat_level = CalculateCombinedThreatLevel(recent_events);
            group.correlation_score = CalculateCorrelationScore(recent_events, CorrelationType::TARGET_BASED);
            group.first_event_time = recent_events.front().timestamp;
            group.last_event_time = recent_events.back().timestamp;
            
            std::ostringstream desc;
            desc << "Multiple processes (" << recent_events.size() 
                 << " events) targeting same file/registry: " << target;
            group.description = desc.str();
            
            group.metadata["target"] = target;
            group.metadata["event_count"] = std::to_string(recent_events.size());
            
            AddCorrelationGroup(group);
        }
    }
}

void CorrelationEngine::DetectSequenceBasedCorrelations() {
    std::lock_guard<std::mutex> lock(events_mutex_);
    
    // Check time window events for known attack patterns
    if (time_window_events_.size() < static_cast<size_t>(config_.min_events_for_correlation)) {
        return;
    }
    
    std::vector<SecurityEvent> events;
    for (const auto& tracked : time_window_events_) {
        events.push_back(tracked.event);
    }
    
    if (MatchesAttackPattern(events)) {
        CorrelatedEventGroup group;
        group.correlation_id = GenerateCorrelationId();
        group.type = CorrelationType::SEQUENCE_BASED;
        group.events = events;
        group.combined_threat_level = ThreatLevel::CRITICAL;
        group.correlation_score = 0.9; // High score for pattern matches
        group.first_event_time = events.front().timestamp;
        group.last_event_time = events.back().timestamp;
        group.description = DescribeAttackPattern(events);
        
        group.metadata["pattern_type"] = "known_attack_sequence";
        group.metadata["event_count"] = std::to_string(events.size());
        
        AddCorrelationGroup(group);
    }
}

void CorrelationEngine::DetectThreatEscalation() {
    std::lock_guard<std::mutex> lock(events_mutex_);
    
    for (auto& [process_id, events] : process_events_) {
        if (events.size() < 2) {
            continue;
        }
        
        // Check if threat level is increasing
        bool escalating = false;
        ThreatLevel prev_level = ThreatLevel::LOW;
        std::vector<SecurityEvent> escalation_events;
        
        for (const auto& tracked : events) {
            if (static_cast<int>(tracked.event.threat_level) > static_cast<int>(prev_level)) {
                escalating = true;
                escalation_events.push_back(tracked.event);
            }
            prev_level = tracked.event.threat_level;
        }
        
        if (escalating && escalation_events.size() >= static_cast<size_t>(config_.min_events_for_correlation)) {
            CorrelatedEventGroup group;
            group.correlation_id = GenerateCorrelationId();
            group.type = CorrelationType::THREAT_ESCALATION;
            group.events = escalation_events;
            group.combined_threat_level = CalculateCombinedThreatLevel(escalation_events);
            group.correlation_score = 0.85; // High score for escalation
            group.first_event_time = escalation_events.front().timestamp;
            group.last_event_time = escalation_events.back().timestamp;
            
            std::ostringstream desc;
            desc << "Threat escalation detected from process " << process_id 
                 << " with " << escalation_events.size() << " escalating events";
            group.description = desc.str();
            
            group.metadata["process_id"] = std::to_string(process_id);
            group.metadata["escalation_type"] = "threat_level_increase";
            
            AddCorrelationGroup(group);
        }
    }
}

double CorrelationEngine::CalculateCorrelationScore(const std::vector<SecurityEvent>& events, 
                                                     CorrelationType type) {
    if (events.empty()) {
        return 0.0;
    }
    
    double score = 0.0;
    
    // Base score on event count
    score += std::min(static_cast<double>(events.size()) / 10.0, 0.3);
    
    // Add score for threat level
    int high_threat_count = 0;
    for (const auto& event : events) {
        if (event.threat_level == ThreatLevel::HIGH || 
            event.threat_level == ThreatLevel::CRITICAL) {
            high_threat_count++;
        }
    }
    score += (static_cast<double>(high_threat_count) / events.size()) * 0.4;
    
    // Add score based on correlation type
    switch (type) {
        case CorrelationType::PROCESS_BASED:
            score += 0.2; // Same process is significant
            break;
        case CorrelationType::TARGET_BASED:
            score += 0.25; // Same target is very significant
            break;
        case CorrelationType::SEQUENCE_BASED:
            score += 0.3; // Pattern match is highly significant
            break;
        case CorrelationType::THREAT_ESCALATION:
            score += 0.3; // Escalation is highly significant
            break;
        case CorrelationType::TIME_BASED:
            score += 0.15; // Time proximity is moderately significant
            break;
    }
    
    return std::min(score, 1.0);
}

ThreatLevel CorrelationEngine::CalculateCombinedThreatLevel(const std::vector<SecurityEvent>& events) {
    if (events.empty()) {
        return ThreatLevel::LOW;
    }
    
    // Find the highest threat level
    ThreatLevel max_level = ThreatLevel::LOW;
    int critical_count = 0;
    int high_count = 0;
    
    for (const auto& event : events) {
        if (static_cast<int>(event.threat_level) > static_cast<int>(max_level)) {
            max_level = event.threat_level;
        }
        
        if (event.threat_level == ThreatLevel::CRITICAL) {
            critical_count++;
        } else if (event.threat_level == ThreatLevel::HIGH) {
            high_count++;
        }
    }
    
    // If multiple high/critical threats, escalate
    if (critical_count >= 2 || (critical_count >= 1 && high_count >= 2)) {
        return ThreatLevel::CRITICAL;
    }
    
    if (high_count >= 3) {
        return ThreatLevel::CRITICAL;
    }
    
    // Escalate by one level if many events
    if (events.size() >= 5 && max_level != ThreatLevel::CRITICAL) {
        return static_cast<ThreatLevel>(static_cast<int>(max_level) + 1);
    }
    
    return max_level;
}

bool CorrelationEngine::IsCorrelationSignificant(const std::vector<SecurityEvent>& events, 
                                                  CorrelationType type) {
    double score = CalculateCorrelationScore(events, type);
    return score >= config_.min_correlation_score;
}

void CorrelationEngine::AddCorrelationGroup(const CorrelatedEventGroup& group) {
    std::lock_guard<std::mutex> lock(correlations_mutex_);
    
    // Check if similar correlation already exists
    // Note: This uses linear search which is O(n) where n is active correlations.
    // For most use cases with <1000 active correlations, this is acceptable.
    // For high-volume scenarios, consider using a hash-based index on
    // (type, process_id) for O(1) duplicate detection.
    bool duplicate = false;
    for (const auto& existing : active_correlations_) {
        if (existing.type == group.type && 
            existing.events.size() == group.events.size() &&
            !existing.events.empty() && !group.events.empty() &&
            existing.events.front().process_id == group.events.front().process_id) {
            duplicate = true;
            break;
        }
    }
    
    if (!duplicate) {
        active_correlations_.push_back(group);
        correlation_count_++;
        
        // Limit number of correlations
        if (active_correlations_.size() > static_cast<size_t>(config_.max_correlation_groups)) {
            active_correlations_.erase(active_correlations_.begin());
        }
        
        // Notify callback if registered
        {
            std::lock_guard<std::mutex> cb_lock(callback_mutex_);
            if (correlation_callback_) {
                correlation_callback_(group);
            }
        }
    }
}

void CorrelationEngine::CleanupOldEvents() {
    auto now = std::chrono::system_clock::now();
    
    // Clean up time window events
    while (!time_window_events_.empty()) {
        if (IsWithinTimeWindow(time_window_events_.front().timestamp, now)) {
            break;
        }
        time_window_events_.pop_front();
    }
}

std::string CorrelationEngine::GenerateCorrelationId() {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::ostringstream oss;
    oss << "CORR-" << timestamp << "-" << (counter++);
    return oss.str();
}

bool CorrelationEngine::IsWithinTimeWindow(const std::chrono::system_clock::time_point& time1,
                                            const std::chrono::system_clock::time_point& time2) const {
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(
        time2 > time1 ? time2 - time1 : time1 - time2);
    return diff.count() <= config_.time_window_seconds;
}

bool CorrelationEngine::MatchesAttackPattern(const std::vector<SecurityEvent>& events) {
    if (events.size() < 3) {
        return false;
    }
    
    // Pattern 1: Process creation -> File modification -> Registry modification
    bool has_process_creation = false;
    bool has_file_mod = false;
    bool has_registry_mod = false;
    
    for (const auto& event : events) {
        if (event.type == EventType::PROCESS_CREATION) {
            has_process_creation = true;
        } else if (event.type == EventType::FILE_MODIFICATION || 
                   event.type == EventType::FILE_DELETION) {
            has_file_mod = true;
        } else if (event.type == EventType::REGISTRY_MODIFICATION) {
            has_registry_mod = true;
        }
    }
    
    if (has_process_creation && has_file_mod && has_registry_mod) {
        return true;
    }
    
    // Pattern 2: Memory injection followed by file/registry changes
    bool has_memory_injection = false;
    for (const auto& event : events) {
        if (event.type == EventType::MEMORY_INJECTION) {
            has_memory_injection = true;
            break;
        }
    }
    
    if (has_memory_injection && (has_file_mod || has_registry_mod)) {
        return true;
    }
    
    return false;
}

std::string CorrelationEngine::DescribeAttackPattern(const std::vector<SecurityEvent>& events) {
    std::ostringstream desc;
    desc << "Known attack pattern detected: ";
    
    // Analyze the pattern
    bool has_process_creation = false;
    bool has_file_mod = false;
    bool has_registry_mod = false;
    bool has_memory_injection = false;
    
    for (const auto& event : events) {
        switch (event.type) {
            case EventType::PROCESS_CREATION:
                has_process_creation = true;
                break;
            case EventType::FILE_MODIFICATION:
            case EventType::FILE_DELETION:
                has_file_mod = true;
                break;
            case EventType::REGISTRY_MODIFICATION:
                has_registry_mod = true;
                break;
            case EventType::MEMORY_INJECTION:
                has_memory_injection = true;
                break;
            default:
                break;
        }
    }
    
    if (has_memory_injection) {
        desc << "Memory injection attack chain";
    } else if (has_process_creation && has_file_mod && has_registry_mod) {
        desc << "Multi-stage persistence attack";
    } else {
        desc << "Suspicious event sequence";
    }
    
    return desc.str();
}

std::vector<CorrelatedEventGroup> CorrelationEngine::GetActiveCorrelations() const {
    std::lock_guard<std::mutex> lock(correlations_mutex_);
    return active_correlations_;
}

void CorrelationEngine::SetConfiguration(const CorrelationConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
}

CorrelationConfig CorrelationEngine::GetConfiguration() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

uint64_t CorrelationEngine::GetProcessedEventCount() const {
    return processed_event_count_.load();
}

uint64_t CorrelationEngine::GetCorrelationCount() const {
    return correlation_count_.load();
}

uint64_t CorrelationEngine::GetActiveCorrelationCount() const {
    std::lock_guard<std::mutex> lock(correlations_mutex_);
    return active_correlations_.size();
}

void CorrelationEngine::RegisterCorrelationCallback(CorrelationCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    correlation_callback_ = callback;
}

void CorrelationEngine::ClearOldCorrelations() {
    std::lock_guard<std::mutex> lock(correlations_mutex_);
    
    // Keep only recent correlations (last 100)
    if (active_correlations_.size() > 100) {
        active_correlations_.erase(
            active_correlations_.begin(), 
            active_correlations_.begin() + (active_correlations_.size() - 100)
        );
    }
}

} // namespace HIPS
