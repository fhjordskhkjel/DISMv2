/*
 * HIPS Driver Rule Engine
 * 
 * Manages security rules and provides rule matching capabilities
 * for the kernel driver to make real-time blocking decisions.
 */

#include <ntddk.h>
#include "hips_driver.h"

// External references
extern PHIPS_DRIVER_CONTEXT g_DriverContext;

// Static rule ID counter
static ULONG g_NextRuleId = 1;

/**
 * Add a security rule to the driver
 */
NTSTATUS HipsAddRule(
    _In_ PHIPS_RULE Rule
)
{
    PHIPS_RULE newRule;
    KIRQL oldIrql;

    if (!g_DriverContext || !Rule) {
        return STATUS_INVALID_PARAMETER;
    }

    // Allocate memory for the new rule
    newRule = (PHIPS_RULE)HipsAllocateNonPagedMemory(sizeof(HIPS_RULE));
    if (!newRule) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Copy the rule data
    RtlCopyMemory(newRule, Rule, sizeof(HIPS_RULE));
    
    // Assign a unique rule ID if not already set
    if (newRule->RuleId == 0) {
        newRule->RuleId = g_NextRuleId++;
    }

    // Add to the rule list (thread-safe)
    KeAcquireSpinLock(&g_DriverContext->Lock, &oldIrql);
    InsertTailList(&g_DriverContext->RuleList, &newRule->ListEntry);
    KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);

    HipsDbgPrint("Added rule: ID=%lu, Type=%d, Action=%d, Pattern=%wZ\n",
                 newRule->RuleId,
                 (int)newRule->EventType,
                 (int)newRule->Action,
                 newRule->Pattern);

    return STATUS_SUCCESS;
}

/**
 * Remove a security rule from the driver
 */
NTSTATUS HipsRemoveRule(
    _In_ ULONG RuleId
)
{
    KIRQL oldIrql;
    PLIST_ENTRY listEntry;
    PHIPS_RULE rule;
    BOOLEAN found = FALSE;

    if (!g_DriverContext) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&g_DriverContext->Lock, &oldIrql);

    // Search for the rule
    listEntry = g_DriverContext->RuleList.Flink;
    while (listEntry != &g_DriverContext->RuleList) {
        rule = CONTAINING_RECORD(listEntry, HIPS_RULE, ListEntry);
        
        if (rule->RuleId == RuleId) {
            // Remove from list
            RemoveEntryList(listEntry);
            found = TRUE;
            break;
        }
        
        listEntry = listEntry->Flink;
    }

    KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);

    if (found) {
        HipsDbgPrint("Removed rule: ID=%lu\n", RuleId);
        HipsFreeMemory(rule);
        return STATUS_SUCCESS;
    }

    return STATUS_NOT_FOUND;
}

/**
 * Find a matching rule for the given event type and pattern
 */
PHIPS_RULE HipsFindRule(
    _In_ HIPS_EVENT_TYPE EventType,
    _In_opt_ PCWSTR Pattern
)
{
    KIRQL oldIrql;
    PLIST_ENTRY listEntry;
    PHIPS_RULE rule;
    PHIPS_RULE matchingRule = NULL;

    if (!g_DriverContext) {
        return NULL;
    }

    KeAcquireSpinLock(&g_DriverContext->Lock, &oldIrql);

    // Search for a matching rule
    listEntry = g_DriverContext->RuleList.Flink;
    while (listEntry != &g_DriverContext->RuleList) {
        rule = CONTAINING_RECORD(listEntry, HIPS_RULE, ListEntry);
        
        // Check if rule is enabled and matches event type
        if (rule->Enabled && rule->EventType == EventType) {
            // If no pattern is provided, or rule has no pattern, it's a match
            if (!Pattern || !rule->Pattern[0]) {
                matchingRule = rule;
                break;
            }
            
            // Check if pattern matches
            if (HipsMatchesPattern(Pattern, rule->Pattern)) {
                matchingRule = rule;
                break;
            }
        }
        
        listEntry = listEntry->Flink;
    }

    KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);

    return matchingRule;
}

/**
 * Initialize default security rules
 */
