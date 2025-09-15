#include "OfflineRegistry.h"
#include <sddl.h>
#include <sstream>
#include <filesystem>

#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;

static std::wstring to_w(const std::string& s) { return std::wstring(s.begin(), s.end()); }

static bool AdjustPriv(const wchar_t* name, bool enable) {
    HANDLE hToken{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) return false;
    LUID luid{}; if (!LookupPrivilegeValueW(nullptr, name, &luid)) { CloseHandle(hToken); return false; }
    TOKEN_PRIVILEGES tp{}; tp.PrivilegeCount=1; tp.Privileges[0].Luid=luid; tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;
    BOOL ok = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr); CloseHandle(hToken);
    return ok && GetLastError() == ERROR_SUCCESS;
}

bool OfflineRegistry::enableBackupRestorePrivileges(std::string& err) {
    if (!AdjustPriv(L"SeBackupPrivilege", true))  { err = "Failed to enable SeBackupPrivilege";  return false; }
    if (!AdjustPriv(L"SeRestorePrivilege", true)) { err = "Failed to enable SeRestorePrivilege"; return false; }
    return true;
}

OfflineRegistry::LoadResult OfflineRegistry::loadBasicHives(const std::string& imageRoot, bool software, bool system, bool userDefault) {
    LoadResult lr{}; std::string perr;
    if (!enableBackupRestorePrivileges(perr)) { lr.error = perr; return lr; }
    fs::path root(imageRoot);
    if (software) {
        fs::path sw = root / "Windows" / "System32" / "config" / "SOFTWARE";
        if (fs::exists(sw)) {
            if (RegLoadKeyW(HKEY_LOCAL_MACHINE, MOUNT_SOFTWARE, to_w(sw.string()).c_str()) == ERROR_SUCCESS) lr.softwareLoaded = true;
        }
    }
    if (system) {
        fs::path sy = root / "Windows" / "System32" / "config" / "SYSTEM";
        if (fs::exists(sy)) {
            if (RegLoadKeyW(HKEY_LOCAL_MACHINE, MOUNT_SYSTEM, to_w(sy.string()).c_str()) == ERROR_SUCCESS) lr.systemLoaded = true;
        }
    }
    if (userDefault) {
        fs::path df = root / "Windows" / "System32" / "config" / "DEFAULT";
        if (fs::exists(df)) {
            if (RegLoadKeyW(HKEY_USERS, MOUNT_DEFAULT, to_w(df.string()).c_str()) == ERROR_SUCCESS) lr.defaultLoaded = true;
        }
    }
    return lr;
}

bool OfflineRegistry::unloadBasicHives(std::string& err) {
    (void)err;
    RegUnLoadKeyW(HKEY_LOCAL_MACHINE, MOUNT_SOFTWARE);
    RegUnLoadKeyW(HKEY_LOCAL_MACHINE, MOUNT_SYSTEM);
    RegUnLoadKeyW(HKEY_USERS, MOUNT_DEFAULT);
    return true;
}

HKEY OfflineRegistry::getMountedRoot(const std::string& hiveRoot) {
    if (_stricmp(hiveRoot.c_str(), "SOFTWARE") == 0) return HKEY_LOCAL_MACHINE;
    if (_stricmp(hiveRoot.c_str(), "SYSTEM") == 0) return HKEY_LOCAL_MACHINE;
    if (_stricmp(hiveRoot.c_str(), "DEFAULT") == 0) return HKEY_USERS;
    return nullptr;
}

