/*
 * Advanced HIPS Kernel Mode Driver
 * 
 * This kernel driver provides low-level system monitoring capabilities
 * that cannot be bypassed by user-mode malware or rootkits.
 * 
 * Features:
 * - File system minifilter for real-time file monitoring
 * - Process creation/termination callbacks
 * - Registry operation monitoring
 * - Memory protection and injection detection
 * - Network traffic monitoring integration
 */

#include <ntddk.h>
#include <wdf.h>
#include <fltKernel.h>
#include "hips_driver.h"

// Driver tags and constants
#define HIPS_DRIVER_TAG 'spiH'
#define HIPS_DEVICE_NAME L"\\Device\\HipsDriver"
#define HIPS_SYMBOLIC_LINK L"\\DosDevices\\HipsDriver"

// Global variables
PDEVICE_OBJECT g_DeviceObject = NULL;
PFLT_FILTER g_FilterHandle = NULL;
PHIPS_DRIVER_CONTEXT g_DriverContext = NULL;

// Function prototypes
DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD HipsDriverUnload;
NTSTATUS HipsCreateDevice(PDRIVER_OBJECT DriverObject);
NTSTATUS HipsDeleteDevice(void);

// Device I/O dispatch routines
_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CLOSE)
DRIVER_DISPATCH HipsDispatchCreateClose;

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH HipsDispatchDeviceControl;

// Filter callbacks
FLT_PREOP_CALLBACK_STATUS HipsPreCreateCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS HipsPostCreateCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

// Forward declarations for additional callbacks
FLT_PREOP_CALLBACK_STATUS HipsPreWriteCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS HipsPostWriteCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS HipsPreSetInfoCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);

FLT_POSTOP_CALLBACK_STATUS HipsPostSetInfoCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

NTSTATUS HipsFilterUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags);

// Filter registration structure
CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    {
        IRP_MJ_CREATE,
        0,
        HipsPreCreateCallback,
        HipsPostCreateCallback
    },
    {
        IRP_MJ_WRITE,
        0,
        HipsPreWriteCallback,
        HipsPostWriteCallback
    },
    {
        IRP_MJ_SET_INFORMATION,
        0,
        HipsPreSetInfoCallback,
        HipsPostSetInfoCallback
    },
    { IRP_MJ_OPERATION_END }
};

CONST FLT_REGISTRATION FilterRegistration = {
    sizeof(FLT_REGISTRATION),         // Size
    FLT_REGISTRATION_VERSION,         // Version
    0,                                // Flags
    NULL,                             // Context
    Callbacks,                        // Operation callbacks
    HipsFilterUnload,                 // FilterUnloadCallback
    NULL,                             // InstanceSetupCallback
    NULL,                             // InstanceQueryTeardownCallback
    NULL,                             // InstanceTeardownStartCallback
    NULL,                             // InstanceTeardownCompleteCallback
    NULL,                             // GenerateFileNameCallback
    NULL,                             // NormalizeNameComponentCallback
    NULL                              // NormalizeContextCleanupCallback
};

