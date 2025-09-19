#include "stdafx.h"
#include "DriverInterface.h"
#include <winioctl.h>

// Driver IOCTL codes (must match driver definitions)
#define HIPS_DEVICE_TYPE 0x8000
#define IOCTL_HIPS_GET_VERSION      CTL_CODE(HIPS_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_HIPS_START_MONITORING CTL_CODE(HIPS_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_HIPS_STOP_MONITORING  CTL_CODE(HIPS_DEVICE_TYPE, 0x802, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_HIPS_GET_EVENTS       CTL_CODE(HIPS_DEVICE_TYPE, 0x803, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_HIPS_SET_CONFIG       CTL_CODE(HIPS_DEVICE_TYPE, 0x804, METHOD_BUFFERED, FILE_WRITE_DATA)

// Driver event structure (must match driver definition)
#pragma pack(push, 1)
typedef struct _DRIVER_EVENT {
    DWORD EventType;
    DWORD ThreatLevel;
    LARGE_INTEGER Timestamp;
    ULONG ProcessId;
    ULONG ThreadId;
    WCHAR FilePath[260];
    WCHAR ProcessPath[260];
    CHAR AdditionalData[512];
} DRIVER_EVENT, *PDRIVER_EVENT;
#pragma pack(pop)

CDriverInterface::CDriverInterface()
    : m_driverHandle(INVALID_HANDLE_VALUE)
    , m_devicePath("\\\\.\\HipsDriver")
{
    InitializeCriticalSection(&m_csLock);
}

CDriverInterface::~CDriverInterface()
{
    DisconnectFromDriver();
    DeleteCriticalSection(&m_csLock);
}

BOOL CDriverInterface::ConnectToDriver()
{
    EnterCriticalSection(&m_csLock);
    
    if (IsConnected()) {
        LeaveCriticalSection(&m_csLock);
        return TRUE;
    }

    // Try to open the driver device
    m_driverHandle = CreateFileA(
        m_devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    BOOL result = (m_driverHandle != INVALID_HANDLE_VALUE);
    
    LeaveCriticalSection(&m_csLock);
    return result;
}

void CDriverInterface::DisconnectFromDriver()
{
    EnterCriticalSection(&m_csLock);
    
    if (m_driverHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_driverHandle);
        m_driverHandle = INVALID_HANDLE_VALUE;
    }
    
    LeaveCriticalSection(&m_csLock);
}

BOOL CDriverInterface::IsConnected() const
{
    return (m_driverHandle != INVALID_HANDLE_VALUE);
}

BOOL CDriverInterface::StartDriverMonitoring()
{
    if (!IsConnected()) {
        return FALSE;
    }

    return SendControlCode(IOCTL_HIPS_START_MONITORING);
}

BOOL CDriverInterface::StopDriverMonitoring()
{
    if (!IsConnected()) {
        return FALSE;
    }

    return SendControlCode(IOCTL_HIPS_STOP_MONITORING);
}

BOOL CDriverInterface::GetDriverVersion(DWORD& version)
{
    if (!IsConnected()) {
        return FALSE;
    }

    DWORD bytesReturned = 0;
    return SendControlCode(IOCTL_HIPS_GET_VERSION, 
                          nullptr, 0,
                          &version, sizeof(DWORD),
                          &bytesReturned);
}

BOOL CDriverInterface::GetEventsFromDriver(std::vector<SecurityEvent>& events)
{
    if (!IsConnected()) {
        return FALSE;
    }

    // Allocate buffer for driver events
    const DWORD bufferSize = sizeof(DRIVER_EVENT) * 100; // Get up to 100 events
    std::vector<BYTE> buffer(bufferSize);
    
    DWORD bytesReturned = 0;
    if (!SendControlCode(IOCTL_HIPS_GET_EVENTS,
                        nullptr, 0,
                        buffer.data(), bufferSize,
                        &bytesReturned)) {
        return FALSE;
    }

    // Convert driver events to SecurityEvent objects
    events.clear();
    
    DWORD eventCount = bytesReturned / sizeof(DRIVER_EVENT);
    PDRIVER_EVENT driverEvents = reinterpret_cast<PDRIVER_EVENT>(buffer.data());
    
    for (DWORD i = 0; i < eventCount; ++i) {
        events.push_back(ConvertDriverEvent(&driverEvents[i]));
    }

    return TRUE;
}

