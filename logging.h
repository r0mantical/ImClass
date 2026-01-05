#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <mutex>

namespace logger {
    struct LogEntry {
        std::string message;
        std::string timestamp;

        LogEntry(const std::string& msg) : message(msg) {
            auto now = std::time(nullptr);
            struct tm tm;
            localtime_s(&tm, &now);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            timestamp = oss.str();
        }

        std::string toString() const {
            std::ostringstream oss;
            oss << "[" << timestamp << "] " << message;
            return oss.str();
        }
    };

    inline std::vector<LogEntry> logs;
    inline std::mutex log_mutex;

    void addLog(const std::string& message) {
        std::lock_guard<std::mutex> lock(log_mutex);
        logs.push_back(LogEntry(message));
    }

    const std::vector<LogEntry>& getLogs() {
        return logs;
    }

    inline void clearLogs() {
        std::lock_guard<std::mutex> lock(log_mutex);
        logs.clear();
    }
}