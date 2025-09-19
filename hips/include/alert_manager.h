#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include "hips_core.h"
#include <string>
#include <vector>
#include <mutex>

namespace HIPS {

struct Alert {
    SecurityEvent event;
    std::string message;
    SYSTEMTIME timestamp;
    bool acknowledged;
};

class AlertManager {
public:
    AlertManager();
    ~AlertManager();

    bool Initialize();
    void SendAlert(const SecurityEvent& event, const std::string& message);
    std::vector<Alert> GetAlerts(bool include_acknowledged = false);
    void AcknowledgeAlert(size_t index);
    void ClearAlerts();

private:
    std::vector<Alert> alerts_;
    std::mutex alerts_mutex_;
    
    void NotifyUser(const Alert& alert);
    void WriteAlertToLog(const Alert& alert);
};

} // namespace HIPS

#endif // ALERT_MANAGER_H