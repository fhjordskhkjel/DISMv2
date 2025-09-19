# Advanced HIPS (Host Intrusion Prevention System)

## Overview

The Advanced HIPS is a comprehensive Windows-only Host Intrusion Prevention System designed to provide enterprise-level security monitoring and protection. It offers real-time monitoring of file system activity, process execution, network connections, registry modifications, and memory protection.

## Features

### Core Monitoring Capabilities
- **File System Monitoring**: Real-time monitoring of file access, modifications, and deletions
- **Process Monitoring**: Detection of process creation, termination, and suspicious behavior
- **Network Monitoring**: Monitoring of network connections and traffic patterns
- **Registry Monitoring**: Detection of registry key modifications and suspicious changes
- **Memory Protection**: Protection against memory injection and exploit attempts

### Advanced Security Features
- **Customizable Rules Engine**: Define custom security rules with flexible patterns and actions
- **Threat Level Assessment**: Automatic evaluation of threat levels (Low, Medium, High, Critical)
- **Real-time Alerting**: Immediate notifications for security events
- **Behavioral Analysis**: Detection of suspicious process behavior patterns
- **Enterprise Management**: Centralized configuration and reporting capabilities

### Response Actions
- **Allow**: Permit the action to proceed
- **Deny**: Block the action from executing
- **Quarantine**: Isolate suspicious files or processes
- **Alert Only**: Generate alerts without blocking
- **Custom**: Execute custom response actions

## Architecture

The HIPS system is built with a modular architecture consisting of:

### Core Components
- **HIPSEngine**: Central orchestration and event processing
- **FileSystemMonitor**: File system activity monitoring
- **ProcessMonitor**: Process lifecycle and behavior monitoring
- **NetworkMonitor**: Network traffic and connection monitoring
- **RegistryMonitor**: Windows registry modification monitoring
- **MemoryProtector**: Memory injection and exploit protection

### Support Components
- **ConfigManager**: Configuration management and persistence
- **LogManager**: Comprehensive logging and audit trails
- **AlertManager**: Alert generation and notification system

## Installation

### Prerequisites
- Windows 7 or later (Windows 10/11 recommended)
- Administrator privileges for installation and operation
- Visual Studio 2019 or later / MinGW-w64 for compilation
- CMake 3.15 or later
- Google Test for running tests (optional)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/fhjordskhkjel/DISMv2.git
cd DISMv2/hips

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the project
cmake --build . --config Release

# Run tests (optional)
ctest -C Release
```

### Installing Pre-built Binaries

1. Download the latest release package
2. Extract to desired installation directory
3. Run `hips.exe` as Administrator

## Configuration

### Default Configuration

The HIPS system comes with sensible default configurations:

- File system monitoring of critical directories (System32, Program Files, etc.)
- Process monitoring with suspicious process detection
- Network monitoring for outbound connections
- Registry monitoring of startup and security keys
- Memory protection against common injection techniques

### Custom Rules

Create custom security rules using the rule engine:

```cpp
SecurityRule custom_rule;
custom_rule.name = "Block Suspicious Executables";
custom_rule.description = "Block execution from temp directories";
custom_rule.event_type = EventType::PROCESS_CREATION;
custom_rule.pattern = "\\temp\\";
custom_rule.action = ActionType::DENY;
custom_rule.min_threat_level = ThreatLevel::MEDIUM;
custom_rule.enabled = true;

hips_engine.AddRule(custom_rule);
```

### Configuration Files

Configuration can be persisted to JSON files:

```cpp
// Save current configuration
hips_engine.SaveConfiguration("hips_config.json");

// Load configuration on startup
hips_engine.LoadConfiguration("hips_config.json");
```

## Usage

### Basic Operation

```cpp
#include "hips_core.h"

// Create and initialize HIPS engine
HIPS::HIPSEngine engine;
engine.Initialize();

// Register event handlers
engine.RegisterEventHandler(HIPS::EventType::PROCESS_CREATION,
    [](const HIPS::SecurityEvent& event) {
        std::cout << "New process: " << event.process_path << std::endl;
    });

