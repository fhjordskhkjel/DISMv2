#include "alert_manager.h"
#include <iostream>
#include <algorithm>

namespace HIPS {

AlertManager::AlertManager() {
}

AlertManager::~AlertManager() {
}

bool AlertManager::Initialize() {
    return true;
}

void AlertManager::SendAlert(const SecurityEvent& event, const std::string& message) {
    Alert alert;
    alert.event = event;
    alert.message = message;
    alert.acknowledged = false;
    GetSystemTime(&alert.timestamp);
    
    {
        std::lock_guard<std::mutex> lock(alerts_mutex_);
        alerts_.push_back(alert);
    }
    
    NotifyUser(alert);
    WriteAlertToLog(alert);
}

std::vector<Alert> AlertManager::GetAlerts(bool include_acknowledged) {
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    
    if (include_acknowledged) {
        return alerts_;
    } else {
        std::vector<Alert> unacknowledged;
        std::copy_if(alerts_.begin(), alerts_.end(), std::back_inserter(unacknowledged),
            [](const Alert& alert) { return !alert.acknowledged; });
        return unacknowledged;
    }
}

void AlertManager::AcknowledgeAlert(size_t index) {
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    if (index < alerts_.size()) {
        alerts_[index].acknowledged = true;
    }
}

void AlertManager::ClearAlerts() {
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    alerts_.clear();
}

void AlertManager::NotifyUser(const Alert& alert) {
    // In a real implementation, this would show system notifications
    std::cout << "[ALERT] " << alert.message << std::endl;
}

void AlertManager::WriteAlertToLog(const Alert& alert) {
    // This would integrate with the log manager
    std::cout << "[LOG] Alert: " << alert.message << " | Event: " 
              << EventTypeToString(alert.event.type) << std::endl;
}

} // namespace HIPS