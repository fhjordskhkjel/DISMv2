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
