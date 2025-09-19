/*
 * Driver Communication Interface
 * 
 * This module provides the interface between the user-mode HIPS application
 * and the kernel-mode driver for enhanced monitoring capabilities.
 */

#ifndef DRIVER_INTERFACE_H
#define DRIVER_INTERFACE_H

#include "hips_core.h"
#include <Windows.h>

namespace HIPS {

// Driver communication class
class DriverInterface {
public:
    DriverInterface();
    ~DriverInterface();

    // Driver management
    bool ConnectToDriver();
    void DisconnectFromDriver();
    bool IsConnected() const { return driver_handle_ != INVALID_HANDLE_VALUE; }

    // Driver control
    bool StartDriverMonitoring();
    bool StopDriverMonitoring();
    bool GetDriverVersion(DWORD& version);

    // Event retrieval
    bool GetEventsFromDriver(std::vector<SecurityEvent>& events);
    
    // Configuration
    bool SendConfigurationToDriver(const std::unordered_map<std::string, std::string>& config);
    
    // Rule management
    bool AddRuleToDriver(const SecurityRule& rule);
    bool RemoveRuleFromDriver(const std::string& ruleName);

    // Statistics
    bool GetDriverStatistics(uint64_t& eventCount, uint64_t& queueSize);

private:
    HANDLE driver_handle_;
    std::string device_path_;
    std::mutex communication_mutex_;
    
    // Internal communication methods
    bool SendControlCode(DWORD controlCode, 
                        PVOID inputBuffer = nullptr, 
                        DWORD inputSize = 0,
                        PVOID outputBuffer = nullptr, 
                        DWORD outputSize = 0,
                        PDWORD bytesReturned = nullptr);
    
    SecurityEvent ConvertDriverEvent(const void* driverEvent);
    void* ConvertToDriverRule(const SecurityRule& rule);
};

// Driver event structure (matches kernel driver)
#pragma pack(push, 1)
struct DriverEvent {
    DWORD event_type;
    DWORD threat_level;
    FILETIME timestamp;
    DWORD process_id;
    DWORD thread_id;
    DWORD data_length;
    WCHAR file_path[260];
    WCHAR process_path[260];
    CHAR additional_data[512];
};

struct DriverRule {
    DWORD rule_id;
    DWORD event_type;
    DWORD action;
    DWORD min_threat_level;
    BOOLEAN enabled;
    WCHAR pattern[260];
    WCHAR description[512];
};

struct DriverConfig {
    BOOLEAN monitor_filesystem;
    BOOLEAN monitor_processes;
    BOOLEAN monitor_registry;
    BOOLEAN monitor_network;
    BOOLEAN monitor_memory;
    DWORD minimum_threat_level;
    DWORD max_event_queue_size;
    DWORD event_timeout_ms;
};
#pragma pack(pop)

// Device control codes (must match driver)
#define HIPS_DEVICE_TYPE 0x8000
#define IOCTL_HIPS_GET_VERSION      CTL_CODE(HIPS_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_HIPS_START_MONITORING CTL_CODE(HIPS_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_HIPS_STOP_MONITORING  CTL_CODE(HIPS_DEVICE_TYPE, 0x802, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_HIPS_GET_EVENTS       CTL_CODE(HIPS_DEVICE_TYPE, 0x803, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_HIPS_SET_CONFIG       CTL_CODE(HIPS_DEVICE_TYPE, 0x804, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_HIPS_ADD_RULE         CTL_CODE(HIPS_DEVICE_TYPE, 0x805, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_HIPS_REMOVE_RULE      CTL_CODE(HIPS_DEVICE_TYPE, 0x806, METHOD_BUFFERED, FILE_WRITE_DATA)

} // namespace HIPS

#endif // DRIVER_INTERFACE_H