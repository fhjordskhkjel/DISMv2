/*
 * HIPS Driver Utilities
 * 
 * Common utility functions used throughout the kernel driver.
 */

#include <ntddk.h>
#include "hips_driver.h"

// External references
extern PHIPS_DRIVER_CONTEXT g_DriverContext;

/**
 * Check if a process is a system process
 */
BOOLEAN HipsIsSystemProcess(
    _In_ ULONG ProcessId
)
{
    // System process (PID 4) and Idle process (PID 0)
    if (ProcessId == 0 || ProcessId == 4) {
        return TRUE;
    }
    
    // Additional system process checks could be added here
    return FALSE;
}

/**
 * Check if a file path is whitelisted
 */
BOOLEAN HipsIsWhitelistedPath(
    _In_ PCWSTR FilePath
)
{
    if (!FilePath) {
        return FALSE;
    }
    
    // System32 and SysWOW64 directories are generally whitelisted
    if (wcsstr(FilePath, L"\\System32\\") || 
        wcsstr(FilePath, L"\\SysWOW64\\")) {
        return TRUE;
    }
    
    // Windows directory
    if (wcsstr(FilePath, L"\\Windows\\")) {
        return TRUE;
    }
    
    // Program Files directories
    if (wcsstr(FilePath, L"\\Program Files\\") ||
        wcsstr(FilePath, L"\\Program Files (x86)\\")) {
        return TRUE;
    }
    
    return FALSE;
}

/**
 * Simple pattern matching function
 */
BOOLEAN HipsMatchesPattern(
    _In_ PCWSTR String,
    _In_ PCWSTR Pattern
)
{
    if (!String || !Pattern) {
        return FALSE;
    }
    
    // Simple substring match for now
    // Could be enhanced with wildcard support
    return wcsstr(String, Pattern) != NULL;
}

/**
 * Allocate and initialize a Unicode string
 */
NTSTATUS HipsAllocateUnicodeString(
    _Out_ PUNICODE_STRING UnicodeString,
    _In_ USHORT MaximumLength
)
{
    if (!UnicodeString) {
        return STATUS_INVALID_PARAMETER;
    }
    
    UnicodeString->Buffer = (PWCH)HipsAllocatePagedMemory(MaximumLength);
    if (!UnicodeString->Buffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    UnicodeString->Length = 0;
    UnicodeString->MaximumLength = MaximumLength;
    RtlZeroMemory(UnicodeString->Buffer, MaximumLength);
    
    return STATUS_SUCCESS;
}

/**
 * Free a Unicode string allocated with HipsAllocateUnicodeString
 */
VOID HipsFreeUnicodeString(
    _In_ PUNICODE_STRING UnicodeString
)
{
    if (UnicodeString && UnicodeString->Buffer) {
        HipsFreeMemory(UnicodeString->Buffer);
        UnicodeString->Buffer = NULL;
        UnicodeString->Length = 0;
        UnicodeString->MaximumLength = 0;
    }
}

/**
 * Copy a Unicode string
 */
NTSTATUS HipsCopyUnicodeString(
    _Out_ PUNICODE_STRING Destination,
    _In_ PCUNICODE_STRING Source
)
{
    if (!Destination || !Source) {
        return STATUS_INVALID_PARAMETER;
    }
    
    if (Source->Length == 0) {
        Destination->Length = 0;
        if (Destination->Buffer && Destination->MaximumLength > 0) {
            Destination->Buffer[0] = L'\0';
        }
        return STATUS_SUCCESS;
    }
    
    if (Destination->MaximumLength < Source->Length + sizeof(WCHAR)) {
        return STATUS_BUFFER_TOO_SMALL;
    }
    
    RtlCopyMemory(Destination->Buffer, Source->Buffer, Source->Length);
    Destination->Length = Source->Length;
    Destination->Buffer[Source->Length / sizeof(WCHAR)] = L'\0';
    
    return STATUS_SUCCESS;
}

/**
 * Check if a file is an executable
 */
BOOLEAN HipsIsExecutableFile(
    _In_ PCWSTR FilePath
)
{
    if (!FilePath) {
        return FALSE;
    }
    
    PCWSTR extension = wcsrchr(FilePath, L'.');
    if (!extension) {
        return FALSE;
    }
    
    // Check common executable extensions
    if (_wcsicmp(extension, L".exe") == 0 ||
        _wcsicmp(extension, L".dll") == 0 ||
        _wcsicmp(extension, L".sys") == 0 ||
        _wcsicmp(extension, L".scr") == 0 ||
        _wcsicmp(extension, L".com") == 0 ||
        _wcsicmp(extension, L".pif") == 0 ||
        _wcsicmp(extension, L".bat") == 0 ||
        _wcsicmp(extension, L".cmd") == 0) {
        return TRUE;
    }
    
    return FALSE;
}

/**
 * Check if a file is a critical system file
 */
BOOLEAN HisIsCriticalSystemFile(
    _In_ PCWSTR FilePath
)
{
    if (!FilePath) {
        return FALSE;
    }
    
    // Check for critical system files
    PCWSTR criticalFiles[] = {
        L"ntoskrnl.exe",
        L"hal.dll",
        L"kernel32.dll",
        L"ntdll.dll",
        L"advapi32.dll",
        L"user32.dll",
        L"gdi32.dll",
        L"ole32.dll",
        L"oleaut32.dll",
        L"rpcrt4.dll",
        L"wininet.dll",
        L"urlmon.dll",
        L"shlwapi.dll",
        L"shell32.dll",
        L"comctl32.dll",
        L"comdlg32.dll",
        L"version.dll",
        NULL
    };
    
    PCWSTR filename = wcsrchr(FilePath, L'\\');
    filename = filename ? filename + 1 : FilePath;
    
    for (int i = 0; criticalFiles[i]; i++) {
        if (_wcsicmp(filename, criticalFiles[i]) == 0) {
            return TRUE;
        }
    }
    
    return FALSE;
}

/**
 * Check if a registry key is critical
 */
BOOLEAN HipsIsCriticalRegistryKey(
    _In_ PCWSTR KeyPath
)
{
    if (!KeyPath) {
        return FALSE;
    }
    
    // Check for critical registry paths
    PCWSTR criticalPaths[] = {
        L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Services",
        L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Control",
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
        L"\\REGISTRY\\MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows",
        L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager",
        NULL
    };
    
    for (int i = 0; criticalPaths[i]; i++) {
        if (wcsstr(KeyPath, criticalPaths[i])) {
            return TRUE;
        }
    }
    
    return FALSE;
}

/**
 * Check if a registry key is an auto-start location
 */
BOOLEAN HipsIsAutoStartRegistryKey(
    _In_ PCWSTR KeyPath
)
{
    if (!KeyPath) {
        return FALSE;
    }
    
    // Check for auto-start registry locations
    if (wcsstr(KeyPath, L"\\Run") ||
        wcsstr(KeyPath, L"\\RunOnce") ||
        wcsstr(KeyPath, L"\\RunServices") ||
        wcsstr(KeyPath, L"\\RunServicesOnce") ||
        wcsstr(KeyPath, L"\\Winlogon") ||
        wcsstr(KeyPath, L"\\Explorer\\Run")) {
        return TRUE;
    }
    
    return FALSE;
}

/**
 * Log an event with formatting
 */
VOID HipsLogEvent(
    _In_ PCSTR Format,
    ...
)
{
    va_list args;
    va_start(args, Format);
    
    // Use vDbgPrintEx for formatted output
    vDbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL, Format, args);
    
    va_end(args);
}