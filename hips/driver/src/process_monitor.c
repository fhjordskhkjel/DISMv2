/*
 * HIPS Driver Process Monitoring
 * 
 * Provides kernel-level process creation and termination monitoring
 * using PsSetCreateProcessNotifyRoutineEx callback.
 */

#include <ntddk.h>
#include <ntimage.h>
#include "hips_driver.h"

// External references
extern PHIPS_DRIVER_CONTEXT g_DriverContext;

// Process notification callback
VOID HipsProcessNotifyCallback(
    _Inout_ PEPROCESS Process,
    _In_ HANDLE ProcessId,
    _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo
);

/**
 * Register process monitoring callbacks
 */
NTSTATUS HipsRegisterProcessCallbacks(void)
{
    NTSTATUS status;

    if (!g_DriverContext) {
        return STATUS_INVALID_PARAMETER;
    }

    // Register process creation/termination callback
    status = PsSetCreateProcessNotifyRoutineEx(HipsProcessNotifyCallback, FALSE);
    if (!NT_SUCCESS(status)) {
        HipsDbgPrint("Failed to register process notify callback: 0x%08X\n", status);
        return status;
    }

    g_DriverContext->ProcessNotifyRoutine = HipsProcessNotifyCallback;
    HipsDbgPrint("Process monitoring callbacks registered successfully\n");

    return STATUS_SUCCESS;
}

/**
 * Unregister process monitoring callbacks
 */
VOID HipsUnregisterProcessCallbacks(void)
{
    if (g_DriverContext && g_DriverContext->ProcessNotifyRoutine) {
        PsSetCreateProcessNotifyRoutineEx(HipsProcessNotifyCallback, TRUE);
        g_DriverContext->ProcessNotifyRoutine = NULL;
        HipsDbgPrint("Process monitoring callbacks unregistered\n");
    }
}

/**
 * Process creation/termination callback
 */
VOID HipsProcessNotifyCallback(
    _Inout_ PEPROCESS Process,
    _In_ HANDLE ProcessId,
    _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo
)
{
    NTSTATUS status;
    HIPS_PROCESS_INFO processInfo;
    HIPS_EVENT_TYPE eventType;
    HIPS_THREAT_LEVEL threatLevel;
    PUNICODE_STRING imageName = NULL;
    PUNICODE_STRING commandLine = NULL;

    UNREFERENCED_PARAMETER(Process);

    // Check if monitoring is enabled
    if (!g_DriverContext || !g_DriverContext->MonitoringEnabled) {
        return;
    }

    // Initialize process info structure
    RtlZeroMemory(&processInfo, sizeof(processInfo));
    processInfo.ProcessId = HandleToUlong(ProcessId);
    KeQuerySystemTime(&processInfo.CreationTime);

    if (CreateInfo != NULL) {
        // Process creation
        eventType = HIPS_EVENT_PROCESS_CREATED;
        
        // Get parent process ID
        processInfo.ParentProcessId = HandleToUlong(CreateInfo->ParentProcessId);
        
        // Get image name
        if (CreateInfo->ImageFileName) {
            imageName = CreateInfo->ImageFileName;
            
            // Copy image path (limited to our buffer size)
            if (imageName->Length > 0 && imageName->Buffer) {
                SIZE_T copyLength = min(imageName->Length, sizeof(processInfo.ImagePath) - sizeof(WCHAR));
                RtlCopyMemory(processInfo.ImagePath, imageName->Buffer, copyLength);
                processInfo.ImagePath[copyLength / sizeof(WCHAR)] = L'\0';
            }
        }
        
        // Get command line
        if (CreateInfo->CommandLine) {
            commandLine = CreateInfo->CommandLine;
            
            // Copy command line (limited to our buffer size)
            if (commandLine->Length > 0 && commandLine->Buffer) {
                SIZE_T copyLength = min(commandLine->Length, sizeof(processInfo.CommandLine) - sizeof(WCHAR));
                RtlCopyMemory(processInfo.CommandLine, commandLine->Buffer, copyLength);
                processInfo.CommandLine[copyLength / sizeof(WCHAR)] = L'\0';
            }
        }
        
        // Check if it's a system process
        processInfo.IsSystem = HipsIsSystemProcess(processInfo.ProcessId);
        
        // Analyze threat level
        threatLevel = HipsAnalyzeProcessThreat(&processInfo);
        processInfo.ThreatLevel = threatLevel;
        
        // Log process creation
        HipsDbgPrint("Process created - PID: %lu, Parent: %lu, Image: %wZ\n",
                     processInfo.ProcessId,
                     processInfo.ParentProcessId,
                     imageName ? imageName : &(UNICODE_STRING){0});
                     
        // Check if we should block this process
        if (threatLevel >= HIPS_THREAT_HIGH) {
            // Find matching rule
            PHIPS_RULE rule = HipsFindRule(HIPS_EVENT_PROCESS_CREATED, processInfo.ImagePath);
            if (rule && rule->Action == HIPS_ACTION_DENY) {
                // Block process creation
                CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
                HipsDbgPrint("Blocked process creation: %wZ (Threat Level: %d)\n",
                             imageName, threatLevel);
            }
        }
        
    } else {
        // Process termination
        eventType = HIPS_EVENT_PROCESS_TERMINATED;
        threatLevel = HIPS_THREAT_NONE;
        
        // Try to get process image name from EPROCESS
        status = SeLocateProcessImageName(Process, &imageName);
        if (NT_SUCCESS(status) && imageName) {
            // Copy image path
            SIZE_T copyLength = min(imageName->Length, sizeof(processInfo.ImagePath) - sizeof(WCHAR));
            RtlCopyMemory(processInfo.ImagePath, imageName->Buffer, copyLength);
            processInfo.ImagePath[copyLength / sizeof(WCHAR)] = L'\0';
            
            // Free the allocated string
            ExFreePool(imageName);
            imageName = NULL;
        }
        
        HipsDbgPrint("Process terminated - PID: %lu\n", processInfo.ProcessId);
    }
    
    // Add event to queue
    status = HipsAddEvent(
        eventType,
        threatLevel,
        processInfo.ImagePath,
        NULL,
        processInfo.ProcessId,
        0,  // No thread ID for process events
        &processInfo,
        sizeof(processInfo)
    );
    
    if (!NT_SUCCESS(status)) {
        HipsDbgPrint("Failed to add process event: 0x%08X\n", status);
    }
}

