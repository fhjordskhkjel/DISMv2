/*
 * HIPS Driver File System Monitoring
 * 
 * Implements file system minifilter callbacks for real-time monitoring
 * of file operations that cannot be bypassed by user-mode malware.
 */

#include <ntddk.h>
#include <fltKernel.h>
#include "hips_driver.h"

// External references
extern PHIPS_DRIVER_CONTEXT g_DriverContext;
extern PFLT_FILTER g_FilterHandle;

// Forward declarations
NTSTATUS HipsGetFilePathFromFileObject(
    _In_ PFILE_OBJECT FileObject,
    _Out_ PUNICODE_STRING FilePath
);

/**
 * Pre-write callback for file operations
 */
FLT_PREOP_CALLBACK_STATUS HipsPreWriteCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
)
{
    NTSTATUS status;
    UNICODE_STRING filePath = {0};
    HIPS_THREAT_LEVEL threatLevel;
    ULONG processId;
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;

    UNREFERENCED_PARAMETER(CompletionContext);

    // Check if monitoring is enabled
    if (!g_DriverContext || !g_DriverContext->MonitoringEnabled) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // Skip if not a regular file
    if (!FltObjects->FileObject || 
        FlagOn(FltObjects->FileObject->Flags, FO_VOLUME_OPEN)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    processId = HandleToUlong(PsGetCurrentProcessId());

    // Get file name information
    status = FltGetFileNameInformation(
        Data,
        FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
        &nameInfo
    );

    if (!NT_SUCCESS(status) || !nameInfo) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // Parse the file name
    status = FltParseFileNameInformation(nameInfo);
    if (!NT_SUCCESS(status)) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // Analyze threat level
    threatLevel = HipsAnalyzeFileThreat(nameInfo->Name.Buffer, processId);

    // Log the file write attempt
    HipsDbgPrint("File write - PID: %lu, File: %wZ, Threat: %d\n",
                 processId, &nameInfo->Name, (int)threatLevel);

    // Check if we should block this operation
    if (threatLevel >= HIPS_THREAT_HIGH) {
        PHIPS_RULE rule = HipsFindRule(HIPS_EVENT_FILE_WRITTEN, nameInfo->Name.Buffer);
        if (rule && rule->Action == HIPS_ACTION_DENY) {
            HipsDbgPrint("Blocked file write: %wZ (Threat Level: %d)\n",
                         &nameInfo->Name, (int)threatLevel);
            
            FltReleaseFileNameInformation(nameInfo);
            Data->IoStatus.Status = STATUS_ACCESS_DENIED;
            Data->IoStatus.Information = 0;
            return FLT_PREOP_COMPLETE;
        }
    }

    // Add event to queue
    status = HipsAddEvent(
        HIPS_EVENT_FILE_WRITTEN,
        threatLevel,
        nameInfo->Name.Buffer,
        NULL, // Process path will be filled later
        processId,
        HandleToUlong(PsGetCurrentThreadId()),
        NULL,
        0
    );

    if (!NT_SUCCESS(status)) {
        HipsDbgPrint("Failed to add file write event: 0x%08X\n", status);
    }

    FltReleaseFileNameInformation(nameInfo);
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

    // Post-processing can be added here if needed
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
    NTSTATUS status;
    FILE_INFORMATION_CLASS fileInfoClass;
    HIPS_THREAT_LEVEL threatLevel;
    ULONG processId;
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    HIPS_EVENT_TYPE eventType = HIPS_EVENT_FILE_ACCESS;

    UNREFERENCED_PARAMETER(CompletionContext);

    // Check if monitoring is enabled
    if (!g_DriverContext || !g_DriverContext->MonitoringEnabled) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // Skip if not a regular file
    if (!FltObjects->FileObject || 
        FlagOn(FltObjects->FileObject->Flags, FO_VOLUME_OPEN)) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    fileInfoClass = Data->Iopb->Parameters.SetFileInformation.FileInformationClass;
    processId = HandleToUlong(PsGetCurrentProcessId());

    // We're interested in file deletion and rename operations
    if (fileInfoClass != FileDispositionInformation &&
        fileInfoClass != FileDispositionInformationEx &&
        fileInfoClass != FileRenameInformation &&
        fileInfoClass != FileRenameInformationEx) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // Get file name information
    status = FltGetFileNameInformation(
        Data,
        FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
        &nameInfo
    );

    if (!NT_SUCCESS(status) || !nameInfo) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // Parse the file name
    status = FltParseFileNameInformation(nameInfo);
    if (!NT_SUCCESS(status)) {
        FltReleaseFileNameInformation(nameInfo);
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    // Determine event type based on operation
    if (fileInfoClass == FileDispositionInformation ||
        fileInfoClass == FileDispositionInformationEx) {
        
        PFILE_DISPOSITION_INFORMATION dispInfo = 
            (PFILE_DISPOSITION_INFORMATION)Data->Iopb->Parameters.SetFileInformation.InfoBuffer;
        
        if (dispInfo && dispInfo->DeleteFile) {
            eventType = HIPS_EVENT_FILE_DELETED;
        }
    } else {
        eventType = HIPS_EVENT_FILE_ACCESS; // Rename operations
    }

    // Analyze threat level
    threatLevel = HipsAnalyzeFileThreat(nameInfo->Name.Buffer, processId);

    // File deletions are generally more suspicious
    if (eventType == HIPS_EVENT_FILE_DELETED) {
        threatLevel = max(threatLevel, HIPS_THREAT_MEDIUM);
        
        // Deleting system files or executables is very suspicious
        if (HisIsCriticalSystemFile(nameInfo->Name.Buffer) ||
            HipsIsExecutableFile(nameInfo->Name.Buffer)) {
            threatLevel = HIPS_THREAT_HIGH;
        }
    }

    // Log the operation
    HipsDbgPrint("File set info - PID: %lu, File: %wZ, Type: %d, Threat: %d\n",
                 processId, &nameInfo->Name, (int)eventType, (int)threatLevel);

    // Check if we should block this operation
    if (threatLevel >= HIPS_THREAT_HIGH) {
        PHIPS_RULE rule = HipsFindRule(eventType, nameInfo->Name.Buffer);
        if (rule && rule->Action == HIPS_ACTION_DENY) {
            HipsDbgPrint("Blocked file operation: %wZ (Threat Level: %d)\n",
                         &nameInfo->Name, (int)threatLevel);
            
            FltReleaseFileNameInformation(nameInfo);
            Data->IoStatus.Status = STATUS_ACCESS_DENIED;
            Data->IoStatus.Information = 0;
            return FLT_PREOP_COMPLETE;
        }
    }

    // Add event to queue
    status = HipsAddEvent(
        eventType,
        threatLevel,
        nameInfo->Name.Buffer,
        NULL, // Process path will be filled later
        processId,
        HandleToUlong(PsGetCurrentThreadId()),
        NULL,
        0
    );

    if (!NT_SUCCESS(status)) {
        HipsDbgPrint("Failed to add file operation event: 0x%08X\n", status);
    }

    FltReleaseFileNameInformation(nameInfo);
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

    // Post-processing can be added here if needed
    return FLT_POSTOP_FINISHED_PROCESSING;
}

/**
 * Analyze file threat level based on path and operation
 */
HIPS_THREAT_LEVEL HipsAnalyzeFileThreat(
    _In_ PCWSTR FilePath,
    _In_ ULONG ProcessId
)
{
    HIPS_THREAT_LEVEL threatLevel = HIPS_THREAT_LOW;

    if (!FilePath) {
        return HIPS_THREAT_NONE;
    }

    // System processes get lower threat scores
    if (HipsIsSystemProcess(ProcessId)) {
        return HIPS_THREAT_NONE;
    }

    // Whitelisted paths get lower scores
    if (HipsIsWhitelistedPath(FilePath)) {
        return HIPS_THREAT_NONE;
    }

    // Check for suspicious file locations
    if (wcsstr(FilePath, L"\\Temp\\") || 
        wcsstr(FilePath, L"\\tmp\\") ||
        wcsstr(FilePath, L"\\AppData\\Local\\Temp\\")) {
        threatLevel = max(threatLevel, HIPS_THREAT_MEDIUM);
    }

    // Files in user profile directories
    if (wcsstr(FilePath, L"\\Users\\") && 
        !wcsstr(FilePath, L"\\AppData\\Roaming\\Microsoft\\")) {
        threatLevel = max(threatLevel, HIPS_THREAT_LOW);
    }

    // Hidden or system directories in user areas
    if (wcsstr(FilePath, L"\\$Recycle.Bin\\") ||
        wcsstr(FilePath, L"\\System Volume Information\\")) {
        threatLevel = max(threatLevel, HIPS_THREAT_MEDIUM);
    }

    // Check file type
    if (HipsIsExecutableFile(FilePath)) {
        threatLevel = max(threatLevel, HIPS_THREAT_MEDIUM);
        
        // Executable files in temp or suspicious locations
        if (wcsstr(FilePath, L"\\Temp\\") || 
            wcsstr(FilePath, L"\\Downloads\\") ||
            wcsstr(FilePath, L"\\Desktop\\")) {
            threatLevel = max(threatLevel, HIPS_THREAT_HIGH);
        }
    }

    // Critical system files
    if (HisIsCriticalSystemFile(FilePath)) {
        threatLevel = max(threatLevel, HIPS_THREAT_HIGH);
    }

    // Check for suspicious file names
    PCWSTR filename = wcsrchr(FilePath, L'\\');
    filename = filename ? filename + 1 : FilePath;

    if (filename) {
        // Files impersonating system processes
        if (_wcsicmp(filename, L"svchost.exe") == 0 ||
            _wcsicmp(filename, L"lsass.exe") == 0 ||
            _wcsicmp(filename, L"winlogon.exe") == 0 ||
            _wcsicmp(filename, L"csrss.exe") == 0) {
            
            // If not in system directories, very suspicious
            if (!wcsstr(FilePath, L"\\System32\\") &&
                !wcsstr(FilePath, L"\\SysWOW64\\")) {
                threatLevel = max(threatLevel, HIPS_THREAT_CRITICAL);
            }
        }

        // Files with double extensions
        if (wcsstr(filename, L".pdf.exe") ||
            wcsstr(filename, L".doc.exe") ||
            wcsstr(filename, L".jpg.exe")) {
            threatLevel = max(threatLevel, HIPS_THREAT_HIGH);
        }
    }

    return threatLevel;
}

/**
 * Get file path from file object (utility function)
 */
NTSTATUS HipsGetFilePathFromFileObject(
    _In_ PFILE_OBJECT FileObject,
    _Out_ PUNICODE_STRING FilePath
)
{
    NTSTATUS status;
    ULONG returnedLength;
    PFILE_NAME_INFORMATION fileNameInfo = NULL;
    ULONG bufferSize = 1024; // Start with 1KB buffer

    if (!FileObject || !FilePath) {
        return STATUS_INVALID_PARAMETER;
    }

    // Allocate buffer for file name
    fileNameInfo = (PFILE_NAME_INFORMATION)HipsAllocatePagedMemory(bufferSize);
    if (!fileNameInfo) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Query the file name
    status = IoQueryFileDosDeviceName(FileObject, &fileNameInfo);
    
    if (NT_SUCCESS(status)) {
        // Allocate Unicode string buffer
        status = HipsAllocateUnicodeString(FilePath, fileNameInfo->FileNameLength + sizeof(WCHAR));
        if (NT_SUCCESS(status)) {
            RtlCopyMemory(
                FilePath->Buffer,
                fileNameInfo->FileName,
                fileNameInfo->FileNameLength
            );
            FilePath->Length = (USHORT)fileNameInfo->FileNameLength;
            FilePath->Buffer[FilePath->Length / sizeof(WCHAR)] = L'\0';
        }
    }

    HipsFreeMemory(fileNameInfo);
    return status;
}