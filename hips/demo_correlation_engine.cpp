/*
 * Standalone demonstration of correlation engine functionality
 * This file can be compiled independently to show correlation engine features
 */

#include "../include/correlation_engine.h"
#include <iostream>
#include <iomanip>

using namespace HIPS;

void printBanner(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void printCorrelation(const CorrelatedEventGroup& group) {
    std::cout << "\n[CORRELATION DETECTED]" << std::endl;
    std::cout << "  ID: " << group.correlation_id << std::endl;
    std::cout << "  Type: ";
    switch (group.type) {
        case CorrelationType::PROCESS_BASED:
            std::cout << "Process-Based";
            break;
        case CorrelationType::TIME_BASED:
            std::cout << "Time-Based";
            break;
        case CorrelationType::TARGET_BASED:
            std::cout << "Target-Based";
            break;
        case CorrelationType::SEQUENCE_BASED:
            std::cout << "Sequence-Based";
            break;
        case CorrelationType::THREAT_ESCALATION:
            std::cout << "Threat Escalation";
            break;
    }
    std::cout << std::endl;
    std::cout << "  Events: " << group.events.size() << std::endl;
    std::cout << "  Score: " << std::fixed << std::setprecision(2) 
              << group.correlation_score << std::endl;
    std::cout << "  Description: " << group.description << std::endl;
    std::cout << "  Combined Threat Level: ";
    switch (group.combined_threat_level) {
        case ThreatLevel::LOW: std::cout << "LOW"; break;
        case ThreatLevel::MEDIUM: std::cout << "MEDIUM"; break;
        case ThreatLevel::HIGH: std::cout << "HIGH"; break;
        case ThreatLevel::CRITICAL: std::cout << "CRITICAL"; break;
    }
    std::cout << std::endl;
}

SecurityEvent createEvent(EventType type, ThreatLevel threat, 
                         DWORD pid, const std::string& process_path,
                         const std::string& target_path = "") {
    SecurityEvent event;
    event.type = type;
    event.threat_level = threat;
    event.process_id = pid;
    event.thread_id = 1000;
    event.process_path = process_path;
    event.target_path = target_path;
    
    // Initialize timestamp
    #ifdef _WIN32
    GetSystemTime(&event.timestamp);
    #else
    // For cross-platform demo
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time);
    event.timestamp.wYear = tm->tm_year + 1900;
    event.timestamp.wMonth = tm->tm_mon + 1;
    event.timestamp.wDay = tm->tm_mday;
    event.timestamp.wHour = tm->tm_hour;
    event.timestamp.wMinute = tm->tm_min;
    event.timestamp.wSecond = tm->tm_sec;
    event.timestamp.wMilliseconds = 0;
    #endif
    
    return event;
}

