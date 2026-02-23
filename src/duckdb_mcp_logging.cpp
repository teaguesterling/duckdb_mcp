#include "duckdb_mcp_logging.hpp"
#include <iostream>
#ifndef __EMSCRIPTEN__
#include <fstream>
#endif
#include <iomanip>
#include <sstream>

namespace duckdb {

MCPLogger &MCPLogger::GetInstance() {
	static MCPLogger instance;
	return instance;
}

MCPLogger::MCPLogger() {
	// Initialize with default settings
	current_level = MCPLogLevel::WARN;
	console_logging = false;
}

MCPLogger::~MCPLogger() {
	std::lock_guard<std::mutex> lock(log_mutex);
#ifndef __EMSCRIPTEN__
	if (log_file.is_open()) {
		log_file.close();
	}
#endif
}

void MCPLogger::SetLogLevel(MCPLogLevel level) {
	std::lock_guard<std::mutex> lock(log_mutex);
	current_level = level;
}

MCPLogLevel MCPLogger::GetLogLevel() const {
	std::lock_guard<std::mutex> lock(log_mutex);
	return current_level;
}

void MCPLogger::SetLogFile(const string &file_path) {
	std::lock_guard<std::mutex> lock(log_mutex);

#ifdef __EMSCRIPTEN__
	// File logging is not available in WASM — use console logging instead
	if (!file_path.empty()) {
		std::cerr << "[MCP-WARN] File logging not available in WASM, ignoring log file: " << file_path << std::endl;
	}
	return;
#else
	// Close existing file if open
	if (log_file.is_open()) {
		log_file.close();
	}

	if (!file_path.empty()) {
		log_file_path = file_path;
		log_file.open(file_path, std::ios::app);
		if (!log_file.is_open()) {
			// Log to console if file cannot be opened
			std::cerr << "[MCP-ERROR] Failed to open log file: " << file_path << std::endl;
		}
	} else {
		log_file_path.clear();
	}
#endif
}

void MCPLogger::EnableConsoleLogging(bool enable) {
	std::lock_guard<std::mutex> lock(log_mutex);
	console_logging = enable;
}

void MCPLogger::LogMessage(MCPLogLevel level, const string &component, const string &message) {
	std::lock_guard<std::mutex> lock(log_mutex);

	string formatted_message = FormatLogEntry(level, component, message);

	// Write to console if enabled
	if (console_logging) {
		if (level >= MCPLogLevel::ERROR) {
			std::cerr << formatted_message << std::endl;
		} else {
			std::cout << formatted_message << std::endl;
		}
	}

#ifndef __EMSCRIPTEN__
	// Write to file if available
	if (log_file.is_open()) {
		log_file << formatted_message << std::endl;
		log_file.flush();
	}
#endif
}

string MCPLogger::FormatLogEntry(MCPLogLevel level, const string &component, const string &message) {
	std::ostringstream oss;
	oss << GetTimestamp() << " [" << GetLevelString(level) << "] "
	    << "[" << component << "] " << message;
	return oss.str();
}

string MCPLogger::GetLevelString(MCPLogLevel level) {
	switch (level) {
	case MCPLogLevel::TRACE:
		return "TRACE";
	case MCPLogLevel::DEBUG:
		return "DEBUG";
	case MCPLogLevel::INFO:
		return "INFO ";
	case MCPLogLevel::WARN:
		return "WARN ";
	case MCPLogLevel::ERROR:
		return "ERROR";
	case MCPLogLevel::OFF:
		return "OFF  ";
	default:
		return "UNKNOWN";
	}
}

string MCPLogger::GetTimestamp() {
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::ostringstream oss;
	oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
	oss << "." << std::setfill('0') << std::setw(3) << ms.count();
	return oss.str();
}

void MCPLogger::LogProtocolMessage(bool outgoing, const string &server, const string &json) {
	if (current_level > MCPLogLevel::DEBUG) {
		return;
	}

	string direction = outgoing ? "SEND" : "RECV";
	string component = StringUtil::Format("MCP-PROTOCOL[%s]", server);

	// Truncate very long JSON messages for readability
	string display_json = json;
	if (display_json.length() > 500) {
		display_json = display_json.substr(0, 497) + "...";
	}

	string message = StringUtil::Format("%s: %s", direction, display_json);
	LogMessage(MCPLogLevel::DEBUG, component, message);
}

void MCPLogger::LogPerformanceMetric(const string &operation, double duration_ms, const string &details) {
	if (current_level > MCPLogLevel::INFO) {
		return;
	}

	string message;
	if (!details.empty()) {
		message = StringUtil::Format("PERF: %s completed in %.2fms (%s)", operation, duration_ms, details);
	} else {
		message = StringUtil::Format("PERF: %s completed in %.2fms", operation, duration_ms);
	}

	LogMessage(MCPLogLevel::INFO, "MCP-PERFORMANCE", message);
}

} // namespace duckdb
