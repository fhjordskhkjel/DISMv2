#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include "CabHandler.h"

namespace fs = std::filesystem;

void printUsage() {
    std::cout << "CAB File Handler - Universal Windows Package Manager (C++20 Enhanced with PSF/WIM Support)\n";
    std::cout << "Usage: TestAI.exe <command> <options>\n\n";
    std::cout << "Commands:\n";
    std::cout << "  extract <cabfile> <destination>     - Extract CAB file to destination folder\n";
    std::cout << "  add <cabfile> <source>              - Add files from source to CAB file\n";
    std::cout << "  list <cabfile>                      - List contents of CAB file\n";
    std::cout << "  create <cabfile> <source>           - Create new CAB file from source folder\n";
    std::cout << "  verify <cabfile>                    - Verify CAB file integrity\n";
    std::cout << "  add-package <package> [options]     - Install Windows Update package (DISM-like)\n";
    std::cout << "  add-package-enhanced <package> [options] - ?? Enhanced DISM w/ PSF/WIM integration\n";
    std::cout << "\n?? NEW: PSF/WIM Commands:\n";
    std::cout << "  extract-psf <psffile> <destination> - Extract PSF/APPX package\n";
    std::cout << "  extract-wim <wimfile> <index> <dest>- Extract WIM image by index\n";
    std::cout << "  list-psf <psffile>                  - List PSF package information\n";
    std::cout << "  list-wim <wimfile>                  - List WIM images\n";
    std::cout << "  apply-wim <wimfile> <index> <dest>  - Apply WIM image to destination\n";
    std::cout << "  capture-wim <source> <wimfile> <name> - Capture directory to WIM\n";
    std::cout << "  detect-type <packagefile>           - Detect package type (CAB/MSU/PSF/WIM)\n";
    std::cout << "  extract-advanced <package> <dest>   - Auto-detect and extract any package type\n";
    std::cout << "\nAdd-Package Options:\n";
    std::cout << "  /PackagePath:<path>                 - Path to package file (.msu/.cab/.psf/.wim)\n";
    std::cout << "  /ExtractedDir:<path>                - ?? Path to already extracted directory\n";
    std::cout << "  /Image:<path>                       - Path to Windows image directory\n";
    std::cout << "  /Online                             - Target the running operating system\n";
    std::cout << "  /ImageIndex:<number>                - ?? WIM image index (default: 1)\n";
    std::cout << "  /LogPath:<path>                     - Path to log file (optional)\n";
    std::cout << "  /Quiet                              - Suppress output (optional)\n";
    std::cout << "  /NoRestart                          - Suppress restart prompts (optional)\n";
    std::cout << "  /CBS                                - ?? Enable Component-Based Servicing\n";
    std::cout << "\nExamples:\n";
    std::cout << "  TestAI.exe extract update.cab C:\\temp\\extracted\n";
    std::cout << "  TestAI.exe create myfiles.cab C:\\source\\folder\n";
    std::cout << "  TestAI.exe add-package /PackagePath:update.msu /Image:C:\\mount\n";
    std::cout << "  TestAI.exe add-package /PackagePath:KB123456.cab /Online\n";
    std::cout << "  TestAI.exe add-package /ExtractedDir:C:\\extracted_msu /Online\n";
    std::cout << "\n?? PSF/WIM Examples:\n";
    std::cout << "  TestAI.exe extract-psf MyApp.appx C:\\temp\\extracted\n";
    std::cout << "  TestAI.exe extract-wim install.wim 1 C:\\temp\\image\n";
    std::cout << "  TestAI.exe add-package /PackagePath:MyApp.psf /Online\n";
    std::cout << "  TestAI.exe add-package /PackagePath:install.wim /ImageIndex:1 /Image:C:\\mount\n";
    std::cout << "  TestAI.exe apply-wim install.wim 1 C:\\target\n";
    std::cout << "  TestAI.exe detect-type mysterious_package.bin\n";
    std::cout << "  TestAI.exe extract-advanced update_package.unknown C:\\temp\n";
    std::cout << "\n?? Enhanced DISM Examples:\n";
    std::cout << "  TestAI.exe add-package-enhanced /PackagePath:complex_update.msu /Online /CBS\n";
    std::cout << "  TestAI.exe add-package-enhanced /ExtractedDir:C:\\mixed_packages /Image:C:\\mount\n";
    std::cout << "  TestAI.exe add-package-enhanced /PackagePath:enterprise.wim /ImageIndex:2 /Online\n";
}