BOOL CDriverInterface::SendConfigurationToDriver(const std::map<std::string, std::string>& config)
{
    // TODO: Implement configuration sending
    UNREFERENCED_PARAMETER(config);
    return FALSE;
}

BOOL CDriverInterface::SendControlCode(DWORD controlCode, 
                                      PVOID inputBuffer, 
                                      DWORD inputSize,
                                      PVOID outputBuffer, 
                                      DWORD outputSize,
                                      PDWORD bytesReturned)
{
    if (!IsConnected()) {
        return FALSE;
    }

    DWORD localBytesReturned = 0;
    if (!bytesReturned) {
        bytesReturned = &localBytesReturned;
    }

    BOOL result = DeviceIoControl(
        m_driverHandle,
        controlCode,
        inputBuffer,
        inputSize,
        outputBuffer,
        outputSize,
        bytesReturned,
        nullptr
    );

    return result;
}

SecurityEvent CDriverInterface::ConvertDriverEvent(const void* driverEventPtr)
{
    const DRIVER_EVENT* driverEvent = static_cast<const DRIVER_EVENT*>(driverEventPtr);
    SecurityEvent event;

    // Convert event type
    switch (driverEvent->EventType) {
        case 1: event.event_type = EventType::FILE_ACCESS; break;
        case 3: event.event_type = EventType::PROCESS_CREATED; break;
        case 4: event.event_type = EventType::PROCESS_TERMINATED; break;
        case 7: event.event_type = EventType::REGISTRY_MODIFIED; break;
        case 9: event.event_type = EventType::MEMORY_INJECTION; break;
        case 10: event.event_type = EventType::NETWORK_CONNECTION; break;
        default: event.event_type = EventType::FILE_ACCESS; break;
    }

    // Convert threat level
    switch (driverEvent->ThreatLevel) {
        case 0: event.threat_level = ThreatLevel::LOW; break;
        case 1: event.threat_level = ThreatLevel::LOW; break;
        case 2: event.threat_level = ThreatLevel::MEDIUM; break;
        case 3: event.threat_level = ThreatLevel::HIGH; break;
        case 4: event.threat_level = ThreatLevel::CRITICAL; break;
        default: event.threat_level = ThreatLevel::LOW; break;
    }
    
    // Convert other fields
    event.process_id = driverEvent->ProcessId;
    event.thread_id = driverEvent->ThreadId;
    event.timestamp = driverEvent->Timestamp.QuadPart;
    
    // Convert wide strings to regular strings
    if (driverEvent->FilePath[0] != L'\0') {
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, driverEvent->FilePath, -1, nullptr, 0, nullptr, nullptr);
        if (sizeNeeded > 0) {
            std::vector<char> buffer(sizeNeeded);
            WideCharToMultiByte(CP_UTF8, 0, driverEvent->FilePath, -1, buffer.data(), sizeNeeded, nullptr, nullptr);
            event.target_path = std::string(buffer.data());
        }
    }
    
    if (driverEvent->ProcessPath[0] != L'\0') {
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, driverEvent->ProcessPath, -1, nullptr, 0, nullptr, nullptr);
        if (sizeNeeded > 0) {
            std::vector<char> buffer(sizeNeeded);
            WideCharToMultiByte(CP_UTF8, 0, driverEvent->ProcessPath, -1, buffer.data(), sizeNeeded, nullptr, nullptr);
            event.process_path = std::string(buffer.data());
        }
    }
    
    // Convert additional data
    if (driverEvent->AdditionalData[0] != '\0') {
        event.additional_data = std::string(driverEvent->AdditionalData);
    }

    return event;
}