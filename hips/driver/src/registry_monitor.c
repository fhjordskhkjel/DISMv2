/*
 * HIPS Driver Registry Monitoring
 * 
 * Provides kernel-level registry monitoring using registry callbacks
 * to detect unauthorized modifications to critical registry keys.
 */

#include <ntddk.h>
#include "hips_driver.h"

// External references
extern PHIPS_DRIVER_CONTEXT g_DriverContext;

// Registry callback function
EX_CALLBACK_FUNCTION HipsRegistryCallback;

/**
 * Register registry monitoring callbacks
 */
NTSTATUS HipsRegisterRegistryCallbacks(void)
{
    NTSTATUS status;
    UNICODE_STRING altitude;

    if (!g_DriverContext) {
        return STATUS_INVALID_PARAMETER;
    }

    // Initialize altitude string for callback registration
    RtlInitUnicodeString(&altitude, L"320000");

    // Register registry callback
    status = CmRegisterCallbackEx(
        HipsRegistryCallback,
        &altitude,
        NULL, // No driver object context
        &g_DriverContext->RegistryCookie,
        NULL,
        NULL
    );

    if (!NT_SUCCESS(status)) {
        HipsDbgPrint("Failed to register registry callback: 0x%08X\n", status);
        return status;
    }

    HipsDbgPrint("Registry monitoring callbacks registered successfully\n");
    return STATUS_SUCCESS;
}

/**
 * Unregister registry monitoring callbacks
 */
VOID HipsUnregisterRegistryCallbacks(void)
{
    if (g_DriverContext && g_DriverContext->RegistryCookie.QuadPart != 0) {
        CmUnRegisterCallback(g_DriverContext->RegistryCookie);
        g_DriverContext->RegistryCookie.QuadPart = 0;
        HipsDbgPrint("Registry monitoring callbacks unregistered\n");
    }
}

/**
 * Registry callback function
 */
NTSTATUS HipsRegistryCallback(
    _In_ PVOID CallbackContext,
    _In_ PVOID Argument1,
    _In_ PVOID Argument2
)
{
    NTSTATUS status = STATUS_SUCCESS;
    REG_NOTIFY_CLASS notifyClass;
    HIPS_EVENT_TYPE eventType;
    HIPS_THREAT_LEVEL threatLevel = HIPS_THREAT_LOW;
    PUNICODE_STRING keyPath = NULL;
    PUNICODE_STRING valueName = NULL;
    ULONG processId = 0;
    
    UNREFERENCED_PARAMETER(CallbackContext);

    // Check if monitoring is enabled
    if (!g_DriverContext || !g_DriverContext->MonitoringEnabled) {
        return STATUS_SUCCESS;
    }

    notifyClass = (REG_NOTIFY_CLASS)(ULONG_PTR)Argument1;

    try {
        switch (notifyClass) {
            case RegNtPreCreateKeyEx:
            case RegNtPreOpenKeyEx:
            {
                PREG_CREATE_KEY_INFORMATION createInfo = (PREG_CREATE_KEY_INFORMATION)Argument2;
                if (createInfo && createInfo->CompleteName) {
                    keyPath = createInfo->CompleteName;
                    eventType = HIPS_EVENT_REGISTRY_ACCESS;
                }
                break;
            }

            case RegNtPreSetValueKey:
            {
                PREG_SET_VALUE_KEY_INFORMATION setInfo = (PREG_SET_VALUE_KEY_INFORMATION)Argument2;
                if (setInfo) {
                    // Get key path from the key object
                    status = CmCallbackGetKeyObjectIDEx(
                        &g_DriverContext->RegistryCookie,
                        setInfo->Object,
                        NULL,
                        &keyPath,
                        0
                    );
                    
                    if (NT_SUCCESS(status) && keyPath) {
                        valueName = setInfo->ValueName;
                        eventType = HIPS_EVENT_REGISTRY_MODIFIED;
                        
                        // Analyze threat level based on key and value
                        threatLevel = HipsAnalyzeRegistryThreat(
                            keyPath->Buffer,
                            valueName ? valueName->Buffer : NULL,
                            HandleToUlong(PsGetCurrentProcessId())
                        );
                    }
                }
                break;
            }

            case RegNtPreDeleteKey:
            {
                PREG_DELETE_KEY_INFORMATION deleteInfo = (PREG_DELETE_KEY_INFORMATION)Argument2;
                if (deleteInfo) {
                    // Get key path from the key object
                    status = CmCallbackGetKeyObjectIDEx(
                        &g_DriverContext->RegistryCookie,
                        deleteInfo->Object,
                        NULL,
                        &keyPath,
                        0
                    );
                    
                    if (NT_SUCCESS(status) && keyPath) {
                        eventType = HIPS_EVENT_REGISTRY_MODIFIED;
                        threatLevel = HIPS_THREAT_HIGH; // Deleting keys is more suspicious
                    }
                }
                break;
            }

            case RegNtPreDeleteValueKey:
            {
                PREG_DELETE_VALUE_KEY_INFORMATION deleteInfo = (PREG_DELETE_VALUE_KEY_INFORMATION)Argument2;
                if (deleteInfo) {
                    // Get key path from the key object
                    status = CmCallbackGetKeyObjectIDEx(
                        &g_DriverContext->RegistryCookie,
                        deleteInfo->Object,
                        NULL,
                        &keyPath,
                        0
                    );
                    
                    if (NT_SUCCESS(status) && keyPath) {
                        valueName = deleteInfo->ValueName;
                        eventType = HIPS_EVENT_REGISTRY_MODIFIED;
                        threatLevel = HIPS_THREAT_MEDIUM;
                    }
                }
                break;
            }

            default:
                return STATUS_SUCCESS;
        }

        // Get current process ID
        processId = HandleToUlong(PsGetCurrentProcessId());

        // Check if this is a system process (lower threat level)
        if (HipsIsSystemProcess(processId)) {
            threatLevel = HIPS_THREAT_NONE;
        }

        // Log the registry event if we have valid data
        if (keyPath && keyPath->Buffer) {
            HipsDbgPrint("Registry operation: PID=%lu, Key=%wZ, Value=%wZ, Type=%d, Threat=%d\n",
                         processId,
                         keyPath,
                         valueName ? valueName : &(UNICODE_STRING){0},
                         (int)eventType,
                         (int)threatLevel);

            // Check if we should block this operation
            if (threatLevel >= HIPS_THREAT_HIGH) {
                PHIPS_RULE rule = HipsFindRule(eventType, keyPath->Buffer);
                if (rule && rule->Action == HIPS_ACTION_DENY) {
                    HipsDbgPrint("Blocked registry operation: %wZ (Threat Level: %d)\n",
                                 keyPath, (int)threatLevel);
                    
                    // Free the key path if we allocated it
                    if (notifyClass == RegNtPreSetValueKey || 
                        notifyClass == RegNtPreDeleteKey || 
                        notifyClass == RegNtPreDeleteValueKey) {
                        CmCallbackReleaseKeyObjectIDEx(keyPath);
                    }
                    
                    return STATUS_ACCESS_DENIED;
                }
            }

            // Add event to queue
            NTSTATUS addStatus = HipsAddEvent(
                eventType,
                threatLevel,
                NULL, // No file path for registry events
                NULL, // No process path for now
                processId,
                HandleToUlong(PsGetCurrentThreadId()),
                keyPath->Buffer,
                min(keyPath->Length, 512)
            );

            if (!NT_SUCCESS(addStatus)) {
                HipsDbgPrint("Failed to add registry event: 0x%08X\n", addStatus);
            }

            // Free the key path if we allocated it
            if (notifyClass == RegNtPreSetValueKey || 
                notifyClass == RegNtPreDeleteKey || 
                notifyClass == RegNtPreDeleteValueKey) {
                CmCallbackReleaseKeyObjectIDEx(keyPath);
            }
        }

    } except (EXCEPTION_EXECUTE_HANDLER) {
        HipsDbgPrint("Exception in registry callback: 0x%08X\n", GetExceptionCode());
        status = STATUS_SUCCESS; // Don't block operations on exceptions
    }

    return status;
}