// Parse DISM-style arguments for add-package command
struct AddPackageArgs {
    std::string packagePath;
    std::string extractedDir;  // ?? NEW: For already extracted MSU directories
    std::string imagePath;
    std::string logPath;
    int imageIndex = 1;        // ?? NEW: For WIM image index
    bool quiet = false;
    bool noRestart = false;
    bool online = false;
    bool useExtractedDir = false;  // ?? NEW: Flag for using extracted directory
    bool useCbs = false;       // ?? NEW: Enable CBS integration
};

AddPackageArgs parseAddPackageArgs(int argc, char* argv[], int startIndex) {
    AddPackageArgs args;
    
    for (int i = startIndex; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg.find("/PackagePath:") == 0) {
            args.packagePath = arg.substr(13);
        }
        else if (arg.find("/ExtractedDir:") == 0) {  // ?? NEW: Support for extracted directories
            args.extractedDir = arg.substr(14);
            args.useExtractedDir = true;
        }
        else if (arg.find("/Image:") == 0) {
            args.imagePath = arg.substr(7);
        }
        else if (arg.find("/ImageIndex:") == 0) {  // ?? NEW: WIM image index support
            try {
                args.imageIndex = std::stoi(arg.substr(12));
            } catch (...) {
                args.imageIndex = 1; // Default to first image
            }
        }
        else if (arg.find("/LogPath:") == 0) {
            args.logPath = arg.substr(9);
        }
        else if (arg == "/Quiet") {
            args.quiet = true;
        }
        else if (arg == "/NoRestart") {
            args.noRestart = true;
        }
        else if (arg == "/Online") {
            args.online = true;
        }
        else if (arg == "/CBS") {  // ?? NEW: CBS integration flag
            args.useCbs = true;
        }
    }
    
    return args;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];
    
    try {
        CabHandler handler;

        // ?? NEW: PSF/WIM Commands
        if (command == "extract-psf") {
            if (argc < 4) {
                std::cerr << "Error: Destination path required for extract-psf command\n";
                return 1;
            }
            std::string psfFile = argv[2];
            std::string destination = argv[3];
            std::cout << "Extracting PSF package " << psfFile << " to " << destination << "...\n";
            
            if (handler.extractPsf(psfFile, destination)) {
                std::cout << "PSF extraction completed successfully.\n";
            } else {
                std::cerr << "PSF extraction failed.\n";
                return 1;
            }
        }
        else if (command == "extract-wim") {
            if (argc < 5) {
                std::cerr << "Error: WIM file, image index, and destination path required\n";
                return 1;
            }
            std::string wimFile = argv[2];
            int imageIndex = std::stoi(argv[3]);
            std::string destination = argv[4];
            std::cout << "Extracting WIM image " << imageIndex << " from " << wimFile << " to " << destination << "...\n";
            
            if (handler.extractWim(wimFile, imageIndex, destination)) {
                std::cout << "WIM extraction completed successfully.\n";
            } else {
                std::cerr << "WIM extraction failed.\n";
                return 1;
            }
        }
        else if (command == "list-psf") {
            std::string psfFile = argv[2];
            std::cout << "Listing PSF package information for " << psfFile << ":\n";
            
            std::vector<PsfPackageInfo> packages;
            if (handler.listPsfContents(psfFile, packages)) {
                for (const auto& pkg : packages) {
                    std::cout << "  Package Name: " << pkg.packageName << "\n";
                    std::cout << "  Version: " << pkg.version << "\n";
                    std::cout << "  Architecture: " << pkg.architecture << "\n";
                    std::cout << "  Display Name: " << pkg.displayName << "\n";
                    std::cout << "  Applicable: " << (pkg.isApplicable ? "Yes" : "No") << "\n";
                }
            } else {
                std::cerr << "Failed to list PSF contents.\n";
                return 1;
            }
        }
        else if (command == "list-wim") {
            std::string wimFile = argv[2];
            std::cout << "Listing WIM images in " << wimFile << ":\n";
            
            std::vector<WimImageInfo> images;
            if (handler.listWimImages(wimFile, images)) {
                for (const auto& img : images) {
                    std::cout << "  Index: " << img.imageIndex << "\n";
                    std::cout << "  Name: " << img.imageName << "\n";
                    std::cout << "  Description: " << img.description << "\n";
                    std::cout << "  Architecture: " << img.architecture << "\n";
                    std::cout << "  Version: " << img.version << "\n";
                    std::cout << "  Bootable: " << (img.bootable ? "Yes" : "No") << "\n";
                    std::cout << "  ---\n";
                }
            } else {
                std::cerr << "Failed to list WIM images.\n";
                return 1;
            }
        }
        else if (command == "apply-wim") {
            if (argc < 5) {
                std::cerr << "Error: WIM file, image index, and destination path required\n";
                return 1;
            }
            std::string wimFile = argv[2];
            int imageIndex = std::stoi(argv[3]);
            std::string destination = argv[4];
            std::cout << "Applying WIM image " << imageIndex << " from " << wimFile << " to " << destination << "...\n";
            
            if (handler.applyWimImage(wimFile, imageIndex, destination)) {
                std::cout << "WIM image applied successfully.\n";
            } else {
                std::cerr << "WIM image application failed.\n";
                return 1;
            }
        }
        else if (command == "capture-wim") {
            if (argc < 5) {
                std::cerr << "Error: Source path, WIM file, and image name required\n";
                return 1;
            }
            std::string sourcePath = argv[2];
            std::string wimFile = argv[3];
            std::string imageName = argv[4];
            std::string description = argc > 5 ? argv[5] : "";
            
            std::cout << "Capturing " << sourcePath << " to WIM file " << wimFile << "...\n";
            
            if (handler.captureWimImage(sourcePath, wimFile, imageName, description)) {
                std::cout << "WIM image captured successfully.\n";
            } else {
                std::cerr << "WIM image capture failed.\n";
                return 1;
            }
        }
        else if (command == "detect-type") {
            std::string packageFile = argv[2];
            std::string detectedType;
            
            if (handler.detectPackageType(packageFile, detectedType)) {
                std::cout << "Package type detected: " << detectedType << "\n";
            } else {
                std::cout << "Unable to detect package type for: " << packageFile << "\n";
                return 1;
            }
        }
        else if (command == "extract-advanced") {
            if (argc < 4) {
                std::cerr << "Error: Package file and destination path required\n";
                return 1;
            }
            std::string packageFile = argv[2];
            std::string destination = argv[3];
            
            std::cout << "Auto-detecting and extracting package: " << packageFile << "\n";
            
            if (handler.extractPackageAdvanced(packageFile, destination)) {
                std::cout << "Advanced package extraction completed successfully.\n";
            } else {
                std::cerr << "Advanced package extraction failed.\n";
                return 1;
            }
        }
        else if (command == "add-package") {
            AddPackageArgs args = parseAddPackageArgs(argc, argv, 2);
            
            // Validate input parameters
            if (!args.useExtractedDir && args.packagePath.empty()) {
                std::cerr << "Error: /PackagePath parameter is required when not using /ExtractedDir\n";
                return 1;
            }
            
            if (args.useExtractedDir && args.extractedDir.empty()) {
                std::cerr << "Error: /ExtractedDir parameter cannot be empty\n";
                return 1;
            }
            
            if (args.useExtractedDir && !args.packagePath.empty()) {
                std::cerr << "Error: Cannot specify both /PackagePath and /ExtractedDir parameters\n";
                return 1;
            }
            
            // Validate that either /Image or /Online is specified, but not both
            if (args.online && !args.imagePath.empty()) {
                std::cerr << "Error: Cannot specify both /Online and /Image parameters\n";
                return 1;
            }
            
            if (!args.online && args.imagePath.empty()) {
                std::cerr << "Error: Either /Image or /Online parameter is required for add-package command\n";
                return 1;
            }
            
            // Enable CBS integration if requested
            if (args.useCbs) {
                handler.enableCbsIntegration(true);
                if (!args.quiet) {
                    std::cout << "CBS (Component-Based Servicing) integration enabled\n";
                }
            }
            
            // Check for admin privileges when using /Online
            if (args.online) {
                if (!handler.checkAdminPrivileges()) {
                    std::cerr << "Error: Administrator privileges are required for /Online operations\n";
                    std::cerr << "Please run this application as an administrator\n";
                    return 1;
                }
                
                if (!args.quiet) {
                    if (args.useExtractedDir) {
                        std::cout << "Installing from extracted directory " << args.extractedDir << " to the running operating system...\n";
                    } else {
                        std::cout << "Installing package " << args.packagePath << " to the running operating system...\n";
                    }
                }
            } else {
                if (!args.quiet) {
                    if (args.useExtractedDir) {
                        std::cout << "Installing from extracted directory " << args.extractedDir << " to image " << args.imagePath << "...\n";
                    } else {
                        std::cout << "Installing package " << args.packagePath << " to image " << args.imagePath << "...\n";
                    }
                }
            }
            
            bool success = false;
            
            // ?? NEW: Handle extracted directory installation
            if (args.useExtractedDir) {
                if (args.useCbs) {
                    if (args.online) {
                        // CBS-integrated online installation from extracted directory
                        success = handler.installExtractedPackageWithCbs(args.extractedDir, "", args.logPath, args.quiet);
                    } else {
                        // CBS-integrated offline installation from extracted directory  
                        success = handler.installExtractedPackageWithCbs(args.extractedDir, args.imagePath, args.logPath, args.quiet);
                    }
                } else {
                    if (args.online) {
                        success = handler.installFromExtractedMsuOnline(args.extractedDir, args.logPath, args.quiet);
                    } else {
                        success = handler.installFromExtractedMsu(args.extractedDir, args.imagePath, args.logPath, args.quiet);
                    }
                }
            }
            // Handle package file installation
            else {
                // Detect package type
                std::string packageType;
                if (!handler.detectPackageType(args.packagePath, packageType)) {
                    std::cerr << "Error: Unable to detect package type for: " << args.packagePath << "\n";
                    return 1;
                }
                
                if (!args.quiet) {
                    std::cout << "Detected package type: " << packageType << "\n";
                }
                
                // Handle different package types
                if (packageType == "PSF") {
                    if (args.useCbs) {
                        success = handler.installPsfPackageWithCbs(args.packagePath, 
                                                                 args.online ? "" : args.imagePath, 
                                                                 args.logPath, args.quiet);
                    } else {
                        if (args.online) {
                            success = handler.installPsfPackageOnline(args.packagePath, args.logPath, args.quiet);
                        } else {
                            success = handler.installPsfPackage(args.packagePath, args.imagePath, args.logPath, args.quiet);
                        }
                    }
                } else if (packageType == "WIM") {
                    if (args.useCbs) {
                        success = handler.installWimPackageWithCbs(args.packagePath, args.imageIndex,
                                                                 args.online ? "" : args.imagePath,
                                                                 args.logPath, args.quiet);
                    } else {
                        success = handler.installWimPackage(args.packagePath, args.imageIndex,
                                                          args.online ? "" : args.imagePath,
                                                          args.logPath, args.quiet);
                    }
                } else if (packageType == "MSU") {
                    if (args.useCbs) {
                        if (args.online) {
                            success = handler.installPackageOnlineWithCbs(args.packagePath, args.logPath, args.quiet);
                        } else {
                            success = handler.installPackageWithCbs(args.packagePath, args.imagePath, args.logPath, args.quiet);
                        }
                    } else {
                        if (args.online) {
                            success = handler.installMsuPackageOnline(args.packagePath, args.logPath, args.quiet);
                        } else {
                            success = handler.installMsuPackage(args.packagePath, args.imagePath, args.logPath, args.quiet);
                        }
                    }
                } else if (packageType == "CAB") {
                    if (args.useCbs) {
                        if (args.online) {
                            success = handler.installPackageOnlineWithCbs(args.packagePath, args.logPath, args.quiet);
                        } else {
                            success = handler.installPackageWithCbs(args.packagePath, args.imagePath, args.logPath, args.quiet);
                        }
                    } else {
                        if (args.online) {
                            success = handler.installCabPackageOnline(args.packagePath, args.logPath, args.quiet);
                        } else {
                            success = handler.installCabPackage(args.packagePath, args.imagePath, args.logPath, args.quiet);
                        }
                    }
                } else {
                    std::cerr << "Error: Unsupported package format: " << packageType << "\n";
                    return 1;
                }
            }
            
            if (success) {
                if (!args.quiet) {
                    std::cout << "Package installation completed successfully.\n";
                    if (!args.noRestart && args.online) {
                        std::cout << "Note: A restart may be required to complete the installation.\n";
                        std::cout << "You can restart now or restart later to apply the changes.\n";
                    }
                }
            } else {
                std::cerr << "Package installation failed.\n";
                return 1;
            }
        }
        else if (command == "add-package-enhanced") {
            AddPackageArgs args = parseAddPackageArgs(argc, argv, 2);
            
            // Validate input parameters
            if (!args.useExtractedDir && args.packagePath.empty()) {
                std::cerr << "Error: /PackagePath parameter is required when not using /ExtractedDir\n";
                return 1;
            }
            
            if (args.useExtractedDir && args.extractedDir.empty()) {
                std::cerr << "Error: /ExtractedDir parameter cannot be empty\n";
                return 1;
            }
            
            if (args.useExtractedDir && !args.packagePath.empty()) {
                std::cerr << "Error: Cannot specify both /PackagePath and /ExtractedDir parameters\n";
                return 1;
            }
            
            // Validate that either /Image or /Online is specified, but not both
            if (args.online && !args.imagePath.empty()) {
                std::cerr << "Error: Cannot specify both /Online and /Image parameters\n";
                return 1;
            }
            
            if (!args.online && args.imagePath.empty()) {
                std::cerr << "Error: Either /Image or /Online parameter is required for add-package-enhanced command\n";
                return 1;
            }
            
            // Enable enhanced processing automatically
            handler.enableCbsIntegration(args.useCbs);
            
            if (!args.quiet) {
                std::cout << "?? Enhanced DISM-like Package Installation\n";
                std::cout << "=========================================\n";
                std::cout << "This enhanced mode provides:\n";
                std::cout << "- Universal package format support (CAB/MSU/PSF/WIM)\n";
                std::cout << "- Automatic component detection and processing\n";
                std::cout << "- PSF/WIM integration within updates\n";
                if (args.useCbs) {
                    std::cout << "- Component-Based Servicing integration\n";
                }
                std::cout << "\n";
            }
            
            // Check for admin privileges when using /Online
            if (args.online) {
                if (!handler.checkAdminPrivileges()) {
                    std::cerr << "Error: Administrator privileges are required for /Online operations\n";
                    std::cerr << "Please run this application as an administrator\n";
                    return 1;
                }
                
                if (!args.quiet) {
                    if (args.useExtractedDir) {
                        std::cout << "Enhanced installation from extracted directory " << args.extractedDir << " to the running operating system...\n";
                    } else {
                        std::cout << "Enhanced installation of package " << args.packagePath << " to the running operating system...\n";
                    }
                }
            } else {
                if (!args.quiet) {
                    if (args.useExtractedDir) {
                        std::cout << "Enhanced installation from extracted directory " << args.extractedDir << " to image " << args.imagePath << "...\n";
                    } else {
                        std::cout << "Enhanced installation of package " << args.packagePath << " to image " << args.imagePath << "...\n";
                    }
                }
            }
            
            bool success = false;
            
            if (args.useExtractedDir) {
                // Enhanced extracted directory processing
                if (!args.quiet) {
                    std::cout << "Scanning extracted directory for all package types...\n";
                }
                
                // Process any package type found in the directory
                auto files = handler.getFilesInDirectory(args.extractedDir, true);
                for (const auto& file : files) {
                    std::string detectedType;
                    if (handler.detectPackageType(file, detectedType)) {
                        if (!args.quiet) {
                            std::cout << "Found " << detectedType << " component: " << fs::path(file).filename().string() << "\n";
                        }
                    }
                }
                
                if (args.useCbs) {
                    success = handler.installExtractedPackageWithCbs(args.extractedDir, 
                                                                   args.online ? "" : args.imagePath, 
                                                                   args.logPath, args.quiet);
                } else {
                    if (args.online) {
                        success = handler.installFromExtractedMsuOnline(args.extractedDir, args.logPath, args.quiet);
                    } else {
                        success = handler.installFromExtractedMsu(args.extractedDir, args.imagePath, args.logPath, args.quiet);
                    }
                }
            } else {
                // Enhanced package file processing
                std::string packageType;
                if (!handler.detectPackageType(args.packagePath, packageType)) {
                    std::cerr << "Error: Unable to detect package type for: " << args.packagePath << "\n";
                    return 1;
                }
                
                if (!args.quiet) {
                    std::cout << "Package type detected: " << packageType << "\n";
                    std::cout << "Initializing enhanced " << packageType << " processing...\n";
                }
                
                // Use enhanced processing for all package types
                if (packageType == "PSF") {
                    if (args.useCbs) {
                        success = handler.installPsfPackageWithCbs(args.packagePath, 
                                                                 args.online ? "" : args.imagePath, 
                                                                 args.logPath, args.quiet);
                    } else {
                        if (args.online) {
                            success = handler.installPsfPackageOnline(args.packagePath, args.logPath, args.quiet);
                        } else {
                            success = handler.installPsfPackage(args.packagePath, args.imagePath, args.logPath, args.quiet);
                        }
                    }
                } else if (packageType == "WIM") {
                    if (args.useCbs) {
                        success = handler.installWimPackageWithCbs(args.packagePath, args.imageIndex,
                                                                 args.online ? "" : args.imagePath,
                                                                 args.logPath, args.quiet);
                    } else {
                        success = handler.installWimPackage(args.packagePath, args.imageIndex,
                                                          args.online ? "" : args.imagePath,
                                                          args.logPath, args.quiet);
                    }
                } else if (packageType == "MSU") {
                    // Enhanced MSU processing with PSF/WIM detection
                    if (args.useCbs) {
                        if (args.online) {
                            success = handler.installPackageOnlineWithCbs(args.packagePath, args.logPath, args.quiet);
                        } else {
                            success = handler.installPackageWithCbs(args.packagePath, args.imagePath, args.logPath, args.quiet);
                        }
                    } else {
                        if (args.online) {
                            success = handler.installMsuPackageOnline(args.packagePath, args.logPath, args.quiet);
                        } else {
                            success = handler.installMsuPackage(args.packagePath, args.imagePath, args.logPath, args.quiet);
                        }
                    }
                } else if (packageType == "CAB") {
                    // Enhanced CAB processing with PSF/WIM detection
                    if (args.useCbs) {
                        if (args.online) {
                            success = handler.installPackageOnlineWithCbs(args.packagePath, args.logPath, args.quiet);
                        } else {
                            success = handler.installPackageWithCbs(args.packagePath, args.imagePath, args.logPath, args.quiet);
                        }
                    } else {
                        if (args.online) {
                            success = handler.installCabPackageOnline(args.packagePath, args.logPath, args.quiet);
                        } else {
                            success = handler.installCabPackage(args.packagePath, args.imagePath, args.logPath, args.quiet);
                        }
                    }
                } else {
                    std::cerr << "Error: Unsupported package format: " << packageType << "\n";
                    return 1;
                }
            }
            
            if (success) {
                if (!args.quiet) {
                    std::cout << "\n? Enhanced package installation completed successfully!\n";
                    std::cout << "?? Features used:\n";
                    std::cout << "   - Universal package format support\n";
                    std::cout << "   - Automatic PSF/WIM component detection\n";
                    std::cout << "   - Enhanced extraction methods\n";
                    if (args.useCbs) {
                        std::cout << "   - Component-Based Servicing integration\n";
                    }
                    if (!args.noRestart && args.online) {
                        std::cout << "\nNote: A restart may be required to complete the installation.\n";
                        std::cout << "You can restart now or restart later to apply the changes.\n";
                    }
                }
            } else {
                std::cerr << "Enhanced package installation failed.\n";
                return 1;
            }
        }
        else {
            // Handle existing commands
            std::string cabFile = argv[2];
            
            if (command == "extract") {
                if (argc < 4) {
                    std::cerr << "Error: Destination path required for extract command\n";
                    return 1;
                }
                std::string destination = argv[3];
                std::cout << "Extracting " << cabFile << " to " << destination << "...\n";
                
                if (handler.extractCab(cabFile, destination)) {
                    std::cout << "Extraction completed successfully.\n";
                } else {
                    std::cerr << "Extraction failed.\n";
                    return 1;
                }
            }
            else if (command == "add") {
                if (argc < 4) {
                    std::cerr << "Error: Source path required for add command\n";
                    return 1;
                }
                std::string source = argv[3];
                std::cout << "Adding files from " << source << " to " << cabFile << "...\n";
                
                if (handler.addToCab(cabFile, source)) {
                    std::cout << "Files added successfully.\n";
                } else {
                    std::cerr << "Add operation failed.\n";
                    return 1;
                }
            }
            else if (command == "list") {
                std::cout << "Listing contents of " << cabFile << ":\n";
                auto files = handler.listCabContents(cabFile);
                
                if (files.empty()) {
                    std::cout << "No files found or error reading CAB file.\n";
                    return 1;
                }
                
                for (const auto& file : files) {
                    std::cout << "  " << file.filename << " (" << file.size << " bytes, " 
                             << file.compressedSize << " compressed)\n";
                }
            }
            else if (command == "create") {
                if (argc < 4) {
                    std::cerr << "Error: Source path required for create command\n";
                    return 1;
                }
                std::string source = argv[3];
                std::cout << "Creating CAB file " << cabFile << " from " << source << "...\n";
                
                if (handler.createCab(cabFile, source)) {
                    std::cout << "CAB file created successfully.\n";
                } else {
                    std::cerr << "CAB creation failed.\n";
                    return 1;
                }
            }
            else if (command == "verify") {
                std::cout << "Verifying " << cabFile << "...\n";
                
                if (handler.verifyCab(cabFile)) {
                    std::cout << "CAB file is valid.\n";
                } else {
                    std::cerr << "CAB file verification failed.\n";
                    return 1;
                }
            }
            else {
                std::cerr << "Error: Unknown command '" << command << "'\n";
                printUsage();
                return 1;
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}