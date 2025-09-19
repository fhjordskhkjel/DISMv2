# Advanced HIPS - Host Intrusion Prevention System

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/platform-windows-lightgrey)](https://github.com/fhjordskhkjel/DISMv2)

## Advanced HIPS (Host Intrusion Prevention System)

**🚨 Enterprise-Grade Windows Security Solution with Kernel-Level Protection**

The Advanced HIPS system is a comprehensive Windows-only Host Intrusion Prevention System designed for enterprise environments. It provides real-time monitoring and protection against advanced threats and intrusions through both user-mode and **kernel-mode components**.

### 🛡️ Core Protection Features
- **Kernel-Level File System Monitoring**: Minifilter driver for real-time file access, modifications, and deletions
- **Advanced Process Monitoring**: Kernel callbacks for process creation/termination that cannot be bypassed
- **Network Traffic Analysis**: Monitor network connections and detect suspicious activity
- **Registry Protection**: Kernel-level registry monitoring for critical keys and auto-start locations
- **Memory Protection**: Prevent memory injection and exploit attempts through kernel callbacks
- **Behavioral Analysis**: Detect advanced persistent threats and zero-day exploits
- **Driver Communication**: Secure communication channel between kernel driver and user-mode application

### 🎯 Key Advantages
- **Kernel-Mode Protection**: Cannot be bypassed by rootkits or advanced malware
- **Windows-Native**: Built specifically for Windows using Win32 APIs and Windows Driver Kit (WDK)
- **Enterprise-Ready**: Designed for large-scale deployments with centralized management
- **Visual Studio 2022 Compatible**: Full support for modern development tools and workflows
- **Highly Customizable**: Flexible rule engine for custom security policies
- **Performance Optimized**: Minimal system impact with intelligent filtering
- **Comprehensive Testing**: Enterprise-level test suite ensuring reliability

### 📁 HIPS Documentation
The complete HIPS documentation is located in [hips/docs/README.md](hips/docs/README.md).

## 🚀 Quick Start

### Prerequisites
- Windows 10 or later (Windows 11 recommended for best compatibility)
- Administrator privileges for installation and operation
- **Visual Studio 2022** with C++ workload for compilation
- **Windows Driver Kit (WDK) for Visual Studio 2022** for kernel driver compilation
- CMake 3.15 or later
- **Code signing certificate** or test signing enabled for driver installation

### Building from Source

#### Option 1: Complete Build with Kernel Driver (Recommended)
```batch
# Navigate to HIPS directory
cd hips

# Build both user-mode application and kernel driver
build_with_driver.bat
```

#### Option 2: User-Mode Only Build
```batch
# Navigate to HIPS directory
cd hips

# Windows build using provided script
build.bat

# Manual build using CMake
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release

# Run tests
ctest -C Release
```

#### Option 3: Using Visual Studio 2022 IDE
```batch
# Open the solution file
AdvancedHIPS.sln

# Build the entire solution (Ctrl+Shift+B)
# Or build individual projects:
# - HipsUserMode: User-mode application
# - HipsDriver: Kernel-mode driver
```

### Installation

#### Kernel Driver Installation (Required for Full Protection)
1. **Enable Test Signing** (for development/testing):
   ```batch
   # Run as Administrator
   bcdedit /set testsigning on
   # Reboot required
   ```

2. **Install the Kernel Driver**:
   ```batch
   # Navigate to driver output directory
   cd hips\bin\x64\Release
   
   # Install using PnP utility
   pnputil /add-driver HipsDriver.inf /install
   
   # Or right-click HipsDriver.inf and select "Install"
   ```

3. **Start the Driver Service**:
   ```batch
   # Start the driver
   sc start HipsDriver
   
   # Verify driver is running
   sc query HipsDriver
   ```

#### User-Mode Application
1. Build the project using instructions above
2. Run `hips.exe` as Administrator
3. Configure using `config/hips_config.json`

**Note**: For production deployment, obtain a valid code signing certificate and sign the driver.

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

## Configuration

The HIPS system is configured through `hips/config/hips_config.json`. Key configuration areas include:

- **Monitoring Rules**: Define what activities to monitor
- **Alert Thresholds**: Set sensitivity levels for different threat types
- **Response Actions**: Configure automatic response to detected threats
- **Logging Levels**: Control detail level of audit logs
- **Performance Tuning**: Optimize system impact

## Usage

### Starting the HIPS System

```bash
# Start HIPS monitoring (requires Administrator privileges)
cd hips/build/Release
hips.exe
```

### Managing Rules

The HIPS system supports dynamic rule management through its API:

```cpp
// Add a custom security rule
SecurityRule rule;
rule.name = "Block Suspicious Process";
rule.event_type = EventType::PROCESS_CREATION;
rule.action = ActionType::DENY;
rule.pattern = "suspicious.exe";
hips_engine.AddRule(rule);
```

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

## Performance Considerations

The HIPS system is designed for minimal performance impact:

- **Intelligent Filtering**: Only monitor relevant events
- **Optimized Callbacks**: Fast event processing
- **Resource Management**: Automatic cleanup and memory management
- **Configurable Sensitivity**: Adjust monitoring levels based on needs

## Contributing

1. Fork the repository
2. Create a feature branch
3. Implement your changes with tests
4. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Support

- 📖 [Documentation](hips/docs/README.md)
- 🐛 [Issue Tracker](https://github.com/fhjordskhkjel/DISMv2/issues)
- 💬 [Discussions](https://github.com/fhjordskhkjel/DISMv2/discussions)

---

**Note**: The Advanced HIPS system is designed for Windows environments and requires appropriate security privileges for operation.