/**
 * Analyze process threat level based on various indicators
 */
HIPS_THREAT_LEVEL HipsAnalyzeProcessThreat(
    _In_ PHIPS_PROCESS_INFO ProcessInfo
)
{
    HIPS_THREAT_LEVEL threatLevel = HIPS_THREAT_NONE;
    
    if (!ProcessInfo) {
        return HIPS_THREAT_NONE;
    }
    
    // System processes are generally safe
    if (ProcessInfo->IsSystem) {
        return HIPS_THREAT_NONE;
    }
    
    // Check for suspicious characteristics
    PCWSTR imagePath = ProcessInfo->ImagePath;
    PCWSTR commandLine = ProcessInfo->CommandLine;
    
    // Check file path indicators
    if (imagePath && wcslen(imagePath) > 0) {
        // Executable in temp directories
        if (wcsstr(imagePath, L"\\Temp\\") || 
            wcsstr(imagePath, L"\\tmp\\") ||
            wcsstr(imagePath, L"\\AppData\\Local\\Temp\\")) {
            threatLevel = max(threatLevel, HIPS_THREAT_MEDIUM);
        }
        
        // Executable with suspicious names
        if (wcsstr(imagePath, L"svchost.exe") && 
            !wcsstr(imagePath, L"\\System32\\") &&
            !wcsstr(imagePath, L"\\SysWOW64\\")) {
            threatLevel = max(threatLevel, HIPS_THREAT_HIGH);
        }
        
        // Hidden or system file attributes in user directories
        if (wcsstr(imagePath, L"\\Users\\") && 
            (wcsstr(imagePath, L"\\AppData\\") || wcsstr(imagePath, L"\\$Recycle.Bin\\"))) {
            threatLevel = max(threatLevel, HIPS_THREAT_MEDIUM);
        }
        
        // Executables with no file extension or suspicious extensions
        PCWSTR extension = wcsrchr(imagePath, L'.');
        if (!extension || 
            _wcsicmp(extension, L".scr") == 0 ||
            _wcsicmp(extension, L".pif") == 0 ||
            _wcsicmp(extension, L".com") == 0) {
            threatLevel = max(threatLevel, HIPS_THREAT_MEDIUM);
        }
    }
    
    // Check command line indicators
    if (commandLine && wcslen(commandLine) > 0) {
        // PowerShell with encoded commands
        if (wcsstr(commandLine, L"powershell") && 
            (wcsstr(commandLine, L"-EncodedCommand") || 
             wcsstr(commandLine, L"-enc") ||
             wcsstr(commandLine, L"FromBase64String"))) {
            threatLevel = max(threatLevel, HIPS_THREAT_HIGH);
        }
        
        // WMI suspicious usage
        if (wcsstr(commandLine, L"wmic") && 
            (wcsstr(commandLine, L"process call create") ||
             wcsstr(commandLine, L"shadowcopy delete"))) {
            threatLevel = max(threatLevel, HIPS_THREAT_HIGH);
        }
        
        // Registry manipulation
        if (wcsstr(commandLine, L"reg add") && 
            (wcsstr(commandLine, L"CurrentVersion\\Run") ||
             wcsstr(commandLine, L"CurrentVersion\\RunOnce"))) {
            threatLevel = max(threatLevel, HIPS_THREAT_MEDIUM);
        }
        
        // Network reconnaissance
        if (wcsstr(commandLine, L"netstat") || 
            wcsstr(commandLine, L"nslookup") ||
            wcsstr(commandLine, L"ping") ||
            wcsstr(commandLine, L"tracert")) {
            threatLevel = max(threatLevel, HIPS_THREAT_LOW);
        }
    }
    
    // Check parent-child process relationships
    if (ProcessInfo->ParentProcessId != 0) {
        // Common injection targets spawning unexpected children
        if (imagePath) {
            PCWSTR filename = wcsrchr(imagePath, L'\\');
            filename = filename ? filename + 1 : imagePath;
            
            // Check for process hollowing indicators
            if (_wcsicmp(filename, L"rundll32.exe") == 0 ||
                _wcsicmp(filename, L"regsvr32.exe") == 0 ||
                _wcsicmp(filename, L"mshta.exe") == 0) {
                threatLevel = max(threatLevel, HIPS_THREAT_MEDIUM);
            }
        }
    }
    
    return threatLevel;
}

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
    
    // Additional system process detection could be added here
    // such as checking if the process is running from System32
    
    return FALSE;
}