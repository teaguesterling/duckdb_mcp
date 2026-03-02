#pragma once

#include "duckdb.hpp"
#include "protocol/mcp_transport.hpp"
#include "protocol/mcp_message.hpp"
#include <atomic>

namespace duckdb {

#ifndef __EMSCRIPTEN__

// MCP server capabilities
struct MCPCapabilities {
	bool supports_resources = false;
	bool supports_tools = false;
	bool supports_prompts = false;
	bool supports_sampling = false;
	vector<string> resource_schemes;
	vector<string> tool_names;
	vector<string> prompt_names;
};

// MCP resource metadata
struct MCPResource {
	string uri;
	string name;
	string description;
	string mime_type;
	int64_t size = -1;
	time_t last_modified = 0;
	string etag;
	string content; // For small resources
	bool content_loaded = false;
};

// MCP connection state
enum class MCPConnectionState { DISCONNECTED, CONNECTING, CONNECTED, INITIALIZED, ERROR };

// Main MCP connection class
class MCPConnection {
public:
	MCPConnection(const string &server_name, unique_ptr<MCPTransport> transport);
	~MCPConnection();

	// Connection lifecycle
	bool Connect();
	void Disconnect();
	bool IsConnected() const;
	bool IsInitialized() const;
	MCPConnectionState GetState() const;

	// Initialization
	bool Initialize();
	const MCPCapabilities &GetCapabilities() const {
		return capabilities;
	}

	// Resource operations
	vector<MCPResource> ListResources(const string &cursor = "");
	MCPResource ReadResource(const string &uri);
	bool ResourceExists(const string &uri);

	// Tool operations (placeholder for Phase 2)
	vector<string> ListTools();
	Value CallTool(const string &name, const Value &arguments);

	// Connection info
	string GetServerName() const {
		return server_name;
	}
	string GetConnectionInfo() const;
	bool Ping();

	// Error handling
	string GetLastError() const;
	bool HasRecoverableError() const;
	void ClearError();

	// Connection health monitoring
	bool IsHealthy() const;
	time_t GetLastActivityTime() const {
		return last_activity_time;
	}
	uint32_t GetConsecutiveFailures() const {
		return consecutive_failures;
	}

	// Raw MCP protocol access
	MCPMessage SendRequest(const string &method, const Value &params);

private:
	string server_name;
	unique_ptr<MCPTransport> transport;
	atomic<MCPConnectionState> state;
	MCPCapabilities capabilities;
	atomic<uint64_t> next_request_id;
	mutable mutex error_mutex;
	string last_error;
	atomic<bool> is_recoverable_error;
	atomic<uint32_t> consecutive_failures;
	atomic<time_t> last_activity_time;
	mutable mutex connection_mutex;

	// Protocol helpers
	bool SendNotification(const string &method, const Value &params);
	Value GenerateRequestId();

	// Initialization helpers
	bool SendInitialize();
	bool WaitForInitialized();
	void ParseCapabilities(const Value &server_info);

	// Error handling
	void SetError(const string &error, bool recoverable = false);
	void HandleTransportError(const string &operation);
	void RecordSuccess();
	void RecordFailure();

	// Automatic retry logic
	bool SendRequestWithRetry(const string &method, const Value &params, MCPMessage &response, int max_retries = 3);
};

#endif // !__EMSCRIPTEN__

} // namespace duckdb