NTSTATUS HipsInitializeDefaultRules(void)
{
    NTSTATUS status = STATUS_SUCCESS;
    HIPS_RULE rule;

    if (!g_DriverContext) {
        return STATUS_INVALID_PARAMETER;
    }

    // Rule 1: Block executable files in temp directories
    RtlZeroMemory(&rule, sizeof(rule));
    rule.EventType = HIPS_EVENT_FILE_WRITTEN;
    rule.Action = HIPS_ACTION_DENY;
    rule.MinThreatLevel = HIPS_THREAT_HIGH;
    rule.Enabled = TRUE;
    wcscpy_s(rule.Pattern, RTL_NUMBER_OF(rule.Pattern), L"\\Temp\\*.exe");
    wcscpy_s(rule.Description, RTL_NUMBER_OF(rule.Description), 
             L"Block executable creation in temp directories");
    
    status = HipsAddRule(&rule);
    if (!NT_SUCCESS(status)) {
        HipsDbgPrint("Failed to add default rule 1: 0x%08X\n", status);
    }

    // Rule 2: Block critical registry modifications
    RtlZeroMemory(&rule, sizeof(rule));
    rule.EventType = HIPS_EVENT_REGISTRY_MODIFIED;
    rule.Action = HIPS_ACTION_ALERT_ONLY; // Start with alerts only
    rule.MinThreatLevel = HIPS_THREAT_HIGH;
    rule.Enabled = TRUE;
    wcscpy_s(rule.Pattern, RTL_NUMBER_OF(rule.Pattern), L"*\\CurrentVersion\\Run*");
    wcscpy_s(rule.Description, RTL_NUMBER_OF(rule.Description), 
             L"Monitor auto-start registry modifications");
    
    status = HipsAddRule(&rule);
    if (!NT_SUCCESS(status)) {
        HipsDbgPrint("Failed to add default rule 2: 0x%08X\n", status);
    }

    // Rule 3: Block process hollowing attempts
    RtlZeroMemory(&rule, sizeof(rule));
    rule.EventType = HIPS_EVENT_PROCESS_CREATED;
    rule.Action = HIPS_ACTION_ALERT_ONLY;
    rule.MinThreatLevel = HIPS_THREAT_HIGH;
    rule.Enabled = TRUE;
    wcscpy_s(rule.Pattern, RTL_NUMBER_OF(rule.Pattern), L"*svchost.exe");
    wcscpy_s(rule.Description, RTL_NUMBER_OF(rule.Description), 
             L"Monitor suspicious svchost.exe processes");
    
    status = HipsAddRule(&rule);
    if (!NT_SUCCESS(status)) {
        HipsDbgPrint("Failed to add default rule 3: 0x%08X\n", status);
    }

    // Rule 4: Block critical system file modifications
    RtlZeroMemory(&rule, sizeof(rule));
    rule.EventType = HIPS_EVENT_FILE_WRITTEN;
    rule.Action = HIPS_ACTION_DENY;
    rule.MinThreatLevel = HIPS_THREAT_CRITICAL;
    rule.Enabled = TRUE;
    wcscpy_s(rule.Pattern, RTL_NUMBER_OF(rule.Pattern), L"*\\System32\\*");
    wcscpy_s(rule.Description, RTL_NUMBER_OF(rule.Description), 
             L"Block unauthorized System32 modifications");
    
    status = HipsAddRule(&rule);
    if (!NT_SUCCESS(status)) {
        HipsDbgPrint("Failed to add default rule 4: 0x%08X\n", status);
    }

    // Rule 5: Monitor file deletions
    RtlZeroMemory(&rule, sizeof(rule));
    rule.EventType = HIPS_EVENT_FILE_DELETED;
    rule.Action = HIPS_ACTION_ALERT_ONLY;
    rule.MinThreatLevel = HIPS_THREAT_MEDIUM;
    rule.Enabled = TRUE;
    wcscpy_s(rule.Pattern, RTL_NUMBER_OF(rule.Pattern), L"*.exe");
    wcscpy_s(rule.Description, RTL_NUMBER_OF(rule.Description), 
             L"Monitor executable file deletions");
    
    status = HipsAddRule(&rule);
    if (!NT_SUCCESS(status)) {
        HipsDbgPrint("Failed to add default rule 5: 0x%08X\n", status);
    }

    HipsDbgPrint("Default security rules initialized\n");
    return STATUS_SUCCESS;
}