// Start monitoring
engine.Start();

// Keep running...
// (In practice, this would be a service or GUI application)

// Stop and cleanup
engine.Stop();
engine.Shutdown();
```

### Enterprise Deployment

For enterprise environments:

1. **Central Management**: Deploy configuration files via Group Policy
2. **Logging Integration**: Configure centralized log collection
3. **Alert Integration**: Connect to SIEM systems for alert processing
4. **Performance Tuning**: Adjust scan intervals and thresholds for your environment

## API Reference

### HIPSEngine Class

#### Core Methods
- `bool Initialize()`: Initialize the HIPS engine
- `bool Start()`: Start monitoring and protection
- `bool Stop()`: Stop monitoring
- `bool Shutdown()`: Cleanup and shutdown

#### Rule Management
- `bool AddRule(const SecurityRule& rule)`: Add a security rule
- `bool RemoveRule(const std::string& rule_name)`: Remove a rule
- `std::vector<SecurityRule> GetRules()`: Get all rules

#### Event Handling
- `void RegisterEventHandler(EventType type, handler)`: Register event callback
- `uint64_t GetEventCount(EventType type)`: Get event statistics
- `uint64_t GetTotalEventCount()`: Get total event count

### Event Types

- `EventType::FILE_ACCESS`: File access events
- `EventType::FILE_MODIFICATION`: File modification events
- `EventType::FILE_DELETION`: File deletion events
- `EventType::PROCESS_CREATION`: Process creation events
- `EventType::PROCESS_TERMINATION`: Process termination events
- `EventType::NETWORK_CONNECTION`: Network connection events
- `EventType::REGISTRY_MODIFICATION`: Registry modification events
- `EventType::MEMORY_INJECTION`: Memory injection attempts
- `EventType::EXPLOIT_ATTEMPT`: Exploit attempt detection

### Threat Levels

- `ThreatLevel::LOW`: Low-risk events
- `ThreatLevel::MEDIUM`: Medium-risk events requiring attention
- `ThreatLevel::HIGH`: High-risk events requiring immediate action
- `ThreatLevel::CRITICAL`: Critical threats requiring immediate response

## Performance Considerations

### Optimization Guidelines

1. **Scan Intervals**: Adjust based on system performance requirements
2. **File Type Filtering**: Exclude benign file types from monitoring
3. **Directory Exclusions**: Exclude frequently-changing directories
4. **Memory Thresholds**: Tune memory usage alerts for your environment

### Resource Usage

- **CPU**: Typically 1-5% on modern systems
- **Memory**: 50-200MB depending on configuration
- **Disk I/O**: Minimal impact with proper filtering
- **Network**: Negligible for monitoring operations

## Troubleshooting

### Common Issues

1. **High CPU Usage**: Reduce scan frequency or add file type exclusions
2. **False Positives**: Adjust threat level thresholds or add exclusion rules
3. **Permission Errors**: Ensure administrator privileges
4. **Service Startup Issues**: Check Windows Event Log for detailed errors

### Logging

Enable detailed logging for troubleshooting:

```cpp
log_manager.SetLogLevel(LogLevel::DEBUG);
log_manager.SetLogFile("hips_debug.log");
```

### Support

- GitHub Issues: Report bugs and feature requests
- Documentation: Comprehensive API and usage documentation
- Enterprise Support: Available for enterprise customers

## Security Considerations

### Deployment Security

1. **Privilege Management**: Run with minimal required privileges
2. **Configuration Protection**: Protect configuration files from tampering
3. **Log Security**: Secure log files and transmission
4. **Update Management**: Implement secure update mechanisms

### Threat Model

The HIPS system protects against:

- Malware execution and persistence
- Unauthorized file system modifications
- Process injection and privilege escalation
- Network-based attacks and data exfiltration
- Registry-based persistence mechanisms

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Implement your changes with tests
4. Submit a pull request

## Changelog

### Version 1.0.0
- Initial release
- Core monitoring capabilities
- Rule engine implementation
- Enterprise-grade testing suite
- Comprehensive documentation