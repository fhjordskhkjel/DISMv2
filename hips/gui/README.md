# HIPS Configuration Tool

A cross-platform console application for managing the Advanced HIPS configuration with an intuitive menu-driven interface.

## Features

- **Interactive Console Interface**: Menu-driven navigation for easy configuration management
- **Real-time Configuration Editing**: Live editing of all HIPS settings
- **Driver Status Monitoring**: Check and control HIPS kernel driver status
- **Configuration Preview**: View complete configuration in formatted JSON
- **Cross-platform Support**: Works on Windows, Linux, and macOS
- **JSON Integration**: Seamless integration with existing hips_config.json
- **Input Validation**: Prevents invalid configuration values

## Requirements

- .NET 8.0 Runtime or later
- Access to HIPS configuration file
- Administrator privileges (Windows only, for driver control)

## Building and Running

```bash
# Navigate to the GUI directory
cd hips/gui

# Build the application
dotnet build --configuration Release

# Run the application
dotnet run --configuration Release
```

## Usage

### Main Menu Options

1. **General Settings**: Configure real-time protection, startup scan, log levels, and scan intervals
2. **Monitoring Settings**: Manage file and process monitoring options
3. **Status & Control**: View driver status and control driver services (Windows only)
4. **Save Configuration**: Write current settings to configuration file
5. **Reload Configuration**: Refresh settings from configuration file
6. **Preview Configuration**: View complete configuration in JSON format

### Example Workflow

1. Start the application
2. Navigate to "General Settings" (option 1)
3. Modify settings as needed (toggle options, change values)
4. Save configuration (option 4 from main menu)
5. Check status and restart driver if needed (option 3)

## Configuration Management

The tool provides interactive editing for:

### General Settings
- Real-time protection enable/disable
- Startup scan configuration
- Log level selection (ERROR, WARN, INFO, DEBUG)
- Scan interval adjustment (100-60000ms)

### Monitoring Settings
- File monitoring enable/disable
- Network drive monitoring
- Scan depth configuration (1-10)
- Process monitoring settings
- Memory threshold adjustment (100-4096MB)

## Driver Control (Windows Only)

- **Start Driver**: `sc start HipsDriver`
- **Stop Driver**: `sc stop HipsDriver`
- **Restart Driver**: Stop followed by start with delay
- **Status Check**: Query current driver state

## Error Handling

The application includes comprehensive error handling for:
- Missing configuration files (creates default configuration)
- Invalid JSON syntax
- File permission issues
- Driver communication errors (Windows)
- Invalid input validation