bool OfflineRegistry::setValue(const std::string& hiveRoot, const std::string& subKey, const std::string& valueName, const std::string& typeStr, const std::string& value, std::string& err) {
    HKEY base = getMountedRoot(hiveRoot); if (!base) { err = "Invalid hive root"; return false; }
    const wchar_t* mountName = nullptr;
    if (_stricmp(hiveRoot.c_str(), "SOFTWARE") == 0) mountName = MOUNT_SOFTWARE;
    else if (_stricmp(hiveRoot.c_str(), "SYSTEM") == 0) mountName = MOUNT_SYSTEM;
    else if (_stricmp(hiveRoot.c_str(), "DEFAULT") == 0) mountName = MOUNT_DEFAULT;
    if (!mountName) { err = "Unknown mount"; return false; }

    std::wstring fullSub = to_w(subKey);
    std::wstring path = std::wstring(mountName) + L"\\" + fullSub;
    HKEY hKey{}; LONG rc = RegCreateKeyExW(base, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr);
    if (rc != ERROR_SUCCESS) { err = "RegCreateKeyExW failed"; return false; }

    LONG wr = ERROR_INVALID_PARAMETER;
    if (_stricmp(typeStr.c_str(), "REG_SZ") == 0) {
        auto ws = to_w(value); wr = RegSetValueExW(hKey, to_w(valueName).c_str(), 0, REG_SZ, (const BYTE*)ws.c_str(), (DWORD)((ws.size()+1)*sizeof(wchar_t)));
    } else if (_stricmp(typeStr.c_str(), "REG_DWORD") == 0) {
        DWORD dw = (DWORD)std::strtoul(value.c_str(), nullptr, 0); wr = RegSetValueExW(hKey, to_w(valueName).c_str(), 0, REG_DWORD, (const BYTE*)&dw, sizeof(dw));
    } else if (_stricmp(typeStr.c_str(), "REG_QWORD") == 0) {
        ULONGLONG qw = (ULONGLONG)_strtoui64(value.c_str(), nullptr, 0); wr = RegSetValueExW(hKey, to_w(valueName).c_str(), 0, REG_QWORD, (const BYTE*)&qw, sizeof(qw));
    } else { RegCloseKey(hKey); err = "Unsupported type"; return false; }

    RegCloseKey(hKey);
    if (wr != ERROR_SUCCESS) { err = "RegSetValueExW failed"; return false; }
    return true;
}

bool OfflineRegistry::getActiveControlSetSubkey(std::wstring& controlSetSubkey, std::string& err) {
    HKEY hKey{}; LONG rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, (std::wstring(MOUNT_SYSTEM) + L"\\Select").c_str(), 0, KEY_QUERY_VALUE, &hKey);
    if (rc != ERROR_SUCCESS) { err = "Open SYSTEM\\Select failed"; return false; }
    DWORD current = 1; DWORD cb = sizeof(current);
    rc = RegQueryValueExW(hKey, L"Current", nullptr, nullptr, reinterpret_cast<LPBYTE>(&current), &cb);
    RegCloseKey(hKey);
    if (rc != ERROR_SUCCESS) { err = "Query Current failed"; return false; }
    wchar_t buf[64]; _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"ControlSet%03u", (unsigned)current);
    controlSetSubkey.assign(buf);
    return true;
}

bool OfflineRegistry::setTimezoneLoaded(const std::string& timeZoneId, std::string& err) {
    std::wstring cs; if (!getActiveControlSetSubkey(cs, err)) return false;
    std::wstring key = std::wstring(MOUNT_SYSTEM) + L"\\" + cs + L"\\Control\\TimeZoneInformation";
    HKEY hKey{}; LONG rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr);
    if (rc != ERROR_SUCCESS) { err = "Open TimeZoneInformation failed"; return false; }
    auto ws = to_w(timeZoneId);
    rc = RegSetValueExW(hKey, L"TimeZoneKeyName", 0, REG_SZ, (const BYTE*)ws.c_str(), (DWORD)((ws.size()+1)*sizeof(wchar_t)));
    RegCloseKey(hKey);
    return rc == ERROR_SUCCESS;
}

bool OfflineRegistry::setDefaultUserLocaleLoaded(const std::string& localeName, std::string& err) {
    std::wstring key = std::wstring(MOUNT_DEFAULT) + L"\\Control Panel\\International";
    HKEY hKey{}; LONG rc = RegCreateKeyExW(HKEY_USERS, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr);
    if (rc != ERROR_SUCCESS) { err = "Open HKU\\DEFAULT\\...International failed"; return false; }
    auto ws = to_w(localeName);
    rc = RegSetValueExW(hKey, L"LocaleName", 0, REG_SZ, (const BYTE*)ws.c_str(), (DWORD)((ws.size()+1)*sizeof(wchar_t)));
    RegCloseKey(hKey);
    return rc == ERROR_SUCCESS;
}

bool OfflineRegistry::setTimezone(const std::string& imageRoot, const std::string& timeZoneId, std::string& err) {
    auto lr = loadBasicHives(imageRoot, false, true, false); if (!lr.systemLoaded) { err = "SYSTEM hive not loaded"; return false; }
    bool ok = setTimezoneLoaded(timeZoneId, err);
    std::string tmp; unloadBasicHives(tmp);
    return ok;
}

bool OfflineRegistry::setDefaultUserLocale(const std::string& imageRoot, const std::string& localeName, std::string& err) {
    auto lr = loadBasicHives(imageRoot, false, false, true); if (!lr.defaultLoaded) { err = "DEFAULT hive not loaded"; return false; }
    bool ok = setDefaultUserLocaleLoaded(localeName, err);
    std::string tmp; unloadBasicHives(tmp);
    return ok;
}