/**
 * Driver entry point
 */
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint("[HIPS] Driver loading...\n");

    // Set driver unload routine
    DriverObject->DriverUnload = HipsDriverUnload;

    // Set dispatch routines
    DriverObject->MajorFunction[IRP_MJ_CREATE] = HipsDispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = HipsDispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HipsDispatchDeviceControl;

    // Create device object
    status = HipsCreateDevice(DriverObject);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[HIPS] Failed to create device: 0x%08X\n", status);
        return status;
    }

    // Initialize driver context
    g_DriverContext = (PHIPS_DRIVER_CONTEXT)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(HIPS_DRIVER_CONTEXT),
        HIPS_DRIVER_TAG
    );

    if (g_DriverContext == NULL) {
        DbgPrint("[HIPS] Failed to allocate driver context\n");
        HipsDeleteDevice();
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(g_DriverContext, sizeof(HIPS_DRIVER_CONTEXT));
    
    // Initialize context
    g_DriverContext->DriverObject = DriverObject;
    g_DriverContext->DeviceObject = g_DeviceObject;
    g_DriverContext->MonitoringEnabled = FALSE;
    KeInitializeSpinLock(&g_DriverContext->Lock);
    InitializeListHead(&g_DriverContext->EventList);
    InitializeListHead(&g_DriverContext->RuleList);
    
    // Initialize configuration with defaults
    RtlZeroMemory(&g_DriverContext->Configuration, sizeof(HIPS_CONFIG));
    g_DriverContext->Configuration.MonitorFileSystem = TRUE;
    g_DriverContext->Configuration.MonitorProcesses = TRUE;
    g_DriverContext->Configuration.MonitorRegistry = TRUE;
    g_DriverContext->Configuration.MonitorNetwork = FALSE; // Will be implemented later
    g_DriverContext->Configuration.MonitorMemory = FALSE;  // Will be implemented later
    g_DriverContext->Configuration.MinimumThreatLevel = HIPS_THREAT_LOW;
    g_DriverContext->Configuration.MaxEventQueueSize = 1000;
    g_DriverContext->Configuration.EventTimeoutMs = 5000;

    // Register filter
    status = FltRegisterFilter(DriverObject, &FilterRegistration, &g_FilterHandle);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[HIPS] Failed to register filter: 0x%08X\n", status);
        ExFreePoolWithTag(g_DriverContext, HIPS_DRIVER_TAG);
        HipsDeleteDevice();
        return status;
    }

    // Start filtering
    status = FltStartFiltering(g_FilterHandle);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[HIPS] Failed to start filtering: 0x%08X\n", status);
        FltUnregisterFilter(g_FilterHandle);
        ExFreePoolWithTag(g_DriverContext, HIPS_DRIVER_TAG);
        HipsDeleteDevice();
        return status;
    }

    // Register process callbacks
    status = HipsRegisterProcessCallbacks();
    if (!NT_SUCCESS(status)) {
        DbgPrint("[HIPS] Failed to register process callbacks: 0x%08X\n", status);
    }

    // Register registry callbacks
    status = HipsRegisterRegistryCallbacks();
    if (!NT_SUCCESS(status)) {
        DbgPrint("[HIPS] Failed to register registry callbacks: 0x%08X\n", status);
    }
    
    // Initialize default security rules
    status = HipsInitializeDefaultRules();
    if (!NT_SUCCESS(status)) {
        DbgPrint("[HIPS] Failed to initialize default rules: 0x%08X\n", status);
    }

    DbgPrint("[HIPS] Driver loaded successfully\n");
    return STATUS_SUCCESS;
}

/**
 * Driver unload routine
 */
VOID HipsDriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);

    DbgPrint("[HIPS] Driver unloading...\n");

    // Unregister callbacks
    HipsUnregisterProcessCallbacks();
    HipsUnregisterRegistryCallbacks();
    
    // Clean up rules
    HipsCleanupRules();

    // Stop filtering and unregister filter
    if (g_FilterHandle != NULL) {
        FltUnregisterFilter(g_FilterHandle);
        g_FilterHandle = NULL;
    }

    // Clean up driver context
    if (g_DriverContext != NULL) {
        ExFreePoolWithTag(g_DriverContext, HIPS_DRIVER_TAG);
        g_DriverContext = NULL;
    }

    // Delete device
    HipsDeleteDevice();

    DbgPrint("[HIPS] Driver unloaded\n");
}

/**
 * Create device object and symbolic link
 */
NTSTATUS HipsCreateDevice(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    NTSTATUS status;
    UNICODE_STRING deviceName, symbolicLink;

    RtlInitUnicodeString(&deviceName, HIPS_DEVICE_NAME);
    RtlInitUnicodeString(&symbolicLink, HIPS_SYMBOLIC_LINK);

    // Create device object
    status = IoCreateDevice(
        DriverObject,
        0,
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &g_DeviceObject
    );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Create symbolic link
    status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(g_DeviceObject);
        g_DeviceObject = NULL;
        return status;
    }

    g_DeviceObject->Flags |= DO_BUFFERED_IO;
    g_DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

/**
 * Delete device object and symbolic link
 */
NTSTATUS HipsDeleteDevice(void)
{
    UNICODE_STRING symbolicLink;

    if (g_DeviceObject != NULL) {
        RtlInitUnicodeString(&symbolicLink, HIPS_SYMBOLIC_LINK);
        IoDeleteSymbolicLink(&symbolicLink);
        IoDeleteDevice(g_DeviceObject);
        g_DeviceObject = NULL;
    }

    return STATUS_SUCCESS;
}

/**
 * Handle create/close IRPs
 */
NTSTATUS HipsDispatchCreateClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

/**
 * Handle device control IRPs
 */
