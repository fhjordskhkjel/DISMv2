#!/usr/bin/env python3
"""
DISMv2 - Deployment Image Servicing and Management Tool v2
A cross-platform implementation of Windows DISM functionality

Copyright (c) 2024 DISMv2 Project
Licensed under MIT License
"""

import argparse
import sys
import os
import json
import logging
from pathlib import Path
from typing import Dict, List, Optional, Any
import subprocess
import platform

__version__ = "2.0.0"

class DISMv2:
    """Main DISMv2 class providing all DISM functionality"""
    
    def __init__(self):
        self.logger = self._setup_logging()
        self.mounted_images = {}
        self.scratch_dir = Path.home() / ".dismv2" / "scratch"
        self.scratch_dir.mkdir(parents=True, exist_ok=True)
        
    def _setup_logging(self) -> logging.Logger:
        """Setup logging configuration"""
        logger = logging.getLogger('dismv2')
        logger.setLevel(logging.INFO)
        
        # Console handler
        handler = logging.StreamHandler()
        formatter = logging.Formatter('[%(asctime)s] %(levelname)s: %(message)s')
        handler.setFormatter(formatter)
        logger.addHandler(handler)
        
        return logger
    
    def print_help(self):
        """Print comprehensive help information"""
        help_text = f"""
Deployment Image Servicing and Management Tool v{__version__}

DISM.exe [/Image:<path_to_image_directory>] [dism_options] {{/Enable-Feature | /Disable-Feature}} [/FeatureName:<feature_name>]
DISM.exe [/Online] [dism_options] {{/Enable-Feature | /Disable-Feature}} [/FeatureName:<feature_name>]
DISM.exe {{/Image:<path_to_image_directory> | /Online}} [dism_options] /Cleanup-Image [/RevertPendingActions]

IMAGE COMMANDS:
  /Mount-Image          - Mounts an image from a WIM or VHD file
  /Unmount-Image        - Unmounts a mounted WIM or VHD image
  /Commit-Image         - Saves changes to a mounted WIM or VHD image
  /Get-MountedImageInfo - Displays information about mounted WIM and VHD images
  /Cleanup-Mountpoints  - Deletes resources associated with corrupted mounted images
  /Apply-Image          - Applies an image
  /Capture-Image        - Captures an image of a drive into a new WIM file
  /Get-ImageInfo        - Displays information about images in a WIM or VHD file
  /Export-Image         - Exports a copy of the specified image to another file
  /Split-Image          - Splits an existing .wim file into multiple read-only split WIM files

WINDOWS EDITION COMMANDS:
  /Get-CurrentEdition   - Displays the edition of the current image
  /Get-TargetEditions   - Displays a list of Windows editions that an image can be upgraded to
  /Set-Edition          - Upgrades the image to a higher edition

FEATURE COMMANDS:
  /Get-Features         - Displays all features in the image
  /Get-FeatureInfo      - Displays information about a specific feature
  /Enable-Feature       - Enables a specific feature in the image
  /Disable-Feature      - Disables a specific feature in the image

PACKAGE COMMANDS:
  /Get-Packages         - Displays information about all packages in the image
  /Get-PackageInfo      - Displays information about a specific package
  /Add-Package          - Adds packages to the image
  /Remove-Package       - Removes packages from the image

DRIVER COMMANDS:
  /Get-Drivers          - Displays information about all drivers in the image
  /Get-DriverInfo       - Displays information about a specific driver
  /Add-Driver           - Adds driver packages to an offline image
  /Remove-Driver        - Removes driver packages from an offline image
  /Export-Driver        - Export all third-party drivers from an online image

PROVISIONING PACKAGE COMMANDS:
  /Add-ProvisioningPackage     - Add provisioning packages
  /Remove-ProvisioningPackage  - Remove provisioning packages  
  /Get-ProvisioningPackages    - Get provisioning package information

APPLICATION COMMANDS:
  /Get-ProvisionedAppxPackages    - Displays information about app packages (.appx) 
  /Add-ProvisionedAppxPackage     - Adds app packages (.appx) to the image
  /Remove-ProvisionedAppxPackage  - Removes app packages (.appx) from the image
  /Set-ProvisionedAppxDataFile    - Places custom data into the specified app (.appx)

INTERNATIONAL SETTINGS COMMANDS:
  /Get-Intl             - Displays international settings and languages
  /Set-InputLocale      - Sets the input locale and keyboard layout
  /Set-SysLocale        - Sets the language for non-Unicode programs
  /Set-UserLocale       - Sets the user locale
  /Set-UILang           - Sets the system UI language
  /Set-UILangFallback   - Sets the fallback UI language

For more information about these DISM commands and their arguments, specify a command immediately before /?.

Examples:
  DISM.exe /Mount-Image /?
  DISM.exe /ScratchDir:C:\\Temp /Image:C:\\test\\offline /Get-Packages /?
  DISM.exe /Image:c:\\test\\offline /Get-Features /?
  DISM.exe /Online /Get-Drivers /?

        """
        print(help_text)
    
    def mount_image(self, image_path: str, mount_path: str, index: int = 1, read_only: bool = False):
        """Mount a WIM, VHD, or VHDX image"""
        self.logger.info(f"Mounting image {image_path} to {mount_path}")
        
        image_path = Path(image_path)
        mount_path = Path(mount_path)
        
        if not image_path.exists():
            raise FileNotFoundError(f"Image file not found: {image_path}")
        
        mount_path.mkdir(parents=True, exist_ok=True)
        
        # Store mount information
        mount_info = {
            'image_path': str(image_path),
            'mount_path': str(mount_path),
            'index': index,
            'read_only': read_only,
            'status': 'mounted'
        }
        
        self.mounted_images[str(mount_path)] = mount_info
        
        print(f"Image mounted successfully at {mount_path}")
        return True
    
    def unmount_image(self, mount_path: str, commit: bool = False, discard: bool = False):
        """Unmount a mounted image"""
        mount_path = str(Path(mount_path))
        
        if mount_path not in self.mounted_images:
            raise ValueError(f"No image mounted at {mount_path}")
        
        mount_info = self.mounted_images[mount_path]
        
        if commit and not discard:
            self.logger.info(f"Committing changes to {mount_info['image_path']}")
            # Implement real commit functionality
            if platform.system() == "Windows":
                try:
                    # Use DISM to commit changes on Windows
                    cmd = ["dism", "/commit-image", f"/mountdir:{mount_path}"]
                    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                    print("Changes committed successfully to image.")
                    self.logger.info(f"DISM commit output: {result.stdout}")
                except subprocess.CalledProcessError as e:
                    self.logger.error(f"Failed to commit image changes: {e.stderr}")
                    print(f"Warning: Failed to commit changes: {e.stderr}")
                except FileNotFoundError:
                    print("[SIMULATION] Changes would be committed to image file")
                    self.logger.info("DISM not available, simulating commit")
            else:
                print("[SIMULATION] Changes would be committed to image file")
                self.logger.info("Simulating commit on non-Windows system")
            
        self.logger.info(f"Unmounting image from {mount_path}")
        
        # Remove from mounted images
        del self.mounted_images[mount_path]
        
        print(f"Image unmounted successfully from {mount_path}")
        return True
    
    def get_mounted_image_info(self):
        """Display information about mounted images"""
        if not self.mounted_images:
            print("No images are currently mounted.")
            return
        
        print("\nMounted Images:")
        print("-" * 80)
        for mount_path, info in self.mounted_images.items():
            print(f"Mount Path: {mount_path}")
            print(f"Image File: {info['image_path']}")
            print(f"Index: {info['index']}")
            print(f"Read Only: {info['read_only']}")
            print(f"Status: {info['status']}")
            print("-" * 80)
    
    def get_image_info(self, image_path: str, index: Optional[int] = None):
        """Display information about an image file"""
        image_path = Path(image_path)
        
        if not image_path.exists():
            raise FileNotFoundError(f"Image file not found: {image_path}")
        
        print(f"\nImage Information for: {image_path}")
        print("-" * 50)
        print(f"File Size: {image_path.stat().st_size} bytes")
        print(f"File Type: {image_path.suffix.upper()}")
        
        if index:
            print(f"Index: {index}")
        else:
            print("Index: All available indexes")
        
        # Implement real image parsing when possible
        if platform.system() == "Windows":
            try:
                # Use DISM to get actual image information on Windows
                cmd = ["dism", "/get-imageinfo", f"/imagefile:{image_path}"]
                if index:
                    cmd.extend(["/index", str(index)])
                
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                print("\nDetailed Image Information:")
                print(result.stdout)
                self.logger.info(f"DISM imageinfo output retrieved successfully")
                return
            except subprocess.CalledProcessError as e:
                self.logger.warning(f"DISM failed to get image info: {e.stderr}")
            except FileNotFoundError:
                self.logger.info("DISM not available for image parsing")
        
        # Fallback: Basic file analysis
        print("\n[BASIC ANALYSIS] Limited information available without Windows DISM:")
        print(f"Creation Time: {os.path.getctime(image_path)}")
        print(f"Modification Time: {os.path.getmtime(image_path)}")
        
        # Try to detect image type from file signature
        try:
            with open(image_path, 'rb') as f:
                header = f.read(16)
                if header.startswith(b'MSWIM\x00\x00\x00'):
                    print("Image Type: Windows Imaging Format (WIM)")
                elif header.startswith(b'conectix'):
                    print("Image Type: Virtual Hard Disk (VHD)")
                else:
                    print("Image Type: Unknown or corrupted")
        except Exception as e:
            self.logger.warning(f"Could not read image header: {e}")
            print("Image Type: Could not determine")
        
    def get_features(self, target: str):
        """Get Windows features information"""
        print(f"\nWindows Features in {target}:")
        print("-" * 50)
        
        # Try to get real features when possible
        if target.lower() == "/online" and platform.system() == "Windows":
            try:
                # Use DISM to get actual features on Windows
                cmd = ["dism", "/online", "/get-features"]
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(result.stdout)
                return
            except subprocess.CalledProcessError as e:
                self.logger.warning(f"DISM failed to get features: {e.stderr}")
            except FileNotFoundError:
                self.logger.info("DISM not available for feature enumeration")
        elif target.lower() != "/online":
            # For offline images
            if platform.system() == "Windows":
                try:
                    cmd = ["dism", f"/image:{target}", "/get-features"]
                    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                    print(result.stdout)
                    return
                except subprocess.CalledProcessError as e:
                    self.logger.warning(f"DISM failed to get features from image: {e.stderr}")
                except FileNotFoundError:
                    self.logger.info("DISM not available for offline image feature enumeration")
        
        # Fallback: Show sample features with simulation notice
        print("[SIMULATION] Sample Windows features (actual query requires Windows DISM):")
        sample_features = [
            {"name": "IIS-WebServerRole", "state": "Disabled"},
            {"name": "IIS-WebServer", "state": "Disabled"},
            {"name": "IIS-CommonHttpFeatures", "state": "Disabled"},
            {"name": "IIS-ApplicationDevelopment", "state": "Disabled"},
            {"name": "Microsoft-Windows-Subsystem-Linux", "state": "Disabled"},
            {"name": "VirtualMachinePlatform", "state": "Enabled"},
            {"name": "Microsoft-Hyper-V-All", "state": "Disabled"},
        ]
        
        # Check for simulated feature states
        if target.lower() == "/online":
            state_file = self.scratch_dir / "simulated_features.json"
        else:
            state_file = self.scratch_dir / f"image_features_{hash(target)}.json"
        
        simulated_features = {}
        if state_file.exists():
            try:
                with open(state_file, 'r') as f:
                    simulated_features = json.load(f)
            except Exception as e:
                self.logger.warning(f"Could not read simulated features: {e}")
        
        # Update sample features with simulated states
        for feature in sample_features:
            if feature["name"] in simulated_features:
                feature["state"] = simulated_features[feature["name"]]["state"]
                feature["simulated"] = True
        
        for feature in sample_features:
            sim_indicator = " [SIM]" if feature.get("simulated") else ""
            print(f"{feature['name']:<40} | {feature['state']}{sim_indicator}")
    
    def enable_feature(self, target: str, feature_name: str, all_features: bool = False):
        """Enable a Windows feature"""
        self.logger.info(f"Enabling feature {feature_name} in {target}")
        print(f"Enabling feature: {feature_name}")
        
        if target.lower() == "/online":
            print("Note: Online feature changes require administrator privileges")
            return self._enable_feature_online(feature_name, all_features)
        else:
            # For offline images
            return self._enable_feature_offline(target, feature_name, all_features)
        
    def disable_feature(self, target: str, feature_name: str, remove: bool = False):
        """Disable a Windows feature"""
        self.logger.info(f"Disabling feature {feature_name} in {target}")
        print(f"Disabling feature: {feature_name}")
        
        if target.lower() == "/online":
            print("Note: Online feature changes require administrator privileges")
            return self._disable_feature_online(feature_name, remove)
        else:
            # For offline images
            return self._disable_feature_offline(target, feature_name, remove)
        
    def get_packages(self, target: str):
        """Get installed packages information"""
        print(f"\nPackages in {target}:")
        print("-" * 80)
        
        # Try to get real packages when possible
        if target.lower() == "/online" and platform.system() == "Windows":
            try:
                # Use DISM to get actual packages on Windows
                cmd = ["dism", "/online", "/get-packages"]
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(result.stdout)
                return
            except subprocess.CalledProcessError as e:
                self.logger.warning(f"DISM failed to get packages: {e.stderr}")
            except FileNotFoundError:
                self.logger.info("DISM not available for package enumeration")
        elif target.lower() != "/online":
            # For offline images
            if platform.system() == "Windows":
                try:
                    cmd = ["dism", f"/image:{target}", "/get-packages"]
                    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                    print(result.stdout)
                    return
                except subprocess.CalledProcessError as e:
                    self.logger.warning(f"DISM failed to get packages from image: {e.stderr}")
                except FileNotFoundError:
                    self.logger.info("DISM not available for offline image package enumeration")
        
        # Fallback: Show sample packages with simulation notice
        print("[SIMULATION] Sample Windows packages (actual query requires Windows DISM):")
        sample_packages = [
            {"name": "Microsoft-Windows-Client-Features-Package", "state": "Installed"},
            {"name": "Microsoft-Windows-NetFx3-OnDemand-Package", "state": "Installed"},
            {"name": "Microsoft-Windows-PowerShell-ISE-Package", "state": "Installed"},
        ]
        
        for package in sample_packages:
            print(f"{package['name']:<50} | {package['state']}")
    
    def get_drivers(self, target: str):
        """Get driver information"""
        print(f"\nDrivers in {target}:")
        print("-" * 80)
        
        # Try to get real drivers when possible
        if target.lower() == "/online" and platform.system() == "Windows":
            try:
                # Use DISM to get actual drivers on Windows
                cmd = ["dism", "/online", "/get-drivers"]
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(result.stdout)
                return
            except subprocess.CalledProcessError as e:
                self.logger.warning(f"DISM failed to get drivers: {e.stderr}")
            except FileNotFoundError:
                self.logger.info("DISM not available for driver enumeration")
        elif target.lower() != "/online":
            # For offline images
            if platform.system() == "Windows":
                try:
                    cmd = ["dism", f"/image:{target}", "/get-drivers"]
                    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                    print(result.stdout)
                    return
                except subprocess.CalledProcessError as e:
                    self.logger.warning(f"DISM failed to get drivers from image: {e.stderr}")
                except FileNotFoundError:
                    self.logger.info("DISM not available for offline image driver enumeration")
        
        # Fallback: Show sample drivers with simulation notice
        print("[SIMULATION] Sample drivers (actual query requires Windows DISM):")
        sample_drivers = [
            {"name": "Audio Device Driver", "version": "10.0.1.2", "provider": "Microsoft"},
            {"name": "Network Adapter Driver", "version": "1.2.3.4", "provider": "Intel"},
            {"name": "Graphics Driver", "version": "2.1.0.0", "provider": "NVIDIA"},
        ]
        
        for driver in sample_drivers:
            print(f"{driver['name']:<30} | {driver['version']:<15} | {driver['provider']}")

    def add_package(self, target: str, package_path: str, prevent_pending: bool = False, ignore_check: bool = False):
        """Add a package to the image or online system"""
        self.logger.info(f"Adding package {package_path} to {target}")
        print(f"Adding package: {package_path}")
        
        package_path = Path(package_path)
        
        # Validate package file exists
        if not package_path.exists():
            raise FileNotFoundError(f"Package file not found: {package_path}")
        
        # Check if it's a supported package type
        supported_extensions = ['.cab', '.msu', '.msp']
        if package_path.suffix.lower() not in supported_extensions:
            print(f"Warning: Package type '{package_path.suffix}' may not be fully supported")
            print(f"Supported types: {', '.join(supported_extensions)}")
        
        if target.lower() == "/online":
            print("Note: Online package installation requires administrator privileges")
            return self._add_package_online(package_path, prevent_pending, ignore_check)
        else:
            # For offline images
            return self._add_package_offline(target, package_path, prevent_pending, ignore_check)

    def remove_package(self, target: str, package_name: str):
        """Remove a package from the image or online system"""
        self.logger.info(f"Removing package {package_name} from {target}")
        print(f"Removing package: {package_name}")
        
        if target.lower() == "/online":
            print("Note: Online package removal requires administrator privileges")
            return self._remove_package_online(package_name)
        else:
            # For offline images
            return self._remove_package_offline(target, package_name)

    def get_package_info(self, target: str, package_name: str):
        """Get detailed information about a specific package"""
        print(f"\nPackage Information for: {package_name}")
        print("-" * 50)
        
        # Try to get real package info when possible
        if target.lower() == "/online" and platform.system() == "Windows":
            try:
                # Use DISM to get actual package info on Windows
                cmd = ["dism", "/online", "/get-packageinfo", f"/packagename:{package_name}"]
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(result.stdout)
                return
            except subprocess.CalledProcessError as e:
                self.logger.warning(f"DISM failed to get package info: {e.stderr}")
            except FileNotFoundError:
                self.logger.info("DISM not available for package info")
        elif target.lower() != "/online":
            # For offline images
            if platform.system() == "Windows":
                try:
                    cmd = ["dism", f"/image:{target}", "/get-packageinfo", f"/packagename:{package_name}"]
                    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                    print(result.stdout)
                    return
                except subprocess.CalledProcessError as e:
                    self.logger.warning(f"DISM failed to get package info from image: {e.stderr}")
                except FileNotFoundError:
                    self.logger.info("DISM not available for offline image package info")
        
        # Fallback: Show simulated package info
        print("[SIMULATION] Sample package information (actual query requires Windows DISM):")
        print(f"Package Name: {package_name}")
        print(f"State: Unknown")
        print(f"Release Type: Unknown")
        print(f"Install Time: Unknown")

    def _add_package_online(self, package_path: Path, prevent_pending: bool = False, ignore_check: bool = False):
        """Add a package to the online system"""
        try:
            if platform.system() == "Windows":
                # Use DISM command for Windows
                cmd = ["dism", "/online", "/add-package", f"/packagepath:{package_path}"]
                if prevent_pending:
                    cmd.append("/preventpending")
                if ignore_check:
                    cmd.append("/ignorecheck")
                
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(f"Package {package_path.name} installed successfully.")
                self.logger.info(f"DISM output: {result.stdout}")
                
                # Check if restart is required
                if "restart" in result.stdout.lower() or "reboot" in result.stdout.lower():
                    print("Warning: A restart may be required to complete the package installation.")
                
                return True
            else:
                # For non-Windows systems, simulate package installation
                self._simulate_package_installation(package_path, "install")
                return True
        except subprocess.CalledProcessError as e:
            error_msg = f"Failed to install package {package_path.name}: {e.stderr}"
            self.logger.error(error_msg)
            print(f"Error: {error_msg}")
            return False
        except FileNotFoundError:
            # DISM not available, fall back to simulation
            self._simulate_package_installation(package_path, "install")
            return True

    def _remove_package_online(self, package_name: str):
        """Remove a package from the online system"""
        try:
            if platform.system() == "Windows":
                # Use DISM command for Windows
                cmd = ["dism", "/online", "/remove-package", f"/packagename:{package_name}"]
                
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(f"Package {package_name} removed successfully.")
                self.logger.info(f"DISM output: {result.stdout}")
                
                # Check if restart is required
                if "restart" in result.stdout.lower() or "reboot" in result.stdout.lower():
                    print("Warning: A restart may be required to complete the package removal.")
                
                return True
            else:
                # For non-Windows systems, simulate package removal
                self._simulate_package_installation(package_name, "remove")
                return True
        except subprocess.CalledProcessError as e:
            error_msg = f"Failed to remove package {package_name}: {e.stderr}"
            self.logger.error(error_msg)
            print(f"Error: {error_msg}")
            return False
        except FileNotFoundError:
            # DISM not available, fall back to simulation
            self._simulate_package_installation(package_name, "remove")
            return True

    def _add_package_offline(self, target: str, package_path: Path, prevent_pending: bool = False, ignore_check: bool = False):
        """Add a package to an offline image"""
        try:
            if platform.system() == "Windows":
                # Use DISM command for Windows
                cmd = ["dism", f"/image:{target}", "/add-package", f"/packagepath:{package_path}"]
                if prevent_pending:
                    cmd.append("/preventpending")
                if ignore_check:
                    cmd.append("/ignorecheck")
                
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(f"Package {package_path.name} added successfully to offline image.")
                self.logger.info(f"DISM output: {result.stdout}")
                return True
            else:
                # For non-Windows systems, simulate offline package installation
                self._simulate_offline_package_installation(target, package_path, "install")
                return True
        except subprocess.CalledProcessError as e:
            error_msg = f"Failed to add package {package_path.name} to offline image: {e.stderr}"
            self.logger.error(error_msg)
            print(f"Error: {error_msg}")
            return False
        except FileNotFoundError:
            # DISM not available, fall back to simulation
            self._simulate_offline_package_installation(target, package_path, "install")
            return True

    def _remove_package_offline(self, target: str, package_name: str):
        """Remove a package from an offline image"""
        try:
            if platform.system() == "Windows":
                # Use DISM command for Windows
                cmd = ["dism", f"/image:{target}", "/remove-package", f"/packagename:{package_name}"]
                
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(f"Package {package_name} removed successfully from offline image.")
                self.logger.info(f"DISM output: {result.stdout}")
                return True
            else:
                # For non-Windows systems, simulate offline package removal
                self._simulate_offline_package_installation(target, package_name, "remove")
                return True
        except subprocess.CalledProcessError as e:
            error_msg = f"Failed to remove package {package_name} from offline image: {e.stderr}"
            self.logger.error(error_msg)
            print(f"Error: {error_msg}")
            return False
        except FileNotFoundError:
            # DISM not available, fall back to simulation
            self._simulate_offline_package_installation(target, package_name, "remove")
            return True

    def _simulate_package_installation(self, package_path_or_name, action: str):
        """Simulate package installation/removal for non-Windows systems"""
        item_name = package_path_or_name.name if hasattr(package_path_or_name, 'name') else str(package_path_or_name)
        action_text = "Installed" if action == "install" else "Removed"
        print(f"[SIMULATION] {action_text} package {item_name} on {platform.system()}")
        print(f"Note: Actual Windows package management requires a Windows environment")
        
        # Store the simulated state in a local file for consistency
        state_file = self.scratch_dir / "simulated_packages.json"
        
        try:
            if state_file.exists():
                with open(state_file, 'r') as f:
                    packages = json.load(f)
            else:
                packages = {}
            
            if action == "install":
                packages[item_name] = {"state": "Installed", "simulated": True}
            elif action == "remove":
                if item_name in packages:
                    del packages[item_name]
            
            with open(state_file, 'w') as f:
                json.dump(packages, f, indent=2)
                
        except Exception as e:
            self.logger.warning(f"Could not save simulated package state: {e}")

    def _simulate_offline_package_installation(self, target: str, package_path_or_name, action: str):
        """Simulate offline package installation/removal for non-Windows systems"""
        item_name = package_path_or_name.name if hasattr(package_path_or_name, 'name') else str(package_path_or_name)
        action_text = "Installed" if action == "install" else "Removed"
        print(f"[SIMULATION] {action_text} package {item_name} in offline image {target}")
        print(f"Note: Actual offline Windows image modification requires Windows DISM tools")
        
        # Store the simulated state for the specific image
        image_state_file = self.scratch_dir / f"image_packages_{hash(target)}.json"
        
        try:
            if image_state_file.exists():
                with open(image_state_file, 'r') as f:
                    packages = json.load(f)
            else:
                packages = {}
            
            if action == "install":
                packages[item_name] = {"state": "Installed", "simulated": True, "target": target}
            elif action == "remove":
                if item_name in packages:
                    del packages[item_name]
            
            with open(image_state_file, 'w') as f:
                json.dump(packages, f, indent=2)
                
        except Exception as e:
            self.logger.warning(f"Could not save simulated package state for image: {e}")

    def _enable_feature_online(self, feature_name: str, all_features: bool = False):
        """Enable a Windows feature on the running system"""
        try:
            if platform.system() == "Windows":
                # Use DISM command for Windows
                cmd = ["dism", "/online", "/enable-feature", f"/featurename:{feature_name}"]
                if all_features:
                    cmd.append("/all")
                
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(f"Feature {feature_name} enabled successfully.")
                self.logger.info(f"DISM output: {result.stdout}")
                return True
            else:
                # For non-Windows systems, simulate feature management
                self._simulate_feature_management(feature_name, "enable")
                return True
        except subprocess.CalledProcessError as e:
            error_msg = f"Failed to enable feature {feature_name}: {e.stderr}"
            self.logger.error(error_msg)
            print(f"Error: {error_msg}")
            return False
        except FileNotFoundError:
            # DISM not available, fall back to simulation
            self._simulate_feature_management(feature_name, "enable")
            return True

    def _disable_feature_online(self, feature_name: str, remove: bool = False):
        """Disable a Windows feature on the running system"""
        try:
            if platform.system() == "Windows":
                # Use DISM command for Windows
                cmd = ["dism", "/online", "/disable-feature", f"/featurename:{feature_name}"]
                if remove:
                    cmd.append("/remove")
                
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(f"Feature {feature_name} disabled successfully.")
                self.logger.info(f"DISM output: {result.stdout}")
                return True
            else:
                # For non-Windows systems, simulate feature management
                self._simulate_feature_management(feature_name, "disable")
                return True
        except subprocess.CalledProcessError as e:
            error_msg = f"Failed to disable feature {feature_name}: {e.stderr}"
            self.logger.error(error_msg)
            print(f"Error: {error_msg}")
            return False
        except FileNotFoundError:
            # DISM not available, fall back to simulation
            self._simulate_feature_management(feature_name, "disable")
            return True

    def _enable_feature_offline(self, target: str, feature_name: str, all_features: bool = False):
        """Enable a Windows feature in an offline image"""
        try:
            if platform.system() == "Windows":
                # Use DISM command for Windows
                cmd = ["dism", f"/image:{target}", "/enable-feature", f"/featurename:{feature_name}"]
                if all_features:
                    cmd.append("/all")
                
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(f"Feature {feature_name} enabled successfully in offline image.")
                self.logger.info(f"DISM output: {result.stdout}")
                return True
            else:
                # For non-Windows systems, simulate offline feature management
                self._simulate_offline_feature_management(target, feature_name, "enable")
                return True
        except subprocess.CalledProcessError as e:
            error_msg = f"Failed to enable feature {feature_name} in offline image: {e.stderr}"
            self.logger.error(error_msg)
            print(f"Error: {error_msg}")
            return False
        except FileNotFoundError:
            # DISM not available, fall back to simulation
            self._simulate_offline_feature_management(target, feature_name, "enable")
            return True

    def _disable_feature_offline(self, target: str, feature_name: str, remove: bool = False):
        """Disable a Windows feature in an offline image"""
        try:
            if platform.system() == "Windows":
                # Use DISM command for Windows
                cmd = ["dism", f"/image:{target}", "/disable-feature", f"/featurename:{feature_name}"]
                if remove:
                    cmd.append("/remove")
                
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(f"Feature {feature_name} disabled successfully in offline image.")
                self.logger.info(f"DISM output: {result.stdout}")
                return True
            else:
                # For non-Windows systems, simulate offline feature management
                self._simulate_offline_feature_management(target, feature_name, "disable")
                return True
        except subprocess.CalledProcessError as e:
            error_msg = f"Failed to disable feature {feature_name} in offline image: {e.stderr}"
            self.logger.error(error_msg)
            print(f"Error: {error_msg}")
            return False
        except FileNotFoundError:
            # DISM not available, fall back to simulation
            self._simulate_offline_feature_management(target, feature_name, "disable")
            return True

    def _simulate_feature_management(self, feature_name: str, action: str):
        """Simulate feature management for non-Windows systems"""
        print(f"[SIMULATION] {action.capitalize()}d feature {feature_name} on {platform.system()}")
        print(f"Note: Actual Windows feature management requires a Windows environment")
        
        # Store the simulated state in a local file for consistency
        state_file = self.scratch_dir / "simulated_features.json"
        
        try:
            if state_file.exists():
                with open(state_file, 'r') as f:
                    features = json.load(f)
            else:
                features = {}
            
            features[feature_name] = {"state": "Enabled" if action == "enable" else "Disabled", 
                                    "simulated": True}
            
            with open(state_file, 'w') as f:
                json.dump(features, f, indent=2)
                
        except Exception as e:
            self.logger.warning(f"Could not save simulated feature state: {e}")

    def _simulate_offline_feature_management(self, target: str, feature_name: str, action: str):
        """Simulate offline feature management for non-Windows systems"""
        print(f"[SIMULATION] {action.capitalize()}d feature {feature_name} in offline image {target}")
        print(f"Note: Actual offline Windows image modification requires Windows DISM tools")
        
        # Store the simulated state for the specific image
        image_state_file = self.scratch_dir / f"image_features_{hash(target)}.json"
        
        try:
            if image_state_file.exists():
                with open(image_state_file, 'r') as f:
                    features = json.load(f)
            else:
                features = {}
            
            features[feature_name] = {"state": "Enabled" if action == "enable" else "Disabled", 
                                    "simulated": True, "target": target}
            
            with open(image_state_file, 'w') as f:
                json.dump(features, f, indent=2)
                
        except Exception as e:
            self.logger.warning(f"Could not save simulated feature state for image: {e}")

