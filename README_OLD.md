# Advanced HIPS - Host Intrusion Prevention System

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/platform-windows-lightgrey)](https://github.com/fhjordskhkjel/DISMv2)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/fhjordskhkjel/DISMv2)

## 🚀 Development Progress & Status

**Current Release:** v1.2.0 (Enhanced Kernel Driver Integration)  
**Last Updated:** September 2024

### ✅ Completed Features
- **✅ Kernel-Mode Driver**: Production-ready minifilter driver with enhanced security
- **✅ File System Monitoring**: Real-time file operations tracking via minifilter
- **✅ Process Monitoring**: Kernel-level process creation/termination callbacks
- **✅ Registry Protection**: Comprehensive registry monitoring and protection
- **✅ Driver Communication**: Secure user-mode to kernel-mode communication
- **✅ Memory Protection**: Advanced memory injection detection and prevention
- **✅ Configuration System**: Flexible JSON-based configuration management
- **✅ Threat Analysis**: Intelligent threat level assessment and rule engine
- **✅ BSOD Prevention**: Enhanced driver stability with comprehensive error handling

### 🔄 In Progress
- **✅ GUI Configuration Tool**: Windows Forms application for easy configuration (NEW!)
- **🔄 Network Monitoring**: Extended network traffic analysis capabilities
- **🔄 Automated Testing**: Comprehensive test suite for all components

### 📋 Planned Features
- **📋 Real-time Dashboard**: Web-based monitoring interface
- **📋 Enterprise Management**: Centralized policy management
- **📋 Machine Learning**: AI-powered threat detection
- **📋 Cloud Integration**: Cloud-based threat intelligence

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
- **Native VC++ GUI**: Modern MFC-based graphical interface for easy configuration and monitoring
- **BSOD-Proof Driver**: Enhanced exception handling prevents system crashes
- **Highly Customizable**: Flexible rule engine for custom security policies
- **Performance Optimized**: Minimal system impact with intelligent filtering
- **Comprehensive Testing**: Enterprise-level test suite ensuring reliability

### 📁 HIPS Documentation
The complete HIPS documentation is located in [hips/docs/README.md](hips/docs/README.md).

## 🔧 Development Tools Support

### Visual Studio 2022 Integration
- Complete VS2022 solution with user-mode and kernel driver projects
- Integrated WDK support for kernel development
- IntelliSense support for both user and kernel code
- Integrated debugging for kernel drivers

### Build Systems
- **Visual Studio 2022**: Native MSBuild support with WDK integration
- **CMake**: Cross-platform build system with VS2022 generator support
- **Batch Scripts**: Automated build scripts for complete system compilation

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

#### GUI Configuration Tool (NEW!)

The HIPS system now includes a modern MFC-based GUI application built with Visual C++:

**Features:**
- **Real-time Driver Status**: Monitor connection and operational status
- **Event Monitoring**: View security events in real-time with detailed information
- **Driver Control**: Start/stop monitoring, connect/disconnect from kernel driver
- **Activity Logging**: Comprehensive logging of all system activities
- **Configuration Management**: Easy-to-use interface for system settings

**Using the GUI:**
1. Build the GUI project in Visual Studio 2022:
   ```batch
   cd hips\gui_vcpp
   msbuild HipsGui.vcxproj /p:Configuration=Release /p:Platform=x64
   ```

2. Launch the application:
   ```batch
   HipsGui.exe
   ```

3. Connect to the driver and start monitoring through the intuitive interface

#### User-Mode Application
1. Build the project using instructions above
2. Run `hips.exe` as Administrator
3. Configure using `config/hips_config.json` or the new **GUI Configuration Tool**

**GUI Configuration Tool (NEW!):**
```batch
# Navigate to GUI directory
cd hips\gui

# Build and run the configuration tool
dotnet build --configuration Release
dotnet run
```

**Note**: For production deployment, obtain a valid code signing certificate and sign the driver.

## Architecture

The HIPS system is built with a hybrid architecture consisting of both user-mode and kernel-mode components:

### Kernel-Mode Components (NEW!)
- **HipsDriver**: Minifilter driver providing kernel-level monitoring
  - **File System Monitor**: Real-time file operations monitoring via minifilter
  - **Process Monitor**: Process creation/termination callbacks
  - **Registry Monitor**: Registry modification monitoring
  - **Rule Engine**: Real-time security rule evaluation and blocking
  - **Event Manager**: Secure kernel-to-user event communication

### User-Mode Components
- **HIPSEngine**: Central orchestration and event processing
- **FileSystemMonitor**: Enhanced file system activity monitoring
- **ProcessMonitor**: Process lifecycle and behavior monitoring  
- **NetworkMonitor**: Network traffic and connection monitoring
- **RegistryMonitor**: Enhanced Windows registry modification monitoring
- **MemoryProtector**: Memory injection and exploit protection
- **DriverInterface**: Communication bridge to kernel driver

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

## Troubleshooting

### Common Issues

#### Driver Installation Issues
- **Problem**: Driver fails to install with "Driver is not digitally signed" error
- **Solution**: Enable test signing mode or obtain a valid code signing certificate
  ```batch
  # Enable test signing (requires reboot)
  bcdedit /set testsigning on
  ```

#### Driver Won't Start
- **Problem**: `sc start HipsDriver` returns error
- **Solutions**:
  1. Check if driver is properly installed: `sc query HipsDriver`
  2. Verify kernel driver files are in correct location
  3. Check Event Viewer for detailed error messages
  4. Ensure running as Administrator

#### BSOD or System Instability
- **Problem**: Blue Screen of Death when driver is loaded
- **Prevention**: Enhanced driver stability implemented with:
  - Comprehensive buffer validation
  - Exception handling around user buffer access
  - Memory leak prevention
  - Proper IRQL level management
- **Solution**: Boot in safe mode and remove driver if issues persist

#### High CPU Usage
- **Problem**: HIPS system consuming too much CPU
- **Solutions**:
  1. Adjust scan intervals in configuration
  2. Exclude non-critical file extensions
  3. Reduce monitoring scope in `hips_config.json`

#### Configuration Not Applied
- **Problem**: Changes to configuration file not taking effect
- **Solutions**:
  1. Restart HIPS service after configuration changes
  2. Verify JSON syntax is valid
  3. Check file permissions on configuration file
  4. Use the new **HIPS Configuration Tool** for easier management:
     ```bash
     cd hips/gui
     dotnet run --configuration Release
     ```

### HIPS Configuration Tool Features
The new configuration tool provides:
- **Interactive Menu System**: Easy navigation through configuration options
- **Real-time Settings Management**: Modify general and monitoring settings
- **Driver Status Control**: Start, stop, and restart the HIPS driver
- **Configuration Preview**: View complete configuration in formatted JSON
- **Cross-platform Support**: Works on Windows and Linux (console mode)

### Getting Help
- 📖 [Complete Documentation](hips/docs/README.md)
- 🐛 [Report Issues](https://github.com/fhjordskhkjel/DISMv2/issues)
- 💬 [Community Discussions](https://github.com/fhjordskhkjel/DISMv2/discussions)

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

## Troubleshooting

### Common Issues

#### Driver Installation Issues
- **Problem**: Driver fails to install with "Driver is not digitally signed" error
- **Solution**: Enable test signing mode or obtain a valid code signing certificate
  ```batch
  # Enable test signing (requires reboot)
  bcdedit /set testsigning on
  ```

#### Driver Won't Start
- **Problem**: `sc start HipsDriver` returns error
- **Solutions**:
  1. Check if driver is properly installed: `sc query HipsDriver`
  2. Verify kernel driver files are in correct location
  3. Check Event Viewer for detailed error messages
  4. Ensure running as Administrator

#### BSOD or System Instability
- **Problem**: Blue Screen of Death when driver is loaded
- **Prevention**: Enhanced driver stability implemented with:
  - Comprehensive buffer validation
  - Exception handling around user buffer access
  - Memory leak prevention
  - Proper IRQL level management
  - Robust cleanup in exception paths
- **Solution**: Boot in safe mode and remove driver if issues persist

#### GUI Connection Issues
- **Problem**: GUI cannot connect to driver
- **Solutions**:
  1. Ensure driver is running: `sc query HipsDriver`
  2. Run GUI as Administrator
  3. Check Windows Event Viewer for driver errors
  4. Verify driver device is created: Check `\\Device\\HipsDriver`

#### High CPU Usage
- **Problem**: HIPS driver causing high system load
- **Solutions**:
  1. Adjust monitoring sensitivity in configuration
  2. Reduce event queue size
  3. Add exclusions for trusted processes
  4. Check for event loop issues in logs

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Support

- 📖 [Documentation](hips/docs/README.md)
- 🐛 [Issue Tracker](https://github.com/fhjordskhkjel/DISMv2/issues)
- 💬 [Discussions](https://github.com/fhjordskhkjel/DISMv2/discussions)

---

**Note**: The Advanced HIPS system is designed for Windows environments and requires appropriate security privileges for operation.