#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

namespace HIPS {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

class LogManager {
public:
    LogManager();
    ~LogManager();

    bool Initialize();
    void SetLogLevel(LogLevel level);
    void SetLogFile(const std::string& file_path);
    
    void LogDebug(const std::string& message);
    void LogInfo(const std::string& message);
    void LogWarning(const std::string& message);
    void LogError(const std::string& message);
    void LogCritical(const std::string& message);

private:
    LogLevel current_level_;
    std::unique_ptr<std::ofstream> log_file_;
    std::mutex log_mutex_;
    
    void WriteLog(LogLevel level, const std::string& message);
    std::string GetTimestamp();
    std::string LogLevelToString(LogLevel level);
};

} // namespace HIPS

#endif // LOG_MANAGER_H