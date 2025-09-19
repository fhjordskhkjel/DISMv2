/*
 * Advanced HIPS Kernel Driver Header
 * 
 * Contains structure definitions, constants, and function prototypes
 * for the HIPS kernel mode driver.
 */

#ifndef HIPS_DRIVER_H
#define HIPS_DRIVER_H

#include <ntddk.h>
#include <fltKernel.h>

// Driver version
#define HIPS_DRIVER_VERSION 0x01000000  // Version 1.0.0.0

// Device control codes
#define HIPS_DEVICE_TYPE 0x8000
#define IOCTL_HIPS_GET_VERSION      CTL_CODE(HIPS_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_HIPS_START_MONITORING CTL_CODE(HIPS_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_HIPS_STOP_MONITORING  CTL_CODE(HIPS_DEVICE_TYPE, 0x802, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_HIPS_GET_EVENTS       CTL_CODE(HIPS_DEVICE_TYPE, 0x803, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_HIPS_SET_CONFIG       CTL_CODE(HIPS_DEVICE_TYPE, 0x804, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_HIPS_ADD_RULE         CTL_CODE(HIPS_DEVICE_TYPE, 0x805, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_HIPS_REMOVE_RULE      CTL_CODE(HIPS_DEVICE_TYPE, 0x806, METHOD_BUFFERED, FILE_WRITE_DATA)

// Event types
typedef enum _HIPS_EVENT_TYPE {
    HIPS_EVENT_FILE_ACCESS = 1,
    HIPS_EVENT_FILE_OPENED,
    HIPS_EVENT_FILE_WRITTEN,
    HIPS_EVENT_FILE_DELETED,
    HIPS_EVENT_PROCESS_CREATED,
    HIPS_EVENT_PROCESS_TERMINATED,
    HIPS_EVENT_REGISTRY_ACCESS,
    HIPS_EVENT_REGISTRY_MODIFIED,
    HIPS_EVENT_MEMORY_INJECTION,
    HIPS_EVENT_NETWORK_CONNECTION
} HIPS_EVENT_TYPE;

// Threat levels
typedef enum _HIPS_THREAT_LEVEL {
    HIPS_THREAT_NONE = 0,
    HIPS_THREAT_LOW,
    HIPS_THREAT_MEDIUM,
    HIPS_THREAT_HIGH,
    HIPS_THREAT_CRITICAL
} HIPS_THREAT_LEVEL;

// Action types
typedef enum _HIPS_ACTION_TYPE {
    HIPS_ACTION_ALLOW = 0,
    HIPS_ACTION_DENY,
    HIPS_ACTION_QUARANTINE,
    HIPS_ACTION_ALERT_ONLY,
    HIPS_ACTION_CUSTOM
} HIPS_ACTION_TYPE;

// Event structure for kernel-user communication
typedef struct _HIPS_EVENT {
    LIST_ENTRY ListEntry;
    HIPS_EVENT_TYPE EventType;
    HIPS_THREAT_LEVEL ThreatLevel;
    LARGE_INTEGER Timestamp;
    ULONG ProcessId;
    ULONG ThreadId;
    ULONG DataLength;
    WCHAR FilePath[260];        // MAX_PATH equivalent
    WCHAR ProcessPath[260];
    CHAR AdditionalData[512];   // Variable length data
} HIPS_EVENT, *PHIPS_EVENT;

// Configuration structure
typedef struct _HIPS_CONFIG {
    BOOLEAN MonitorFileSystem;
    BOOLEAN MonitorProcesses;
    BOOLEAN MonitorRegistry;
    BOOLEAN MonitorNetwork;
    BOOLEAN MonitorMemory;
    HIPS_THREAT_LEVEL MinimumThreatLevel;
    ULONG MaxEventQueueSize;
    ULONG EventTimeoutMs;
} HIPS_CONFIG, *PHIPS_CONFIG;

