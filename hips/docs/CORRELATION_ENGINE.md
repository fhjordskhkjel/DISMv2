# Correlation Engine Documentation

## Overview

The Correlation Engine is a sophisticated component of the HIPS (Host Intrusion Prevention System) that detects and tracks correlated security events to identify potential multi-stage attacks and attack chains. By analyzing patterns across multiple events, the correlation engine can detect sophisticated threats that might not be apparent from individual events alone.

## Features

### Correlation Types

The engine supports five different correlation detection methods:

1. **Process-Based Correlation**
   - Detects multiple suspicious events originating from the same process
   - Useful for identifying malicious processes engaging in various malicious activities
   - Example: A process that creates files, modifies registry, and makes network connections

2. **Time-Based Correlation**
   - Identifies bursts of high-threat events within a configurable time window
   - Detects coordinated attacks or rapid attack execution
   - Example: Multiple critical events occurring within 60 seconds

3. **Target-Based Correlation**
   - Tracks multiple processes targeting the same file or registry key
   - Identifies coordinated attacks from multiple malicious processes
   - Example: Multiple processes attempting to modify the same system file

4. **Sequence-Based Correlation**
   - Detects known attack patterns and sequences
   - Recognizes multi-stage attack chains
   - Example: Process creation → File modification → Registry modification (classic persistence attack)

5. **Threat Escalation Detection**
   - Monitors for increasing threat levels from the same process
   - Identifies attackers progressively escalating their activities
   - Example: Low → Medium → High → Critical threat progression

## Architecture

### Core Components

```
┌─────────────────────────────────────────────────┐
│           Security Event Sources                │
│  (File Monitor, Process Monitor, Registry, etc) │
└────────────────┬────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────┐
│            HIPS Core Engine                     │
│      (ProcessSecurityEvent)                     │
└────────────────┬────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────┐
│         Correlation Engine                      │
│                                                 │
│  ┌───────────────────────────────────────────┐ │
│  │  Event Tracking                           │ │
│  │  - Process-based queue                    │ │
│  │  - Target-based queue                     │ │
│  │  - Time window queue                      │ │
│  └───────────────────────────────────────────┘ │
│                                                 │
│  ┌───────────────────────────────────────────┐ │
│  │  Correlation Detection                    │ │
│  │  - Pattern matching                       │ │
│  │  - Score calculation                      │ │
│  │  - Threat level aggregation               │ │
│  └───────────────────────────────────────────┘ │
│                                                 │
│  ┌───────────────────────────────────────────┐ │
│  │  Correlation Groups                       │ │
│  │  - Active correlations storage            │ │
│  │  - Metadata management                    │ │
│  └───────────────────────────────────────────┘ │
└────────────────┬────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────┐
│          Alert Manager                          │
│     (Correlation-based Alerts)                  │
└─────────────────────────────────────────────────┘
```

## Configuration

### CorrelationConfig Structure

```cpp
struct CorrelationConfig {
    int time_window_seconds = 60;           // Time window for correlation
    int min_events_for_correlation = 3;     // Minimum events to trigger
    double min_correlation_score = 0.6;     // Minimum score threshold
    int max_events_per_process = 100;       // Max events tracked per process
    int max_correlation_groups = 1000;      // Max active correlations
    
    // Enable/disable specific correlation types
    bool enable_process_correlation = true;
    bool enable_time_correlation = true;
    bool enable_target_correlation = true;
    bool enable_sequence_correlation = true;
    bool enable_threat_escalation = true;
};
```

### Configuration Examples

#### High Security Configuration (Sensitive)
```cpp
CorrelationConfig config;
config.time_window_seconds = 30;        // Shorter window
config.min_events_for_correlation = 2;  // More sensitive
config.min_correlation_score = 0.5;     // Lower threshold
```

#### Balanced Configuration (Default)
```cpp
CorrelationConfig config;
config.time_window_seconds = 60;
config.min_events_for_correlation = 3;
config.min_correlation_score = 0.6;
```

#### Performance Configuration (Lower Overhead)
```cpp
CorrelationConfig config;
config.time_window_seconds = 120;       // Longer window
config.min_events_for_correlation = 5;  // Less sensitive
config.min_correlation_score = 0.7;     // Higher threshold
config.enable_sequence_correlation = false;  // Disable expensive checks
```

## Usage

### Basic Initialization