/**
 * Analyze registry threat level based on key path and operation
 */
HIPS_THREAT_LEVEL HipsAnalyzeRegistryThreat(
    _In_ PCWSTR KeyPath,
    _In_opt_ PCWSTR ValueName,
    _In_ ULONG ProcessId
)
{
    HIPS_THREAT_LEVEL threatLevel = HIPS_THREAT_LOW;

    if (!KeyPath) {
        return HIPS_THREAT_NONE;
    }

    // System processes get lower threat scores
    if (HipsIsSystemProcess(ProcessId)) {
        return HIPS_THREAT_NONE;
    }

    // Check for critical registry keys
    if (HipsIsCriticalRegistryKey(KeyPath)) {
        threatLevel = max(threatLevel, HIPS_THREAT_MEDIUM);
    }

    // Check for auto-start registry locations
    if (HipsIsAutoStartRegistryKey(KeyPath)) {
        threatLevel = max(threatLevel, HIPS_THREAT_HIGH);
    }

    // Check for specific high-risk operations
    if (wcsstr(KeyPath, L"\\CurrentControlSet\\Services\\")) {
        // Service modifications are high risk
        threatLevel = max(threatLevel, HIPS_THREAT_HIGH);
    }

    if (wcsstr(KeyPath, L"\\CurrentVersion\\Run")) {
        // Auto-start modifications are high risk
        threatLevel = max(threatLevel, HIPS_THREAT_HIGH);
    }

    if (wcsstr(KeyPath, L"\\Winlogon\\")) {
        // Winlogon modifications are critical
        threatLevel = max(threatLevel, HIPS_THREAT_CRITICAL);
    }

    if (wcsstr(KeyPath, L"\\Image File Execution Options\\")) {
        // IFEO modifications for persistence are critical
        threatLevel = max(threatLevel, HIPS_THREAT_CRITICAL);
    }

    // Check for specific suspicious value names
    if (ValueName) {
        if (_wcsicmp(ValueName, L"Debugger") == 0 ||
            _wcsicmp(ValueName, L"GlobalFlag") == 0) {
            // IFEO debugger hijacking
            threatLevel = max(threatLevel, HIPS_THREAT_CRITICAL);
        }

        if (_wcsicmp(ValueName, L"Shell") == 0 ||
            _wcsicmp(ValueName, L"Userinit") == 0 ||
            _wcsicmp(ValueName, L"TaskMan") == 0) {
            // Shell replacement attacks
            threatLevel = max(threatLevel, HIPS_THREAT_CRITICAL);
        }
    }

    return threatLevel;
}