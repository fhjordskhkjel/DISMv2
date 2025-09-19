#ifndef MEMORY_PROTECTOR_H
#define MEMORY_PROTECTOR_H

#include "hips_core.h"
#include <string>
#include <functional>
#include <atomic>

namespace HIPS {

class MemoryProtector {
public:
    MemoryProtector();
    ~MemoryProtector();

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
    std::function<void(const SecurityEvent&)> event_callback_;
    
    void SetupMemoryProtection();
    SecurityEvent CreateMemoryEvent(const std::string& description);
};

} // namespace HIPS

#endif // MEMORY_PROTECTOR_H