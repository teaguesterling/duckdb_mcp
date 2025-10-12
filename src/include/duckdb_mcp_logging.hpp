#pragma once

#include "duckdb.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/exception.hpp"
#include <chrono>
#include <mutex>
#include <fstream>

namespace duckdb {

enum class MCPLogLevel : uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    OFF = 5
};

class MCPLogger {
public:
    static MCPLogger& GetInstance();
    
    void SetLogLevel(MCPLogLevel level);
    MCPLogLevel GetLogLevel() const;
    
    void SetLogFile(const string &file_path);
    void EnableConsoleLogging(bool enable);
    
    template<typename... Args>
    void Log(MCPLogLevel level, const string &component, const string &format, Args&&... args) {
        if (level < current_level) {
            return;
        }
        
        string message;
        try {
            message = StringUtil::Format(format, std::forward<Args>(args)...);
        } catch (...) {
            message = format; // Fallback to raw format string
        }
        
        LogMessage(level, component, message);
    }
    
    void LogProtocolMessage(bool outgoing, const string &server, const string &json);
    void LogPerformanceMetric(const string &operation, double duration_ms, const string &details = "");
    
private:
    MCPLogger();
    ~MCPLogger();
    
    void LogMessage(MCPLogLevel level, const string &component, const string &message);
    string FormatLogEntry(MCPLogLevel level, const string &component, const string &message);
    string GetLevelString(MCPLogLevel level);
    string GetTimestamp();
    
    mutable std::mutex log_mutex;
    MCPLogLevel current_level = MCPLogLevel::WARN;
    bool console_logging = false;
    string log_file_path;
    std::ofstream log_file;
};

// Convenience macros
#define MCP_LOG_TRACE(component, format, ...) \
    MCPLogger::GetInstance().Log(MCPLogLevel::TRACE, component, format, ##__VA_ARGS__)

#define MCP_LOG_DEBUG(component, format, ...) \
    MCPLogger::GetInstance().Log(MCPLogLevel::DEBUG, component, format, ##__VA_ARGS__)

#define MCP_LOG_INFO(component, format, ...) \
    MCPLogger::GetInstance().Log(MCPLogLevel::INFO, component, format, ##__VA_ARGS__)

#define MCP_LOG_WARN(component, format, ...) \
    MCPLogger::GetInstance().Log(MCPLogLevel::WARN, component, format, ##__VA_ARGS__)

#define MCP_LOG_ERROR(component, format, ...) \
    MCPLogger::GetInstance().Log(MCPLogLevel::ERROR, component, format, ##__VA_ARGS__)

#define MCP_LOG_PROTOCOL(outgoing, server, json) \
    MCPLogger::GetInstance().LogProtocolMessage(outgoing, server, json)

#define MCP_LOG_PERF(operation, duration_ms, details) \
    MCPLogger::GetInstance().LogPerformanceMetric(operation, duration_ms, details)

// Performance timing helper
class MCPPerformanceTimer {
public:
    MCPPerformanceTimer(const string &operation, const string &details = "") 
        : operation_name(operation), operation_details(details) {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    ~MCPPerformanceTimer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double duration_ms = duration.count() / 1000.0;
        MCP_LOG_PERF(operation_name, duration_ms, operation_details);
    }
    
private:
    string operation_name;
    string operation_details;
    std::chrono::high_resolution_clock::time_point start_time;
};

#define MCP_PERF_TIMER(operation, details) \
    MCPPerformanceTimer _perf_timer(operation, details)

} // namespace duckdb