```cpp
#include "correlation_engine.h"

// Create and initialize correlation engine
HIPS::CorrelationEngine engine;
HIPS::CorrelationConfig config;

if (!engine.Initialize(config)) {
    // Handle initialization error
}
```

### Register Correlation Callback

```cpp
engine.RegisterCorrelationCallback([](const CorrelatedEventGroup& group) {
    std::cout << "Correlation detected: " << group.description << std::endl;
    std::cout << "Events: " << group.events.size() << std::endl;
    std::cout << "Score: " << group.correlation_score << std::endl;
    std::cout << "Threat Level: " << (int)group.combined_threat_level << std::endl;
});
```

### Process Events

```cpp
// Events are automatically processed when integrated with HIPS Core
// Manual processing example:
SecurityEvent event;
event.type = EventType::FILE_MODIFICATION;
event.threat_level = ThreatLevel::HIGH;
event.process_id = 1234;
// ... set other fields ...

engine.ProcessEvent(event);
```

### Query Correlations

```cpp
// Get all active correlations
auto correlations = engine.GetActiveCorrelations();

for (const auto& corr : correlations) {
    std::cout << "Correlation ID: " << corr.correlation_id << std::endl;
    std::cout << "Type: " << (int)corr.type << std::endl;
    std::cout << "Events: " << corr.events.size() << std::endl;
}

// Get statistics
uint64_t processed = engine.GetProcessedEventCount();
uint64_t total_corr = engine.GetCorrelationCount();
uint64_t active_corr = engine.GetActiveCorrelationCount();
```

## Correlation Scoring

The correlation score is calculated based on multiple factors:

1. **Event Count** (0.0 - 0.3)
   - More events = higher score
   - Capped at 10 events for maximum contribution

2. **Threat Level** (0.0 - 0.4)
   - Percentage of high/critical threat events
   - Higher threats = higher score

3. **Correlation Type** (0.15 - 0.3)
   - Process-based: +0.2
   - Target-based: +0.25
   - Sequence-based: +0.3
   - Threat escalation: +0.3
   - Time-based: +0.15

**Final Score Range:** 0.0 - 1.0

### Score Interpretation

- **0.0 - 0.4:** Low correlation (not significant)
- **0.5 - 0.6:** Medium correlation (potential threat)
- **0.7 - 0.8:** High correlation (likely attack)
- **0.9 - 1.0:** Critical correlation (confirmed attack pattern)

## Integration with HIPS Core

The correlation engine is automatically integrated into the HIPS Core engine. All security events are automatically fed to the correlation engine for analysis.

### Automatic Integration

```cpp
// In hips_core.cpp - ProcessSecurityEvent
void HIPSEngine::ProcessSecurityEvent(const SecurityEvent& event) {
    // ... existing processing ...
    
    // Feed event to correlation engine
    if (correlation_engine_) {
        correlation_engine_->ProcessEvent(event);
    }
    
    // ... rest of processing ...
}
```

### Correlation Alerts

When correlations are detected, they automatically generate alerts through the Alert Manager:

```cpp
correlation_engine_->RegisterCorrelationCallback([this](const CorrelatedEventGroup& group) {
    SecurityEvent corr_event;
    corr_event.type = EventType::EXPLOIT_ATTEMPT;
    corr_event.threat_level = group.combined_threat_level;
    corr_event.description = "[CORRELATION] " + group.description;
    
    alert_manager_->SendAlert(corr_event, 
        "Correlation detected: " + group.description);
});
```

## Known Attack Patterns

The correlation engine recognizes several known attack patterns:

### Pattern 1: Multi-Stage Persistence Attack
**Sequence:** Process Creation → File Modification → Registry Modification

**Description:** Attacker creates a process, modifies system files, and establishes persistence through registry modifications.

**Example:**
1. Dropper executed (Process Creation)
2. Payload dropped to disk (File Modification)
3. Autostart registry key created (Registry Modification)

### Pattern 2: Memory Injection Attack Chain
**Sequence:** Memory Injection → File/Registry Changes

**Description:** Attacker injects code into memory and then modifies files or registry.

**Example:**
1. Code injection into legitimate process (Memory Injection)
2. System file modified from injected code (File Modification)
3. Service configuration changed (Registry Modification)

## Performance Considerations

### Memory Usage

- Each tracked event: ~500 bytes
- Default max events per process: 100 (50 KB per process)
- Default max correlation groups: 1000 (variable size)
- Estimated total memory: 5-10 MB under normal load

