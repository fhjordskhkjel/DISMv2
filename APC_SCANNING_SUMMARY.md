# APC Queue Scanning Implementation

## Overview
This document describes the Asynchronous Procedure Call (APC) queue scanning feature added to the HIPS Process Monitor component.

## What is APC Queue Scanning?
APC (Asynchronous Procedure Call) queue scanning is a security technique used to detect code injection in Windows processes. Malware often uses APC injection to execute code in the context of legitimate processes without detection.

### How APC Injection Works
1. Attackers open a handle to a target process
2. They allocate memory in the target process
3. They write malicious code to that memory
4. They queue an APC to one of the process's threads
5. When the thread enters an alertable wait state, the malicious code executes

## Implementation Details

### New Methods Added to ProcessMonitor

#### 1. `ScanThreadAPCQueues()`
Main scanning method that iterates through all monitored processes and checks their threads for suspicious APC entries.
- Skips system processes to avoid false positives
- Generates security events when suspicious APCs are detected
- Event type: `MEMORY_INJECTION`
- Threat level: `HIGH`

#### 2. `ScanProcessThreadAPCs(DWORD pid, const std::string& process_name)`
Scans APC queues for a specific process:
- Enumerates all threads belonging to the process
- Opens each thread with `THREAD_QUERY_INFORMATION` permissions
- Checks for suspicious APC queue entries
- Returns `true` if suspicious activity is detected

#### 3. `GetProcessThreads(DWORD pid)`
Helper method to enumerate threads for a given process:
- Uses `CreateToolhelp32Snapshot` with `TH32CS_SNAPTHREAD`
- Filters threads by owner process ID
- Returns vector of thread IDs

### Integration into Monitoring Loop
The APC scanning is integrated into the main monitoring thread function:
```cpp
void ProcessMonitor::MonitoringThreadFunction() {
    while (running_.load()) {
        ScanForNewProcesses();
        ScanForTerminatedProcesses();
        CheckProcessBehavior();
        ScanThreadAPCQueues();  // New APC scanning
        Sleep(scan_interval_);
    }
}
```

## Cross-Platform Compatibility
The implementation includes proper platform guards:
- Full implementation on Windows (`#ifdef _WIN32`)
- Stub implementations for non-Windows platforms (for compilation)
- Uses Windows-specific APIs: `OpenProcess`, `OpenThread`, `CreateToolhelp32Snapshot`

## Production Implementation Notes
The current implementation provides a framework for APC queue inspection. For production use, the following enhancements are recommended:

### 1. Use NtQueryInformationThread
```cpp
typedef NTSTATUS (NTAPI *pNtQueryInformationThread)(
    HANDLE ThreadHandle,
    THREADINFOCLASS ThreadInformationClass,
    PVOID ThreadInformation,
    ULONG ThreadInformationLength,
    PULONG ReturnLength
);
```

### 2. Check APC Queue Details
- Query `ThreadLastSystemCall` information class
- Enumerate actual APC entries in the queue
- Validate APC target addresses

### 3. Validate Memory Regions
- Check if APC routines point to non-module memory
- Validate against known module ranges
- Detect APCs pointing to suspicious memory regions

### 4. Heuristic Detection
- Monitor unusual APC count patterns
- Check thread alertable wait states
- Detect repeated APC queueing to same thread

## Security Event Generation
When suspicious APC activity is detected, a `SecurityEvent` is generated with:
- **Type**: `MEMORY_INJECTION`
- **Threat Level**: `HIGH`
- **Process ID**: PID of the affected process
- **Process Path**: Full path to the process executable
- **Description**: "Suspicious APC queue entry detected in process: [name]"
- **Timestamp**: System time when detection occurred

## Testing
A test case has been added: `TEST_F(ProcessMonitorTest, APCQueueScanning)`
- Initializes and starts the process monitor
- Allows APC scanning to run for 300ms
- Verifies the monitor remains stable during scanning
- Tests that the feature doesn't cause crashes

## Usage Example
```cpp
// Initialize process monitor
ProcessMonitor monitor;
monitor.Initialize();

// Register callback to receive APC detection events
monitor.RegisterCallback([](const SecurityEvent& event) {
    if (event.type == EventType::MEMORY_INJECTION) {
        std::cout << "APC Injection detected in: " << event.process_path << std::endl;
    }
});

// Start monitoring (includes APC scanning)
monitor.Start();
```

## Performance Considerations
- APC scanning is performed during each monitoring cycle
- Scan interval can be configured via `SetScanInterval()`
- System processes are excluded to reduce overhead
- Thread enumeration uses efficient snapshot mechanisms

## Future Enhancements
1. Implement full NtQueryInformationThread-based APC inspection
2. Add APC target address validation against module ranges
3. Implement machine learning for APC pattern recognition
4. Add configuration options for APC scan frequency
5. Implement APC blocking/prevention mechanisms
6. Add detailed logging of APC queue contents

## Related Documentation
- [Process Monitor Implementation](hips/include/process_monitor.h)
- [HIPS Core Documentation](hips/include/hips_core.h)
- [Windows APC Internals](https://docs.microsoft.com/en-us/windows/win32/sync/asynchronous-procedure-calls)

## Author Notes
This implementation provides the infrastructure for APC queue scanning in the HIPS system. The framework is in place for full APC inspection, with detailed comments on how to implement production-grade detection using Windows internal APIs.
