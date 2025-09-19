#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#include "hips_core.h"
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <string>
#include <thread>
#include <atomic>
#include <functional>

namespace HIPS {

struct NetworkConnection {
    DWORD process_id;
    std::string local_address;
    DWORD local_port;
    std::string remote_address;
    DWORD remote_port;
    DWORD protocol;
    DWORD state;
    std::string process_name;
};

class NetworkMonitor {
public:
    NetworkMonitor();
    ~NetworkMonitor();

    bool Initialize();
    bool Start();
    bool Stop();
    bool Shutdown();

    void RegisterCallback(std::function<void(const SecurityEvent&)> callback);
    
    bool IsRunning() const { return running_.load(); }
    bool IsInitialized() const { return initialized_.load(); }

private:
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    std::thread monitor_thread_;
    std::function<void(const SecurityEvent&)> event_callback_;
    
    void MonitoringThreadFunction();
    void ScanNetworkConnections();
    SecurityEvent CreateNetworkEvent(const NetworkConnection& conn);
    ThreatLevel EvaluateNetworkThreat(const NetworkConnection& conn);
    
    std::vector<NetworkConnection> GetTcpConnections();
    std::vector<NetworkConnection> GetUdpConnections();
    std::string GetProcessNameFromPID(DWORD pid);
    bool IsSuspiciousConnection(const NetworkConnection& conn);
};

} // namespace HIPS

#endif // NETWORK_MONITOR_H