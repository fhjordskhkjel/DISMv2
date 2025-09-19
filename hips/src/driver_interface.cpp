/*
 * Driver Communication Interface Implementation
 * 
 * Implements communication between user-mode HIPS application
 * and kernel-mode driver.
 */

#include "driver_interface.h"
#include <iostream>
#include <vector>
#include <algorithm>

namespace HIPS {

DriverInterface::DriverInterface() 
    : driver_handle_(INVALID_HANDLE_VALUE)
    , device_path_(R"(\\.\HipsDriver)")
{
}

DriverInterface::~DriverInterface() 
{
    DisconnectFromDriver();
}

bool DriverInterface::ConnectToDriver()
{
    std::lock_guard<std::mutex> lock(communication_mutex_);
    
    if (IsConnected()) {
        return true;
    }

    // Try to open the driver device
    driver_handle_ = CreateFileA(
        device_path_.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (driver_handle_ == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        std::cerr << "Failed to connect to HIPS driver: " << error << std::endl;
        return false;
    }

    std::cout << "Connected to HIPS kernel driver successfully" << std::endl;
    return true;
}

void DriverInterface::DisconnectFromDriver()
{
    std::lock_guard<std::mutex> lock(communication_mutex_);
    
    if (driver_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(driver_handle_);
        driver_handle_ = INVALID_HANDLE_VALUE;
        std::cout << "Disconnected from HIPS driver" << std::endl;
    }
}

bool DriverInterface::StartDriverMonitoring()
{
    if (!IsConnected()) {
        return false;
    }

    bool result = SendControlCode(IOCTL_HIPS_START_MONITORING);
    if (result) {
        std::cout << "Driver monitoring started" << std::endl;
    } else {
        std::cerr << "Failed to start driver monitoring" << std::endl;
    }
    
    return result;
}

bool DriverInterface::StopDriverMonitoring()
{
    if (!IsConnected()) {
        return false;
    }

    bool result = SendControlCode(IOCTL_HIPS_STOP_MONITORING);
    if (result) {
        std::cout << "Driver monitoring stopped" << std::endl;
    } else {
        std::cerr << "Failed to stop driver monitoring" << std::endl;
    }
    
    return result;
}

bool DriverInterface::GetDriverVersion(DWORD& version)
{
    if (!IsConnected()) {
        return false;
    }

    DWORD bytesReturned = 0;
    return SendControlCode(
        IOCTL_HIPS_GET_VERSION,
        nullptr, 0,
        &version, sizeof(version),
        &bytesReturned
    ) && bytesReturned == sizeof(version);
}

bool DriverInterface::GetEventsFromDriver(std::vector<SecurityEvent>& events)
{
    if (!IsConnected()) {
        return false;
    }

    // Allocate buffer for driver events
    const size_t maxEvents = 100;
    const size_t bufferSize = maxEvents * sizeof(DriverEvent);
    std::vector<BYTE> buffer(bufferSize);
    
    DWORD bytesReturned = 0;
    bool result = SendControlCode(
        IOCTL_HIPS_GET_EVENTS,
        nullptr, 0,
        buffer.data(), static_cast<DWORD>(bufferSize),
        &bytesReturned
    );

    if (!result || bytesReturned == 0) {
        return false;
    }

    // Convert driver events to SecurityEvent objects
    events.clear();
    size_t eventCount = bytesReturned / sizeof(DriverEvent);
    DriverEvent* driverEvents = reinterpret_cast<DriverEvent*>(buffer.data());
    
    for (size_t i = 0; i < eventCount; ++i) {
        SecurityEvent event = ConvertDriverEvent(&driverEvents[i]);
        events.push_back(std::move(event));
    }

    return true;
}

bool DriverInterface::SendConfigurationToDriver(const std::unordered_map<std::string, std::string>& config)
{
    if (!IsConnected()) {
        return false;
    }

    // Convert configuration to driver format
    DriverConfig driverConfig = {};
    
    auto getValue = [&config](const std::string& key, bool defaultValue = false) -> bool {
        auto it = config.find(key);
        if (it != config.end()) {
            return it->second == "true" || it->second == "1";
        }
        return defaultValue;
    };

    auto getIntValue = [&config](const std::string& key, DWORD defaultValue = 0) -> DWORD {
        auto it = config.find(key);
        if (it != config.end()) {
            return static_cast<DWORD>(std::stoul(it->second));
        }
        return defaultValue;
    };

    driverConfig.monitor_filesystem = getValue("monitor_filesystem", true);
    driverConfig.monitor_processes = getValue("monitor_processes", true);
    driverConfig.monitor_registry = getValue("monitor_registry", true);
    driverConfig.monitor_network = getValue("monitor_network", true);
    driverConfig.monitor_memory = getValue("monitor_memory", true);
    driverConfig.minimum_threat_level = getIntValue("minimum_threat_level", 1);
    driverConfig.max_event_queue_size = getIntValue("max_event_queue_size", 1000);
    driverConfig.event_timeout_ms = getIntValue("event_timeout_ms", 5000);

    return SendControlCode(
        IOCTL_HIPS_SET_CONFIG,
        &driverConfig, sizeof(driverConfig)
    );
}

bool DriverInterface::SendControlCode(DWORD controlCode, 
                                     PVOID inputBuffer, 
                                     DWORD inputSize,
                                     PVOID outputBuffer, 
                                     DWORD outputSize,
                                     PDWORD bytesReturned)
{
    if (!IsConnected()) {
        return false;
    }

    DWORD localBytesReturned = 0;
    if (!bytesReturned) {
        bytesReturned = &localBytesReturned;
    }

    BOOL result = DeviceIoControl(
        driver_handle_,
        controlCode,
        inputBuffer,
        inputSize,
        outputBuffer,
        outputSize,
        bytesReturned,
        nullptr
    );

    if (!result) {
        DWORD error = GetLastError();
        std::cerr << "DeviceIoControl failed with error: " << error << std::endl;
        return false;
    }

    return true;
}

SecurityEvent DriverInterface::ConvertDriverEvent(const void* driverEventPtr)
{
    const DriverEvent* driverEvent = static_cast<const DriverEvent*>(driverEventPtr);
    
    SecurityEvent event;
    
    // Convert event type
    switch (driverEvent->event_type) {
        case 1: event.type = EventType::FILE_ACCESS; break;
        case 2: event.type = EventType::FILE_MODIFICATION; break;
        case 3: event.type = EventType::FILE_DELETION; break;
        case 4: event.type = EventType::PROCESS_CREATION; break;
        case 5: event.type = EventType::PROCESS_TERMINATION; break;
        case 6: event.type = EventType::REGISTRY_MODIFICATION; break;
        case 7: event.type = EventType::MEMORY_INJECTION; break;
        case 8: event.type = EventType::NETWORK_CONNECTION; break;
        default: event.type = EventType::FILE_ACCESS; break;
    }
    
    // Convert threat level
    switch (driverEvent->threat_level) {
        case 0: event.threat_level = ThreatLevel::LOW; break;
        case 1: event.threat_level = ThreatLevel::LOW; break;
        case 2: event.threat_level = ThreatLevel::MEDIUM; break;
        case 3: event.threat_level = ThreatLevel::HIGH; break;
        case 4: event.threat_level = ThreatLevel::CRITICAL; break;
        default: event.threat_level = ThreatLevel::LOW; break;
    }
    
    // Convert other fields
    event.process_id = driverEvent->process_id;
    event.thread_id = driverEvent->thread_id;
    
    // Convert wide strings to regular strings
    if (driverEvent->file_path[0] != L'\0') {
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, driverEvent->file_path, -1, nullptr, 0, nullptr, nullptr);
        if (sizeNeeded > 0) {
            std::vector<char> buffer(sizeNeeded);
            WideCharToMultiByte(CP_UTF8, 0, driverEvent->file_path, -1, buffer.data(), sizeNeeded, nullptr, nullptr);
            event.target_path = std::string(buffer.data());
        }
    }
    
    if (driverEvent->process_path[0] != L'\0') {
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, driverEvent->process_path, -1, nullptr, 0, nullptr, nullptr);
        if (sizeNeeded > 0) {
            std::vector<char> buffer(sizeNeeded);
            WideCharToMultiByte(CP_UTF8, 0, driverEvent->process_path, -1, buffer.data(), sizeNeeded, nullptr, nullptr);
            event.process_path = std::string(buffer.data());
        }
    }
    
    // Convert timestamp
    SYSTEMTIME systemTime;
    FileTimeToSystemTime(&driverEvent->timestamp, &systemTime);
    event.timestamp = systemTime;
    
    // Set description based on event type and threat level
    event.description = "Kernel driver event: " + std::to_string(static_cast<int>(event.type)) + 
                       ", threat level: " + std::to_string(static_cast<int>(event.threat_level));
    
    return event;
}

} // namespace HIPS