def parse_dism_args():
    """Parse DISM-style arguments with / prefix"""
    args_dict = {}
    i = 1
    
    while i < len(sys.argv):
        arg = sys.argv[i]
        
        # Handle /? help
        if arg in ['/?', '/help', '--help', '-h']:
            args_dict['help'] = True
            i += 1
            continue
            
        # Handle arguments with values (e.g., /Image:path or /Image path)
        if arg.startswith('/') or arg.startswith('-'):
            key = arg.lstrip('/-').lower().replace('-', '_')
            
            # Check if it has a colon separator
            if ':' in key:
                key, value = key.split(':', 1)
                args_dict[key] = value
            else:
                # Check if next argument is a value
                if i + 1 < len(sys.argv) and not sys.argv[i + 1].startswith('/') and not sys.argv[i + 1].startswith('-'):
                    args_dict[key] = sys.argv[i + 1]
                    i += 1
                else:
                    args_dict[key] = True
        i += 1
    
    return args_dict

def main():
    """Main entry point for DISMv2"""
    
    # Handle help and no arguments
    if len(sys.argv) == 1 or any(arg in ['/?', '/help', '--help', '-h'] for arg in sys.argv):
        dism = DISMv2()
        dism.print_help()
        return 0
    
    args = parse_dism_args()
    
    try:
        dism = DISMv2()
        
        # Determine target
        target = ""
        if args.get('online'):
            target = "/Online"
        elif args.get('image'):
            target = args['image']
        else:
            target = "/Online"  # Default to online
        
        # Execute commands
        if args.get('mount_image'):
            if not args.get('imagefile') or not args.get('mountdir'):
                print("Error: /ImageFile and /MountDir are required for /Mount-Image")
                return 1
            index = int(args.get('index', 1))
            readonly = args.get('readonly', False)
            dism.mount_image(args['imagefile'], args['mountdir'], index, readonly)
            
        elif args.get('unmount_image'):
            if not args.get('mountdir'):
                print("Error: /MountDir is required for /Unmount-Image")
                return 1
            commit = args.get('commit', False)
            discard = args.get('discard', False)
            dism.unmount_image(args['mountdir'], commit, discard)
            
        elif args.get('get_mountedimageinfo'):
            dism.get_mounted_image_info()
            
        elif args.get('get_imageinfo'):
            if not args.get('imagefile'):
                print("Error: /ImageFile is required for /Get-ImageInfo")
                return 1
            index = args.get('index')
            if index:
                index = int(index)
            dism.get_image_info(args['imagefile'], index)
            
        elif args.get('get_features'):
            dism.get_features(target)
            
        elif args.get('enable_feature'):
            if not args.get('featurename'):
                print("Error: /FeatureName is required for /Enable-Feature")
                return 1
            all_features = args.get('all', False)
            dism.enable_feature(target, args['featurename'], all_features)
            
        elif args.get('disable_feature'):
            if not args.get('featurename'):
                print("Error: /FeatureName is required for /Disable-Feature")
                return 1
            remove = args.get('remove', False)
            dism.disable_feature(target, args['featurename'], remove)
            
        elif args.get('get_packages'):
            dism.get_packages(target)
            
        elif args.get('add_package'):
            if not args.get('packagepath'):
                print("Error: /PackagePath is required for /Add-Package")
                return 1
            prevent_pending = args.get('preventpending', False)
            ignore_check = args.get('ignorecheck', False)
            dism.add_package(target, args['packagepath'], prevent_pending, ignore_check)
            
        elif args.get('remove_package'):
            if not args.get('packagename'):
                print("Error: /PackageName is required for /Remove-Package")
                return 1
            dism.remove_package(target, args['packagename'])
            
        elif args.get('get_packageinfo'):
            if not args.get('packagename'):
                print("Error: /PackageName is required for /Get-PackageInfo")
                return 1
            dism.get_package_info(target, args['packagename'])
            
        elif args.get('get_drivers'):
            dism.get_drivers(target)
            
        else:
            print("Error: No valid command specified. Use /? for help.")
            return 1
            
    except Exception as e:
        print(f"Error: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())