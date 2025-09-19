#include "log_manager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace HIPS {

LogManager::LogManager() : current_level_(LogLevel::INFO) {
}

LogManager::~LogManager() {
    if (log_file_ && log_file_->is_open()) {
        log_file_->close();
    }
}

bool LogManager::Initialize() {
    SetLogFile("hips.log");
    return true;
}

void LogManager::SetLogLevel(LogLevel level) {
    current_level_ = level;
}

void LogManager::SetLogFile(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    if (log_file_ && log_file_->is_open()) {
        log_file_->close();
    }
    
    log_file_ = std::make_unique<std::ofstream>(file_path, std::ios::app);
}

void LogManager::LogDebug(const std::string& message) {
    WriteLog(LogLevel::DEBUG, message);
}

void LogManager::LogInfo(const std::string& message) {
    WriteLog(LogLevel::INFO, message);
}

void LogManager::LogWarning(const std::string& message) {
    WriteLog(LogLevel::WARNING, message);
}

void LogManager::LogError(const std::string& message) {
    WriteLog(LogLevel::ERROR, message);
}

void LogManager::LogCritical(const std::string& message) {
    WriteLog(LogLevel::CRITICAL, message);
}

void LogManager::WriteLog(LogLevel level, const std::string& message) {
    if (static_cast<int>(level) < static_cast<int>(current_level_)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    std::string log_entry = GetTimestamp() + " [" + LogLevelToString(level) + "] " + message;
    
    // Write to console
    std::cout << log_entry << std::endl;
    
    // Write to file
    if (log_file_ && log_file_->is_open()) {
        *log_file_ << log_entry << std::endl;
        log_file_->flush();
    }
}

std::string LogManager::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string LogManager::LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

} // namespace HIPS