### CPU Usage

- Event processing: O(1) insertion
- Correlation detection: O(n) per event type
- Pattern matching: O(n*m) where n = events, m = patterns
- Typical overhead: <1% CPU under normal load

### Tuning Recommendations

1. **High-Volume Environment:**
   - Increase `time_window_seconds` to reduce correlation frequency
   - Increase `min_events_for_correlation` to reduce false positives
   - Disable less critical correlation types

2. **Security-Critical Environment:**
   - Decrease `min_events_for_correlation` for higher sensitivity
   - Lower `min_correlation_score` to catch more subtle attacks
   - Enable all correlation types

3. **Resource-Constrained Systems:**
   - Reduce `max_events_per_process`
   - Reduce `max_correlation_groups`
   - Increase `min_correlation_score`

## Testing

### Running Tests

```bash
# Build and run correlation engine tests
cd hips/build
cmake --build . --target test_correlation_engine
./test_correlation_engine
```

### Running Demo

```bash
# Compile and run the demonstration
cd hips
g++ -std=c++17 -I./include demo_correlation_engine.cpp src/correlation_engine.cpp -o demo_correlation
./demo_correlation
```

The demo demonstrates:
- Process-based correlation
- Target-based correlation
- Threat escalation detection
- Known attack pattern recognition

## API Reference

### CorrelationEngine Class

#### Methods

**Initialize()**
```cpp
bool Initialize();
bool Initialize(const CorrelationConfig& config);
```
Initializes the correlation engine with default or custom configuration.

**ProcessEvent()**
```cpp
void ProcessEvent(const SecurityEvent& event);
```
Processes a security event and checks for correlations.

**DetectCorrelations()**
```cpp
std::vector<CorrelatedEventGroup> DetectCorrelations();
```
Manually triggers correlation detection and returns results.

**GetActiveCorrelations()**
```cpp
std::vector<CorrelatedEventGroup> GetActiveCorrelations() const;
```
Returns all currently active correlations.

**RegisterCorrelationCallback()**
```cpp
void RegisterCorrelationCallback(CorrelationCallback callback);
```
Registers a callback function to be notified when correlations are detected.

**GetProcessedEventCount()**
```cpp
uint64_t GetProcessedEventCount() const;
```
Returns total number of events processed.

**GetCorrelationCount()**
```cpp
uint64_t GetCorrelationCount() const;
```
Returns total number of correlations detected.

**GetActiveCorrelationCount()**
```cpp
uint64_t GetActiveCorrelationCount() const;
```
Returns number of currently active correlations.

## Troubleshooting

### No Correlations Detected

**Possible Causes:**
1. `min_events_for_correlation` is set too high
2. `min_correlation_score` threshold is too high
3. Events are too far apart in time
4. Correlation types are disabled

**Solutions:**
- Lower the minimum event count
- Reduce the correlation score threshold
- Increase the time window
- Enable required correlation types

### Too Many False Positives

**Possible Causes:**
1. Score threshold too low
2. Time window too large
3. Minimum events too low

**Solutions:**
- Increase `min_correlation_score`
- Decrease `time_window_seconds`
- Increase `min_events_for_correlation`

### High Memory Usage

**Possible Causes:**
1. Too many events being tracked
2. Correlation groups not being cleaned up

**Solutions:**
- Reduce `max_events_per_process`
- Reduce `max_correlation_groups`
- Call `ClearOldCorrelations()` periodically

## Future Enhancements

Planned improvements for future versions:

1. **Machine Learning Integration**
   - Anomaly detection using ML models
   - Adaptive threshold adjustment
   - Custom pattern learning

2. **Advanced Pattern Library**
   - Expand known attack patterns
   - MITRE ATT&CK framework integration
   - User-defined custom patterns

3. **Performance Optimizations**
   - Bloom filters for fast lookups
   - Parallel correlation detection
   - Event batching

4. **Enhanced Reporting**
   - Correlation visualization
   - Attack chain graphs
   - Export to SIEM systems

## License

This component is part of the Advanced HIPS system and is licensed under the MIT License.

## Support

For issues, questions, or contributions related to the correlation engine:
- GitHub Issues: https://github.com/fhjordskhkjel/DISMv2/issues
- Documentation: See main HIPS documentation in hips/docs/README.md