NTSTATUS HipsDispatchDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpStack;
    ULONG inputBufferLength, outputBufferLength;
    PVOID inputBuffer, outputBuffer;
    ULONG bytesReturned = 0;

    UNREFERENCED_PARAMETER(DeviceObject);

    // Validate IRP and stack location
    if (!Irp) {
        return STATUS_INVALID_PARAMETER;
    }

    irpStack = IoGetCurrentIrpStackLocation(Irp);
    if (!irpStack) {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    inputBufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outputBufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
    inputBuffer = Irp->AssociatedIrp.SystemBuffer;
    outputBuffer = Irp->AssociatedIrp.SystemBuffer;

    // Validate driver context
    if (!g_DriverContext) {
        status = STATUS_DEVICE_NOT_READY;
        goto cleanup;
    }

    __try {
        switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {
            case IOCTL_HIPS_GET_VERSION:
                // Get driver version
                if (outputBufferLength >= sizeof(ULONG) && outputBuffer) {
                    *(PULONG)outputBuffer = HIPS_DRIVER_VERSION;
                    bytesReturned = sizeof(ULONG);
                } else {
                    status = STATUS_BUFFER_TOO_SMALL;
                }
                break;

            case IOCTL_HIPS_START_MONITORING:
                // Start monitoring
                g_DriverContext->MonitoringEnabled = TRUE;
                HipsDbgPrint("Monitoring started\n");
                break;

            case IOCTL_HIPS_STOP_MONITORING:
                // Stop monitoring
                g_DriverContext->MonitoringEnabled = FALSE;
                HipsDbgPrint("Monitoring stopped\n");
                break;

            case IOCTL_HIPS_GET_EVENTS:
                // Get events from queue
                if (outputBuffer && outputBufferLength > 0) {
                    status = HipsGetEvents(outputBuffer, outputBufferLength, &bytesReturned);
                } else {
                    status = STATUS_INVALID_PARAMETER;
                }
                break;

            case IOCTL_HIPS_SET_CONFIG:
                // Set configuration
                if (inputBuffer && inputBufferLength >= sizeof(HIPS_CONFIG)) {
                    status = HipsSetConfiguration(inputBuffer, inputBufferLength);
                } else {
                    status = STATUS_INVALID_PARAMETER;
                }
                break;

            default:
                status = STATUS_INVALID_DEVICE_REQUEST;
                break;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
        HipsDbgPrint("Exception in device control: 0x%08X\n", status);
        bytesReturned = 0;
    }

cleanup:
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}

/**
 * Filter unload callback
 */
NTSTATUS HipsFilterUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Flags);
    DbgPrint("[HIPS] Filter unloading\n");
    return STATUS_SUCCESS;
}

/**
 * Pre-create callback for file operations
 */
FLT_PREOP_CALLBACK_STATUS HipsPreCreateCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    // Check if monitoring is enabled
    if (!g_DriverContext || !g_DriverContext->MonitoringEnabled) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

/**
 * Post-create callback for file operations
 */
FLT_POSTOP_CALLBACK_STATUS HipsPostCreateCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(Data);

    return FLT_POSTOP_FINISHED_PROCESSING;
}

/**
 * Pre-write callback for file operations
 */
FLT_PREOP_CALLBACK_STATUS HipsPreWriteCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    if (!g_DriverContext || !g_DriverContext->MonitoringEnabled) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

/**
 * Post-write callback for file operations
 */
FLT_POSTOP_CALLBACK_STATUS HipsPostWriteCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    return FLT_POSTOP_FINISHED_PROCESSING;
}

/**
 * Pre-set-information callback for file operations
 */
FLT_PREOP_CALLBACK_STATUS HipsPreSetInfoCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    if (!g_DriverContext || !g_DriverContext->MonitoringEnabled) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

/**
 * Post-set-information callback for file operations
 */
FLT_POSTOP_CALLBACK_STATUS HipsPostSetInfoCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    return FLT_POSTOP_FINISHED_PROCESSING;
}

/**
 * Set driver configuration
 */
NTSTATUS HipsSetConfiguration(
    _In_ PVOID InputBuffer,
    _In_ ULONG InputBufferLength
)
{
    PHIPS_CONFIG newConfig = (PHIPS_CONFIG)InputBuffer;
    KIRQL oldIrql;

    if (!InputBuffer || InputBufferLength < sizeof(HIPS_CONFIG) || !g_DriverContext) {
        return STATUS_INVALID_PARAMETER;
    }

    __try {
        // Validate configuration values
        if (newConfig->MaxEventQueueSize > 10000 || newConfig->MaxEventQueueSize == 0) {
            return STATUS_INVALID_PARAMETER;
        }

        if (newConfig->EventTimeoutMs > 60000) {
            return STATUS_INVALID_PARAMETER;
        }

        // Safely update configuration
        KeAcquireSpinLock(&g_DriverContext->Lock, &oldIrql);
        
        RtlCopyMemory(&g_DriverContext->Configuration, newConfig, sizeof(HIPS_CONFIG));
        
        KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);
        
        HipsDbgPrint("Configuration updated successfully\n");
        
        return STATUS_SUCCESS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // If we acquired the spinlock, release it
        if (KeGetCurrentIrql() >= DISPATCH_LEVEL) {
            KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);
        }
        
        NTSTATUS status = GetExceptionCode();
        HipsDbgPrint("Exception in HipsSetConfiguration: 0x%08X\n", status);
        return status;
    }
}
}