#pragma once
#include <string>
#include <vector>
#include <windows.h>

// Simple helper for loading/unloading offline registry hives and editing values.
// Mount points used:
//   HKLM\\DISMv2_OFFLINE_SOFTWARE -> <image>\\Windows\\System32\\config\\SOFTWARE
//   HKLM\\DISMv2_OFFLINE_SYSTEM   -> <image>\\Windows\\System32\\config\\SYSTEM
//   HKU \\DISMv2_OFFLINE_DEFAULT  -> <image>\\Windows\\System32\\config\\DEFAULT
class OfflineRegistry {
public:
    struct LoadResult {
        bool softwareLoaded = false;
        bool systemLoaded = false;
        bool defaultLoaded = false;
        std::string error;
    };

    static bool enableBackupRestorePrivileges(std::string& err);

    static LoadResult loadBasicHives(const std::string& imageRoot,
                                     bool software=true,
                                     bool system=true,
                                     bool userDefault=true);

    static bool unloadBasicHives(std::string& err);

    // Set a value under a mounted offline hive
    // hiveRoot: "SOFTWARE" | "SYSTEM" | "DEFAULT"
    // typeStr:  "REG_SZ" | "REG_DWORD" | "REG_QWORD"
    static bool setValue(const std::string& hiveRoot,
                         const std::string& subKey,
                         const std::string& valueName,
                         const std::string& typeStr,
                         const std::string& value,
                         std::string& err);

    // Helpers to open mounted hives
    static HKEY getMountedRoot(const std::string& hiveRoot);

    // Convenience helpers
    static bool getActiveControlSetSubkey(std::wstring& controlSetSubkey, std::string& err);
    static bool setTimezoneLoaded(const std::string& timeZoneId, std::string& err);
    static bool setDefaultUserLocaleLoaded(const std::string& localeName, std::string& err);
    static bool setTimezone(const std::string& imageRoot, const std::string& timeZoneId, std::string& err);
    static bool setDefaultUserLocale(const std::string& imageRoot, const std::string& localeName, std::string& err);

    static constexpr const wchar_t* MOUNT_SOFTWARE = L"DISMv2_OFFLINE_SOFTWARE";
    static constexpr const wchar_t* MOUNT_SYSTEM   = L"DISMv2_OFFLINE_SYSTEM";
    static constexpr const wchar_t* MOUNT_DEFAULT  = L"DISMv2_OFFLINE_DEFAULT";
};

// RAII wrapper for an offline registry hive.
// Ensures RegLoadKey/RegUnLoadKey symmetry and simplifies value writes.
class RegistryHive {
public:
    // Construct without loading. Call load() or use the convenience constructor.
    RegistryHive() = default;

    // Convenience constructor that loads immediately.
    RegistryHive(HKEY rootKey, const std::wstring& mountName, const std::wstring& hiveFilePath)
        : _root(rootKey), _mount(mountName), _hivePath(hiveFilePath) {
        std::string err; load(err); // ignore err here; inspect loaded() to check
    }

    // Non-copyable
    RegistryHive(const RegistryHive&) = delete;
    RegistryHive& operator=(const RegistryHive&) = delete;

    // Movable
    RegistryHive(RegistryHive&& other) noexcept {
        *this = std::move(other);
    }
    RegistryHive& operator=(RegistryHive&& other) noexcept {
        if (this != &other) {
            unloadNoThrow();
            _root = other._root; other._root = nullptr;
            _mount = std::move(other._mount);
            _hivePath = std::move(other._hivePath);
            _loaded = other._loaded; other._loaded = false;
        }
        return *this;
    }

    ~RegistryHive() { unloadNoThrow(); }

    // Load the hive file into the registry under mount name.
    bool load(std::string& err);

    // Unload the hive if loaded.
    bool unload(std::string& err);

    bool loaded() const noexcept { return _loaded; }

    // Open or create a subkey under the mounted hive (relative path).
    LONG createKey(const std::wstring& relativeSubKey, REGSAM samDesired, HKEY& hKeyOut) const;

    // Convenience setters under the mounted hive
    bool setString(const std::wstring& relativeSubKey, const std::wstring& valueName, const std::wstring& data, std::string& err) const;
    bool setDword(const std::wstring& relativeSubKey, const std::wstring& valueName, DWORD data, std::string& err) const;

    // Root key where the hive is mounted (HKLM/HKU)
    HKEY root() const noexcept { return _root; }

    // Mount point name used with the root (e.g., L"DISMv2_OFFLINE_SYSTEM")
    const std::wstring& mountName() const noexcept { return _mount; }

    // Full mounted path like "<root>\\<mount>"
    std::wstring fullMountedPath(const std::wstring& relative = L"") const;

private:
    void unloadNoThrow() noexcept;

    HKEY _root { nullptr };
    std::wstring _mount;
    std::wstring _hivePath;
    bool _loaded { false };
};