/**
 * Clean up all rules
 */
VOID HipsCleanupRules(void)
{
    KIRQL oldIrql;
    PLIST_ENTRY listEntry;
    PHIPS_RULE rule;

    if (!g_DriverContext) {
        return;
    }

    KeAcquireSpinLock(&g_DriverContext->Lock, &oldIrql);

    // Remove and free all rules
    while (!IsListEmpty(&g_DriverContext->RuleList)) {
        listEntry = RemoveHeadList(&g_DriverContext->RuleList);
        rule = CONTAINING_RECORD(listEntry, HIPS_RULE, ListEntry);
        HipsFreeMemory(rule);
    }

    KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);

    HipsDbgPrint("All security rules cleaned up\n");
}

/**
 * Get rule statistics
 */
NTSTATUS HipsGetRuleStatistics(
    _Out_ PULONG RuleCount,
    _Out_ PULONG EnabledRuleCount
)
{
    KIRQL oldIrql;
    PLIST_ENTRY listEntry;
    PHIPS_RULE rule;
    ULONG totalCount = 0;
    ULONG enabledCount = 0;

    if (!g_DriverContext || !RuleCount || !EnabledRuleCount) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&g_DriverContext->Lock, &oldIrql);

    // Count rules
    listEntry = g_DriverContext->RuleList.Flink;
    while (listEntry != &g_DriverContext->RuleList) {
        rule = CONTAINING_RECORD(listEntry, HIPS_RULE, ListEntry);
        totalCount++;
        
        if (rule->Enabled) {
            enabledCount++;
        }
        
        listEntry = listEntry->Flink;
    }

    KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);

    *RuleCount = totalCount;
    *EnabledRuleCount = enabledCount;

    return STATUS_SUCCESS;
}

/**
 * Enable or disable a specific rule
 */
NTSTATUS HipsSetRuleEnabled(
    _In_ ULONG RuleId,
    _In_ BOOLEAN Enabled
)
{
    KIRQL oldIrql;
    PLIST_ENTRY listEntry;
    PHIPS_RULE rule;
    BOOLEAN found = FALSE;

    if (!g_DriverContext) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&g_DriverContext->Lock, &oldIrql);

    // Search for the rule
    listEntry = g_DriverContext->RuleList.Flink;
    while (listEntry != &g_DriverContext->RuleList) {
        rule = CONTAINING_RECORD(listEntry, HIPS_RULE, ListEntry);
        
        if (rule->RuleId == RuleId) {
            rule->Enabled = Enabled;
            found = TRUE;
            break;
        }
        
        listEntry = listEntry->Flink;
    }

    KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);

    if (found) {
        HipsDbgPrint("Rule %lu %s\n", RuleId, Enabled ? "enabled" : "disabled");
        return STATUS_SUCCESS;
    }

    return STATUS_NOT_FOUND;
}

/**
 * Update rule action
 */
NTSTATUS HipsSetRuleAction(
    _In_ ULONG RuleId,
    _In_ HIPS_ACTION_TYPE Action
)
{
    KIRQL oldIrql;
    PLIST_ENTRY listEntry;
    PHIPS_RULE rule;
    BOOLEAN found = FALSE;

    if (!g_DriverContext) {
        return STATUS_INVALID_PARAMETER;
    }

    KeAcquireSpinLock(&g_DriverContext->Lock, &oldIrql);

    // Search for the rule
    listEntry = g_DriverContext->RuleList.Flink;
    while (listEntry != &g_DriverContext->RuleList) {
        rule = CONTAINING_RECORD(listEntry, HIPS_RULE, ListEntry);
        
        if (rule->RuleId == RuleId) {
            rule->Action = Action;
            found = TRUE;
            break;
        }
        
        listEntry = listEntry->Flink;
    }

    KeReleaseSpinLock(&g_DriverContext->Lock, oldIrql);

    if (found) {
        HipsDbgPrint("Rule %lu action updated to %d\n", RuleId, (int)Action);
        return STATUS_SUCCESS;
    }

    return STATUS_NOT_FOUND;
}