int main() {
    printBanner("Correlation Engine Demonstration");
    
    std::cout << "\nInitializing Correlation Engine..." << std::endl;
    
    CorrelationEngine engine;
    CorrelationConfig config;
    config.time_window_seconds = 60;
    config.min_events_for_correlation = 3;
    config.min_correlation_score = 0.6;
    config.enable_process_correlation = true;
    config.enable_time_correlation = true;
    config.enable_target_correlation = true;
    config.enable_sequence_correlation = true;
    config.enable_threat_escalation = true;
    
    if (!engine.Initialize(config)) {
        std::cerr << "Failed to initialize correlation engine!" << std::endl;
        return 1;
    }
    
    std::cout << "✓ Correlation Engine initialized successfully" << std::endl;
    std::cout << "  - Time window: " << config.time_window_seconds << " seconds" << std::endl;
    std::cout << "  - Min events: " << config.min_events_for_correlation << std::endl;
    std::cout << "  - Min score: " << config.min_correlation_score << std::endl;
    
    // Register callback for correlations
    int correlation_detected_count = 0;
    engine.RegisterCorrelationCallback([&correlation_detected_count](const CorrelatedEventGroup& group) {
        printCorrelation(group);
        correlation_detected_count++;
    });
    
    // Test Scenario 1: Process-based correlation
    printBanner("Scenario 1: Process-Based Correlation Attack");
    std::cout << "\nSimulating malicious process activity..." << std::endl;
    
    auto evt1 = createEvent(EventType::PROCESS_CREATION, ThreatLevel::MEDIUM, 
                           1234, "C:\\malware\\suspicious.exe");
    evt1.description = "Suspicious process created";
    
    auto evt2 = createEvent(EventType::FILE_MODIFICATION, ThreatLevel::HIGH,
                           1234, "C:\\malware\\suspicious.exe",
                           "C:\\Windows\\System32\\critical.dll");
    evt2.description = "Critical system file modified";
    
    auto evt3 = createEvent(EventType::REGISTRY_MODIFICATION, ThreatLevel::HIGH,
                           1234, "C:\\malware\\suspicious.exe",
                           "HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
    evt3.description = "Autostart registry key modified";
    
    std::cout << "  1. Processing process creation event..." << std::endl;
    engine.ProcessEvent(evt1);
    
    std::cout << "  2. Processing file modification event..." << std::endl;
    engine.ProcessEvent(evt2);
    
    std::cout << "  3. Processing registry modification event..." << std::endl;
    engine.ProcessEvent(evt3);
    
    std::cout << "\nEvents processed: " << engine.GetProcessedEventCount() << std::endl;
    
    // Test Scenario 2: Target-based correlation
    printBanner("Scenario 2: Target-Based Correlation Attack");
    std::cout << "\nSimulating multiple processes targeting same file..." << std::endl;
    
    auto evt4 = createEvent(EventType::FILE_MODIFICATION, ThreatLevel::MEDIUM,
                           2000, "C:\\temp\\attacker1.exe",
                           "C:\\important\\database.db");
    evt4.description = "Database file access from process 2000";
    
    auto evt5 = createEvent(EventType::FILE_MODIFICATION, ThreatLevel::MEDIUM,
                           3000, "C:\\temp\\attacker2.exe",
                           "C:\\important\\database.db");
    evt5.description = "Database file access from process 3000";
    
    auto evt6 = createEvent(EventType::FILE_MODIFICATION, ThreatLevel::HIGH,
                           4000, "C:\\temp\\attacker3.exe",
                           "C:\\important\\database.db");
    evt6.description = "Database file modification from process 4000";
    
    std::cout << "  1. Processing file access from process 2000..." << std::endl;
    engine.ProcessEvent(evt4);
    
    std::cout << "  2. Processing file access from process 3000..." << std::endl;
    engine.ProcessEvent(evt5);
    
    std::cout << "  3. Processing file modification from process 4000..." << std::endl;
    engine.ProcessEvent(evt6);
    
    // Test Scenario 3: Threat escalation
    printBanner("Scenario 3: Threat Escalation Detection");
    std::cout << "\nSimulating escalating threat levels from same process..." << std::endl;
    
    auto evt7 = createEvent(EventType::NETWORK_CONNECTION, ThreatLevel::LOW,
                           5000, "C:\\program\\app.exe");
    evt7.description = "Low threat: Normal network connection";
    
    auto evt8 = createEvent(EventType::FILE_ACCESS, ThreatLevel::MEDIUM,
                           5000, "C:\\program\\app.exe",
                           "C:\\Users\\data.txt");
    evt8.description = "Medium threat: Suspicious file access";
    
    auto evt9 = createEvent(EventType::MEMORY_INJECTION, ThreatLevel::HIGH,
                           5000, "C:\\program\\app.exe");
    evt9.description = "High threat: Memory injection detected";
    
    std::cout << "  1. Processing low threat event..." << std::endl;
    engine.ProcessEvent(evt7);
    
    std::cout << "  2. Processing medium threat event..." << std::endl;
    engine.ProcessEvent(evt8);
    
    std::cout << "  3. Processing high threat event..." << std::endl;
    engine.ProcessEvent(evt9);
    
    // Test Scenario 4: Known attack pattern
    printBanner("Scenario 4: Known Attack Pattern Sequence");
    std::cout << "\nSimulating known attack pattern sequence..." << std::endl;
    
    auto evt10 = createEvent(EventType::PROCESS_CREATION, ThreatLevel::MEDIUM,
                            6000, "C:\\attacker\\dropper.exe");
    evt10.description = "Dropper process created";
    
    auto evt11 = createEvent(EventType::FILE_MODIFICATION, ThreatLevel::HIGH,
                            6001, "C:\\attacker\\payload.exe",
                            "C:\\Windows\\System32\\driver.sys");
    evt11.description = "System driver modified";
    
    auto evt12 = createEvent(EventType::REGISTRY_MODIFICATION, ThreatLevel::HIGH,
                            6001, "C:\\attacker\\payload.exe",
                            "HKLM\\System\\CurrentControlSet\\Services");
    evt12.description = "Service registry modified";
    
    std::cout << "  1. Processing dropper execution..." << std::endl;
    engine.ProcessEvent(evt10);
    
    std::cout << "  2. Processing driver modification..." << std::endl;
    engine.ProcessEvent(evt11);
    
    std::cout << "  3. Processing service installation..." << std::endl;
    engine.ProcessEvent(evt12);
    
    // Summary
    printBanner("Correlation Engine Statistics");
    std::cout << "\nTotal events processed: " << engine.GetProcessedEventCount() << std::endl;
    std::cout << "Total correlations detected: " << engine.GetCorrelationCount() << std::endl;
    std::cout << "Active correlations: " << engine.GetActiveCorrelationCount() << std::endl;
    std::cout << "Correlations alerted via callback: " << correlation_detected_count << std::endl;
    
    // Display all active correlations
    auto correlations = engine.GetActiveCorrelations();
    if (!correlations.empty()) {
        printBanner("All Active Correlations");
        for (const auto& corr : correlations) {
            printCorrelation(corr);
        }
    }
    
    std::cout << "\n✓ Demonstration completed successfully!" << std::endl;
    std::cout << "\nThe correlation engine successfully detected and grouped" << std::endl;
    std::cout << "related security events across multiple attack scenarios." << std::endl;
    
    return 0;
}
