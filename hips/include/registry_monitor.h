#ifndef REGISTRY_MONITOR_H
#define REGISTRY_MONITOR_H

#include "hips_core.h"
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>

namespace HIPS {

class RegistryMonitor {
public:
    RegistryMonitor();
    ~RegistryMonitor();

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
    
    std::vector<HKEY> monitored_keys_;
    
    void MonitoringThreadFunction();
    void SetupRegistryWatching();
    SecurityEvent CreateRegistryEvent(const std::string& key_path);
    ThreatLevel EvaluateRegistryThreat(const std::string& key_path);
    bool IsCriticalRegistryKey(const std::string& key_path);
};

} // namespace HIPS

#endif // REGISTRY_MONITOR_H