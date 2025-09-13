# DISMv2 - Deployment Image Servicing and Management Tool v2

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Python 3.6+](https://img.shields.io/badge/python-3.6+-blue.svg)](https://www.python.org/downloads/)
[![Platform](https://img.shields.io/badge/platform-windows%20%7C%20linux%20%7C%20macos-lightgrey)](https://github.com/fhjordskhkjel/DISMv2)

DISMv2 is a cross-platform implementation of Microsoft's Deployment Image Servicing and Management (DISM) tool. It provides all the core functionality of DISM while being compatible with Windows, Linux, and macOS systems.

## Features

### 🖼️ Image Management
- Mount and unmount WIM, VHD, and VHDX images
- Apply, capture, and export images
- Split large WIM files
- Get comprehensive image information
- Cleanup mount points

### 🔧 Windows Features Management
- Enable and disable Windows features
- Get detailed feature information
- Manage feature dependencies
- Bulk feature operations

### 📦 Package Management
- Install and remove Windows packages
- Query package information
- Manage package dependencies
- Support for CAB, MSU, and MSP packages

### 🔌 Driver Management
- Add and remove driver packages
- Export third-party drivers
- Get driver information
- Support for INF-based drivers

### 📱 Application Management
- Manage provisioned AppX packages
- Add and remove applications
- Set application data files
- Query application information

### 🌍 International Settings
- Configure input locales
- Set system and user locales
- Manage UI languages
- Configure fallback languages

### 🛠️ System Management
- Windows edition upgrades
- Provisioning package management
- Comprehensive logging
- Cross-platform compatibility

## Installation

### From Source
```bash
git clone https://github.com/fhjordskhkjel/DISMv2.git
cd DISMv2
python setup.py install
```

### Using pip (when published)
```bash
pip install dismv2
```

## Quick Start

### Basic Usage
```bash
# Show help
dismv2 /?

# Get information about mounted images
dismv2 /Get-MountedImageInfo

# Mount a WIM image
dismv2 /Mount-Image /ImageFile:install.wim /MountDir:C:\mount /Index:1

# Get Windows features in an offline image
dismv2 /Image:C:\mount /Get-Features

# Enable a feature
dismv2 /Image:C:\mount /Enable-Feature /FeatureName:IIS-WebServerRole

# Unmount and commit changes
dismv2 /Unmount-Image /MountDir:C:\mount /Commit
```

### Online Operations
```bash
# Get features in the running system
dismv2 /Online /Get-Features

# Enable Windows Subsystem for Linux
dismv2 /Online /Enable-Feature /FeatureName:Microsoft-Windows-Subsystem-Linux

# Get installed packages
dismv2 /Online /Get-Packages

# Get driver information
dismv2 /Online /Get-Drivers
```

## Command Reference

### Image Commands
| Command | Description |
|---------|-------------|
| `/Mount-Image` | Mount an image from WIM, VHD, or VHDX file |
| `/Unmount-Image` | Unmount a mounted image |
| `/Commit-Image` | Save changes to a mounted image |
| `/Get-MountedImageInfo` | Display mounted image information |
| `/Get-ImageInfo` | Display image file information |
| `/Apply-Image` | Apply an image to a drive |
| `/Capture-Image` | Capture a drive into a WIM file |
| `/Export-Image` | Export an image to another file |
| `/Split-Image` | Split a WIM file into smaller files |

### Feature Commands
| Command | Description |
|---------|-------------|
| `/Get-Features` | List all Windows features |
| `/Get-FeatureInfo` | Get detailed feature information |
| `/Enable-Feature` | Enable a Windows feature |
| `/Disable-Feature` | Disable a Windows feature |

### Package Commands
| Command | Description |
|---------|-------------|
| `/Get-Packages` | List installed packages |
| `/Get-PackageInfo` | Get detailed package information |
| `/Add-Package` | Install a package |
| `/Remove-Package` | Remove a package |

### Driver Commands
| Command | Description |
|---------|-------------|
| `/Get-Drivers` | List installed drivers |
| `/Get-DriverInfo` | Get detailed driver information |
| `/Add-Driver` | Add a driver package |
| `/Remove-Driver` | Remove a driver package |
| `/Export-Driver` | Export third-party drivers |

## Examples

### Working with Images
```bash
# Mount a Windows installation image
dismv2 /Mount-Image /ImageFile:sources\install.wim /MountDir:C:\mount /Index:1

# Check what features are available
dismv2 /Image:C:\mount /Get-Features

# Enable IIS features
dismv2 /Image:C:\mount /Enable-Feature /FeatureName:IIS-WebServerRole /All

# Add a driver to the image
dismv2 /Image:C:\mount /Add-Driver /Driver:C:\drivers\network.inf

# Commit changes and unmount
dismv2 /Unmount-Image /MountDir:C:\mount /Commit
```

### Online System Management
```bash
# Check current Windows edition
dismv2 /Online /Get-CurrentEdition

# Enable Hyper-V
dismv2 /Online /Enable-Feature /FeatureName:Microsoft-Hyper-V-All

# Install a language pack
dismv2 /Online /Add-Package /PackagePath:language-pack.cab

# Export drivers for backup
dismv2 /Online /Export-Driver /Destination:C:\backup\drivers
```

## Platform Compatibility

### Windows
- Full native DISM compatibility
- All features supported
- Requires administrator privileges for online operations

### Linux
- Core image management features
- Limited Windows feature simulation
- Package management via wine/compatibility layer

### macOS
- Core image management features
- Limited Windows feature simulation
- Package management via wine/compatibility layer

## Requirements

- Python 3.6 or higher
- Administrator/root privileges for online operations
- Additional packages for full functionality (see requirements.txt)

## Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Microsoft for the original DISM tool and documentation
- The open-source community for inspiration and libraries
- Contributors and testers

## Support

- 📖 [Documentation](https://github.com/fhjordskhkjel/DISMv2/wiki)
- 🐛 [Issue Tracker](https://github.com/fhjordskhkjel/DISMv2/issues)
- 💬 [Discussions](https://github.com/fhjordskhkjel/DISMv2/discussions)

---

**Note**: DISMv2 is an independent implementation and is not affiliated with Microsoft Corporation.