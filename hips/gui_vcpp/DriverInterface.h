#pragma once

#include <vector>
#include <string>

// Forward declarations and enums from HIPS core
enum class EventType {
    FILE_ACCESS = 1,
    PROCESS_CREATED,
    PROCESS_TERMINATED,
    REGISTRY_MODIFIED,
    MEMORY_INJECTION,
    NETWORK_CONNECTION
};

enum class ThreatLevel {
    LOW = 1,
    MEDIUM,
    HIGH,
    CRITICAL
};

struct SecurityEvent {
    EventType event_type;
    ThreatLevel threat_level;
    unsigned long long timestamp;
    unsigned long process_id;
    unsigned long thread_id;
    std::string process_path;
    std::string target_path;
    std::string additional_data;
};

/**
 * Driver Interface Class for MFC GUI
 * 
 * Simplified interface to communicate with the HIPS kernel driver
 */
class CDriverInterface
{
public:
    CDriverInterface();
    ~CDriverInterface();

    // Connection management
    BOOL ConnectToDriver();
    void DisconnectFromDriver();
    BOOL IsConnected() const;

    // Driver control
    BOOL StartDriverMonitoring();
    BOOL StopDriverMonitoring();
    BOOL GetDriverVersion(DWORD& version);

    // Event management
    BOOL GetEventsFromDriver(std::vector<SecurityEvent>& events);

    // Configuration
    BOOL SendConfigurationToDriver(const std::map<std::string, std::string>& config);

private:
    HANDLE m_driverHandle;
    std::string m_devicePath;
    mutable CRITICAL_SECTION m_csLock;

    // Helper methods
    BOOL SendControlCode(DWORD controlCode, 
                        PVOID inputBuffer = nullptr, 
                        DWORD inputSize = 0,
                        PVOID outputBuffer = nullptr, 
                        DWORD outputSize = 0,
                        PDWORD bytesReturned = nullptr);
    
    SecurityEvent ConvertDriverEvent(const void* driverEventPtr);
};