// Security rule structure
typedef struct _HIPS_RULE {
    LIST_ENTRY ListEntry;
    ULONG RuleId;
    HIPS_EVENT_TYPE EventType;
    HIPS_ACTION_TYPE Action;
    HIPS_THREAT_LEVEL MinThreatLevel;
    BOOLEAN Enabled;
    WCHAR Pattern[260];         // Pattern to match
    WCHAR Description[512];     // Rule description
} HIPS_RULE, *PHIPS_RULE;

// Driver context structure
typedef struct _HIPS_DRIVER_CONTEXT {
    PDRIVER_OBJECT DriverObject;
    PDEVICE_OBJECT DeviceObject;
    KSPIN_LOCK Lock;
    LIST_ENTRY EventList;
    LIST_ENTRY RuleList;
    HIPS_CONFIG Configuration;
    BOOLEAN MonitoringEnabled;
    ULONG EventCount;
    LARGE_INTEGER StartTime;
    
    // Process monitoring
    PVOID ProcessNotifyRoutine;
    
    // Registry monitoring
    LARGE_INTEGER RegistryCookie;
    
    // Memory monitoring
    PVOID ImageNotifyRoutine;
} HIPS_DRIVER_CONTEXT, *PHIPS_DRIVER_CONTEXT;

// Process information structure
typedef struct _HIPS_PROCESS_INFO {
    ULONG ProcessId;
    ULONG ParentProcessId;
    WCHAR ImagePath[260];
    WCHAR CommandLine[1024];
    LARGE_INTEGER CreationTime;
    BOOLEAN IsSystem;
    HIPS_THREAT_LEVEL ThreatLevel;
} HIPS_PROCESS_INFO, *PHIPS_PROCESS_INFO;

// Network connection information
typedef struct _HIPS_NETWORK_INFO {
    ULONG ProcessId;
    ULONG Protocol;             // TCP, UDP, etc.
    ULONG LocalAddress;
    USHORT LocalPort;
    ULONG RemoteAddress;
    USHORT RemotePort;
    HIPS_THREAT_LEVEL ThreatLevel;
} HIPS_NETWORK_INFO, *PHIPS_NETWORK_INFO;

// Function prototypes for callback management
NTSTATUS HipsRegisterProcessCallbacks(void);
VOID HipsUnregisterProcessCallbacks(void);

NTSTATUS HipsRegisterRegistryCallbacks(void);
VOID HipsUnregisterRegistryCallbacks(void);

NTSTATUS HipsRegisterImageCallbacks(void);
VOID HipsUnregisterImageCallbacks(void);

// Rule management functions
NTSTATUS HipsAddRule(
    _In_ PHIPS_RULE Rule
);

NTSTATUS HipsRemoveRule(
    _In_ ULONG RuleId
);

PHIPS_RULE HipsFindRule(
    _In_ HIPS_EVENT_TYPE EventType,
    _In_opt_ PCWSTR Pattern
);

NTSTATUS HipsInitializeDefaultRules(void);
VOID HipsCleanupRules(void);

NTSTATUS HipsGetRuleStatistics(
    _Out_ PULONG RuleCount,
    _Out_ PULONG EnabledRuleCount
);

NTSTATUS HipsSetRuleEnabled(
    _In_ ULONG RuleId,
    _In_ BOOLEAN Enabled
);

NTSTATUS HipsSetRuleAction(
    _In_ ULONG RuleId,
    _In_ HIPS_ACTION_TYPE Action
);

// Event management functions
NTSTATUS HipsAddEvent(
    _In_ HIPS_EVENT_TYPE EventType,
    _In_ HIPS_THREAT_LEVEL ThreatLevel,
    _In_opt_ PCWSTR FilePath,
    _In_opt_ PCWSTR ProcessPath,
    _In_ ULONG ProcessId,
    _In_ ULONG ThreadId,
    _In_opt_ PVOID AdditionalData,
    _In_ ULONG DataLength
);

