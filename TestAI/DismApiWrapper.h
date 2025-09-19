#pragma once
#include <string>
#include <vector>
#include <windows.h>

class DismApiWrapper {
public:
    struct Options {
        bool online = true;
        std::string imagePath; // used when online=false
        bool all = false;      // for feature dependencies
        bool recurse = true;   // for driver add
        bool forceUnsigned = false; // driver install safety
        bool skipRestart = true;
        bool enableNoRestart = true;
        std::string scratchDir; // optional scratch dir (/ScratchDir)
        std::string logPath;    // optional log file (/LogPath)
        int timeoutMs = 60 * 60 * 1000; // 60 min default
        bool readOnly = false; // for mount-image
    };

    DismApiWrapper() = default;

    // Image Management
    bool mountImage(const std::string& wimPath, int index, const std::string& mountDir, const Options& opt, std::string& out, DWORD& exitCode);
    bool unmountImage(const std::string& mountDir, bool commit, const Options& opt, std::string& out, DWORD& exitCode);
    bool getMountedImages(std::string& out, DWORD& exitCode);

    // Package operations
    bool addPackage(const std::string& packagePath, const Options& opt, std::string& out, DWORD& exitCode);
    bool removePackage(const std::string& packageNameOrPath, const Options& opt, std::string& out, DWORD& exitCode);

    // Features
    bool enableFeature(const std::string& featureName, const Options& opt, std::string& out, DWORD& exitCode);
    bool disableFeature(const std::string& featureName, const Options& opt, std::string& out, DWORD& exitCode);

    // Capabilities
    bool addCapability(const std::string& capabilityName, const Options& opt, std::string& out, DWORD& exitCode);
    bool removeCapability(const std::string& capabilityName, const Options& opt, std::string& out, DWORD& exitCode);

    // Drivers
    bool addDriver(const std::string& driverPath, const Options& opt, std::string& out, DWORD& exitCode);
    bool removeDriver(const std::string& publishedName, const Options& opt, std::string& out, DWORD& exitCode);

    // Provisioned Appx/MSIX
    bool addProvisionedAppx(const std::string& appxPath,
                            const std::vector<std::string>& dependencyPaths,
                            const std::string& licensePath,
                            const Options& opt,
                            std::string& out, DWORD& exitCode);
    bool removeProvisionedAppx(const std::string& packageName,
                               const Options& opt,
                               std::string& out, DWORD& exitCode);

    static std::wstring getSystemToolPath(const wchar_t* toolName);

private:
    static bool runProcessCapture(const std::wstring& command, DWORD timeoutMs, std::string& output, DWORD& exitCode);
    static std::wstring toLong(const std::wstring& p);
    static std::wstring quote(const std::wstring& p);
    static std::wstring onOff(const Options& opt);
    static std::wstring imageArg(const Options& opt);
    static std::wstring commonArgs(const Options& opt);
};
