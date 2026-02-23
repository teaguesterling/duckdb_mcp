#pragma once

#include "duckdb.hpp"
#include <future>

namespace duckdb {

// Forward declarations
struct MCPMessage;

// Abstract transport interface for MCP communication
class MCPTransport {
public:
	virtual ~MCPTransport() = default;

	// Connection lifecycle
	virtual bool Connect() = 0;
	virtual void Disconnect() = 0;
	virtual bool IsConnected() const = 0;

	// Message passing
	virtual void Send(const MCPMessage &message) = 0;
	virtual MCPMessage Receive() = 0;
	virtual MCPMessage SendAndReceive(const MCPMessage &message) = 0;

	// Connection health
	virtual bool Ping() = 0;
	virtual string GetConnectionInfo() const = 0;
};

#ifndef __EMSCRIPTEN__

// Process-based stdio transport implementation
class StdioTransport : public MCPTransport {
public:
	struct StdioConfig {
		string command_path;
		vector<string> arguments;
		string working_directory;
		unordered_map<string, string> environment;
		int timeout_seconds = 30;
	};

	explicit StdioTransport(const StdioConfig &config);
	~StdioTransport() override;

	// MCPTransport interface
	bool Connect() override;
	void Disconnect() override;
	bool IsConnected() const override;
	void Send(const MCPMessage &message) override;
	MCPMessage Receive() override;
	MCPMessage SendAndReceive(const MCPMessage &message) override;
	bool Ping() override;
	string GetConnectionInfo() const override;

private:
	StdioConfig config;
	bool connected;
	int process_pid;
	int stdin_fd;
	int stdout_fd;
	int stderr_fd;
	mutex io_mutex;

	// Process management
	bool StartProcess();
	void StopProcess();
	bool IsProcessRunning() const;

	// I/O operations
	void WriteToProcess(const string &data);
	string ReadFromProcess();
	bool WaitForData(int timeout_ms = 5000);
};

// TCP transport implementation (placeholder for Phase 2)
class TCPTransport : public MCPTransport {
public:
	struct TCPConfig {
		string host;
		int port;
		bool use_tls = false;
		string ca_cert_path;
		string client_cert_path;
		string client_key_path;
		int timeout_seconds = 30;
	};

	explicit TCPTransport(const TCPConfig &config);
	~TCPTransport() override = default;

	// MCPTransport interface (placeholder implementations)
	bool Connect() override;
	void Disconnect() override;
	bool IsConnected() const override;
	void Send(const MCPMessage &message) override;
	MCPMessage Receive() override;
	MCPMessage SendAndReceive(const MCPMessage &message) override;
	bool Ping() override;
	string GetConnectionInfo() const override;

private:
	TCPConfig config;
	bool connected = false;
};

#endif // !__EMSCRIPTEN__

} // namespace duckdb