VOID HipsCleanupEventList(void);

NTSTATUS HipsGetEvents(
    _Out_ PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG BytesReturned
);

// Rule management functions
NTSTATUS HipsAddRule(
    _In_ PHIPS_RULE Rule
);

NTSTATUS HipsRemoveRule(
    _In_ ULONG RuleId
);

PHIPS_RULE HipsFindRule(
    _In_ HIPS_EVENT_TYPE EventType,
    _In_opt_ PCWSTR Pattern
);

// Configuration functions
NTSTATUS HipsSetConfiguration(
    _In_ PVOID InputBuffer,
    _In_ ULONG InputBufferLength
);

NTSTATUS HipsGetConfiguration(
    _Out_ PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG BytesReturned
);

// Threat analysis functions
HIPS_THREAT_LEVEL HipsAnalyzeFileThreat(
    _In_ PCWSTR FilePath,
    _In_ ULONG ProcessId
);

HIPS_THREAT_LEVEL HipsAnalyzeProcessThreat(
    _In_ PHIPS_PROCESS_INFO ProcessInfo
);

HIPS_THREAT_LEVEL HipsAnalyzeRegistryThreat(
    _In_ PCWSTR KeyPath,
    _In_ PCWSTR ValueName,
    _In_ ULONG ProcessId
);

// Utility functions
BOOLEAN HipsIsSystemProcess(
    _In_ ULONG ProcessId
);

BOOLEAN HipsIsWhitelistedPath(
    _In_ PCWSTR FilePath
);

BOOLEAN HipsMatchesPattern(
    _In_ PCWSTR String,
    _In_ PCWSTR Pattern
);

// Logging and debugging
VOID HipsLogEvent(
    _In_ PCSTR Format,
    ...
);

#ifdef DBG
#define HipsDbgPrint(format, ...) DbgPrint("[HIPS] " format, __VA_ARGS__)
#else
#define HipsDbgPrint(format, ...)
#endif

// Memory allocation macros
#define HipsAllocateNonPagedMemory(Size) \
    ExAllocatePoolWithTag(NonPagedPool, (Size), HIPS_DRIVER_TAG)

#define HipsAllocatePagedMemory(Size) \
    ExAllocatePoolWithTag(PagedPool, (Size), HIPS_DRIVER_TAG)

#define HipsFreeMemory(Ptr) \
    if (Ptr) { ExFreePoolWithTag((Ptr), HIPS_DRIVER_TAG); (Ptr) = NULL; }

// String utilities for kernel mode
NTSTATUS HipsAllocateUnicodeString(
    _Out_ PUNICODE_STRING UnicodeString,
    _In_ USHORT MaximumLength
);

VOID HipsFreeUnicodeString(
    _In_ PUNICODE_STRING UnicodeString
);

NTSTATUS HipsCopyUnicodeString(
    _Out_ PUNICODE_STRING Destination,
    _In_ PCUNICODE_STRING Source
);

// File system utilities
NTSTATUS HipsGetFilePathFromFileObject(
    _In_ PFILE_OBJECT FileObject,
    _Out_ PUNICODE_STRING FilePath
);

BOOLEAN HipsIsExecutableFile(
    _In_ PCWSTR FilePath
);

BOOLEAN HisIsCriticalSystemFile(
    _In_ PCWSTR FilePath
);

// Process utilities
NTSTATUS HipsGetProcessImagePath(
    _In_ HANDLE ProcessId,
    _Out_ PUNICODE_STRING ImagePath
);

NTSTATUS HipsGetProcessCommandLine(
    _In_ HANDLE ProcessId,
    _Out_ PUNICODE_STRING CommandLine
);

// Registry utilities
BOOLEAN HipsIsCriticalRegistryKey(
    _In_ PCWSTR KeyPath
);

BOOLEAN HipsIsAutoStartRegistryKey(
    _In_ PCWSTR KeyPath
);

#endif // HIPS_DRIVER_H