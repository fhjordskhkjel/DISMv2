/*
 * HIPS Driver Event Manager
 * 
 * Manages event queuing and retrieval for communication
 * between kernel driver and user-mode application.
 */

#include <ntddk.h>
#include "hips_driver.h"

// External references
extern PHIPS_DRIVER_CONTEXT g_DriverContext;

/**
 * Add an event to the event queue
 */
NTSTATUS HipsAddEvent(
    _In_ HIPS_EVENT_TYPE EventType,
    _In_ HIPS_THREAT_LEVEL ThreatLevel,
    _In_opt_ PCWSTR FilePath,
    _In_opt_ PCWSTR ProcessPath,
    _In_ ULONG ProcessId,
    _In_ ULONG ThreadId,
    _In_opt_ PVOID AdditionalData,
    _In_ ULONG DataLength
)
{
    PHIPS_EVENT event;
    KIRQL oldIrql;
    SIZE_T dataSize = min(DataLength, sizeof(event->AdditionalData));

    if (!g_DriverContext) {
        return STATUS_INVALID_PARAMETER;
    }

    // Allocate event structure with enhanced safety
    event = (PHIPS_EVENT)HipsAllocateNonPagedMemoryZero(sizeof(HIPS_EVENT));
    if (!event) {
        HipsDbgPrint("Failed to allocate event structure\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Initialize event (memory is already zeroed)
    event->EventType = EventType;
    event->ThreatLevel = ThreatLevel;
    event->ProcessId = ProcessId;
    event->ThreadId = ThreadId;
    KeQuerySystemTime(&event->Timestamp);

    // Copy file path if provided
    if (FilePath) {
        SIZE_T pathLength = wcslen(FilePath);
        SIZE_T copyLength = min(pathLength, RTL_NUMBER_OF(event->FilePath) - 1);
        RtlCopyMemory(event->FilePath, FilePath, copyLength * sizeof(WCHAR));
        event->FilePath[copyLength] = L'\0';
    }

    // Copy process path if provided
    if (ProcessPath) {
        SIZE_T pathLength = wcslen(ProcessPath);
        SIZE_T copyLength = min(pathLength, RTL_NUMBER_OF(event->ProcessPath) - 1);
        RtlCopyMemory(event->ProcessPath, ProcessPath, copyLength * sizeof(WCHAR));
        event->ProcessPath[copyLength] = L'\0';
    }

    // Copy additional data if provided
    if (AdditionalData && dataSize > 0) {
        RtlCopyMemory(event->AdditionalData, AdditionalData, dataSize);
        event->DataLength = (ULONG)dataSize;
    }

    // Add to event list (thread-safe)
    KeAcquireSpinLock(&g_DriverContext->Lock, &oldIrql);
    
    // Check if we've exceeded the maximum queue size
    if (g_DriverContext->EventCount >= g_DriverContext->Configuration.MaxEventQueueSize) {
        // Remove oldest event
        if (!IsListEmpty(&g_DriverContext->EventList)) {
            PLIST_ENTRY oldestEntry = RemoveHeadList(&g_DriverContext->EventList);
            PHIPS_EVENT oldestEvent = CONTAINING_RECORD(oldestEntry, HIPS_EVENT, ListEntry);
            HipsFreeMemory(oldestEvent);
            g_DriverContext->EventCount--;
        }
    }
    
    InsertTailList(&g_DriverContext->EventList, &event->ListEntry);
    g_DriverContext->EventCount++;
    
    KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);

    return STATUS_SUCCESS;
}

/**
 * Get events from the queue for user-mode consumption
 */
NTSTATUS HipsGetEvents(
    _Out_ PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG BytesReturned
)
{
    KIRQL oldIrql;
    ULONG bytesUsed = 0;
    ULONG eventCount = 0;
    PUCHAR buffer = (PUCHAR)OutputBuffer;
    NTSTATUS status = STATUS_SUCCESS;

    if (!g_DriverContext || !OutputBuffer || !BytesReturned) {
        return STATUS_INVALID_PARAMETER;
    }

    *BytesReturned = 0;

    // Validate output buffer size
    if (OutputBufferLength < sizeof(HIPS_EVENT)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    __try {
        KeAcquireSpinLock(&g_DriverContext->Lock, &oldIrql);

        // Copy events from list to output buffer
        while (!IsListEmpty(&g_DriverContext->EventList) && 
               (bytesUsed + sizeof(HIPS_EVENT)) <= OutputBufferLength) {
            
            PLIST_ENTRY listEntry = RemoveHeadList(&g_DriverContext->EventList);
            PHIPS_EVENT event = CONTAINING_RECORD(listEntry, HIPS_EVENT, ListEntry);
            
            // Copy event to output buffer with bounds checking
            if (bytesUsed + sizeof(HIPS_EVENT) <= OutputBufferLength) {
                RtlCopyMemory(buffer + bytesUsed, event, sizeof(HIPS_EVENT));
                bytesUsed += sizeof(HIPS_EVENT);
                eventCount++;
            }
            
            // Free the event
            HipsFreeMemory(event);
            g_DriverContext->EventCount--;
        }

        KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);

        *BytesReturned = bytesUsed;
        
        HipsDbgPrint("Retrieved %lu events (%lu bytes)\n", eventCount, bytesUsed);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // If we acquired the spinlock, release it
        if (KeGetCurrentIrql() >= DISPATCH_LEVEL) {
            KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);
        }
        
        status = GetExceptionCode();
        HipsDbgPrint("Exception in HipsGetEvents: 0x%08X\n", status);
        *BytesReturned = 0;
    }
    
    return status;
}

/**
 * Clean up all events in the queue
 */
VOID HipsCleanupEventList(void)
{
    KIRQL oldIrql;
    PLIST_ENTRY listEntry;
    PHIPS_EVENT event;
    ULONG cleanedCount = 0;

    if (!g_DriverContext) {
        return;
    }

    KeAcquireSpinLock(&g_DriverContext->Lock, &oldIrql);

    while (!IsListEmpty(&g_DriverContext->EventList)) {
        listEntry = RemoveHeadList(&g_DriverContext->EventList);
        event = CONTAINING_RECORD(listEntry, HIPS_EVENT, ListEntry);
        
        // Safely free memory
        if (event) {
            HipsFreeMemory(event);
            cleanedCount++;
        }
    }

    // Reset event count
    g_DriverContext->EventCount = 0;

    KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);

    HipsDbgPrint("Cleaned up %lu events from queue\n", cleanedCount);
}
        PLIST_ENTRY listEntry = RemoveHeadList(&g_DriverContext->EventList);
        PHIPS_EVENT event = CONTAINING_RECORD(listEntry, HIPS_EVENT, ListEntry);
        HipsFreeMemory(event);
        g_DriverContext->EventCount--;
    }

    KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);

    HipsDbgPrint("Event list cleaned up\n");
}

/**
 * Get event queue statistics
 */
NTSTATUS HipsGetEventStatistics(
    _Out_ PULONG EventCount,
    _Out_ PULONG MaxQueueSize
)
{
    KIRQL oldIrql;

    if (!g_DriverContext || !EventCount || !MaxQueueSize) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&g_DriverContext->Lock, &oldIrql);
    *EventCount = g_DriverContext->EventCount;
    *MaxQueueSize = g_DriverContext->Configuration.MaxEventQueueSize;
    KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);

    return STATUS_SUCCESS;
}