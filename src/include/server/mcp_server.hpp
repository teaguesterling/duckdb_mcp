#pragma once

#include "duckdb.hpp"
#include "protocol/mcp_transport.hpp"
#include "protocol/mcp_message.hpp"
#include "duckdb/main/database.hpp"
#include <atomic>
#ifndef __EMSCRIPTEN__
#include <thread>
#endif
#include <unordered_map>
#include <ctime>

namespace duckdb {

// Forward declarations
class ResourceProvider;
class ToolHandler;
#ifndef __EMSCRIPTEN__
class HTTPServerTransport;
#else
class WebMCPTransport;
#endif

// MCP Server configuration
struct MCPServerConfig {
	string transport = "stdio";        // "stdio", "http", "https", "memory", "webmcp" (WASM only)
	string bind_address = "localhost"; // For HTTP/HTTPS
	int port = 8080;                   // For HTTP/HTTPS

	// HTTP/HTTPS specific configuration
	string auth_token;    // Optional: Bearer token for HTTP auth
	string ssl_cert_path; // For HTTPS: path to certificate file
	string ssl_key_path;  // For HTTPS: path to private key file

	// Built-in tool configuration
	bool enable_query_tool = true;         // Execute SQL queries (SELECT)
	bool enable_describe_tool = true;      // Describe tables/queries
	bool enable_export_tool = true;        // Export query results
	bool enable_list_tables_tool = true;   // List tables and views
	bool enable_database_info_tool = true; // Database overview info
	bool enable_execute_tool = false;      // Execute DDL/DML (disabled by default for safety)

	// Execute tool granular control
	bool execute_allow_ddl = true; // Allow CREATE, DROP, ALTER, etc.
	bool execute_allow_dml = true; // Allow INSERT, UPDATE, DELETE

	// Dangerous DDL subcategories (all default false for safety)
	bool execute_allow_load = false;   // LOAD, UPDATE_EXTENSIONS
	bool execute_allow_attach = false; // ATTACH, DETACH, COPY_DATABASE
	bool execute_allow_set = false;    // SET, VARIABLE_SET, PRAGMA

	// CORS configuration
	string cors_origins = "*";               // CORS origins: empty=disabled, "*"=wildcard, or comma-separated origins

	// Health endpoint configuration
	bool enable_health_endpoint = true;      // Enable /health endpoint
	bool auth_health_endpoint = false;       // Require auth for /health endpoint

	vector<string> allowed_queries;          // SQL query allowlist (empty = all allowed)
	vector<string> denied_queries;           // SQL query denylist
	string default_result_format = "json";   // Default format for query results ("json", "markdown", "csv")
	uint32_t max_connections = 10;           // Maximum concurrent connections
	uint32_t request_timeout_seconds = 30;   // Request timeout
	uint32_t max_requests = 0;               // Maximum requests before shutdown (0 = unlimited)
	bool background = false;                 // Run server in background thread (for testing)
	bool require_auth = false;               // Authentication required

	// Direct request gating (mcp_server_send_request)
	bool allow_direct_requests = true;       // Allow SQL function to bypass HTTP auth
	bool allow_direct_requests_explicit = false; // Whether the user explicitly set this

	DatabaseInstance *db_instance = nullptr; // DuckDB instance
};

// Resource registry for published resources
class ResourceRegistry {
public:
	void RegisterResource(const string &uri, shared_ptr<ResourceProvider> provider);
	void UnregisterResource(const string &uri);
	vector<string> ListResources() const;
	shared_ptr<ResourceProvider> GetResource(const string &uri) const;
	bool ResourceExists(const string &uri) const;

private:
	mutable mutex registry_mutex;
	unordered_map<string, shared_ptr<ResourceProvider>> resources;
};

// Tool registry for custom tools
class ToolRegistry {
public:
	void RegisterTool(const string &name, shared_ptr<ToolHandler> handler);
	void UnregisterTool(const string &name);
	vector<string> ListTools() const;
	shared_ptr<ToolHandler> GetTool(const string &name) const;
	bool ToolExists(const string &name) const;

private:
	mutable mutex registry_mutex;
	unordered_map<string, shared_ptr<ToolHandler>> tools;
};

// Main MCP Server class
class MCPServer {
public:
	MCPServer(const MCPServerConfig &config);
	~MCPServer();

	// Server lifecycle
	bool Start();           // Start in background mode (spawns thread)
	bool StartForeground(); // Start in foreground mode (no thread, caller uses RunMainLoop)
	void Stop();
	bool IsRunning() const {
		return running.load();
	}

	// Server information
	string GetStatus() const;
	uint32_t GetConnectionCount() const {
		return active_connections.load();
	}
	time_t GetUptime() const;
	uint64_t GetRequestsReceived() const {
		return requests_received.load();
	}
	uint64_t GetResponsesSent() const {
		return responses_sent.load();
	}
	uint64_t GetErrorsReturned() const {
		return errors_returned.load();
	}

	// Resource management
	bool PublishResource(const string &uri, shared_ptr<ResourceProvider> provider);
	bool UnpublishResource(const string &uri);
	vector<string> ListPublishedResources() const;

	// Tool management
	bool RegisterTool(const string &name, shared_ptr<ToolHandler> handler);
	bool UnregisterTool(const string &name);
	vector<string> ListRegisteredTools() const;

#ifndef __EMSCRIPTEN__
	// Main loop for stdio mode (blocks until process ends)
	void RunMainLoop();

	// Main loop for HTTP mode (blocks until Stop() is called)
	bool RunHTTPLoop();
#endif

	// Check if direct requests (via mcp_server_send_request SQL function) are allowed
	bool AllowsDirectRequests() const {
		// If user explicitly set allow_direct_requests, honor it
		if (config.allow_direct_requests_explicit) {
			return config.allow_direct_requests;
		}
		// Auto-disable when auth is required (prevents auth bypass via SQL)
		if (config.require_auth) {
			return false;
		}
		return config.allow_direct_requests;
	}

	// Testing support: process a single request directly (no transport)
	MCPMessage ProcessRequest(const MCPMessage &request);

	// Testing support: set a custom transport (for memory/mock transports)
	void SetTransport(unique_ptr<MCPTransport> transport);

	// Testing support: process exactly one message from transport and return
	// Returns true if a message was processed, false if connection closed
	bool ProcessOneMessage();

private:
	MCPServerConfig config;
	atomic<bool> running;
	atomic<uint32_t> active_connections;
	atomic<uint64_t> requests_received;
	atomic<uint64_t> responses_sent;
	atomic<uint64_t> errors_returned;
	atomic<time_t> start_time;

	ResourceRegistry resource_registry;
	ToolRegistry tool_registry;

#ifndef __EMSCRIPTEN__
	unique_ptr<std::thread> server_thread;
	unique_ptr<HTTPServerTransport> http_server; // For HTTP/HTTPS transport
#else
	unique_ptr<WebMCPTransport> webmcp_transport; // For WebMCP browser transport (WASM only)
#endif
	unique_ptr<MCPTransport> test_transport;     // For testing with custom transports

	// Request handling
#ifndef __EMSCRIPTEN__
	void ServerLoop();
#endif
	void HandleConnection(unique_ptr<MCPTransport> transport);
	MCPMessage HandleRequest(const MCPMessage &request);
	void HandleNotification(const MCPMessage &request);

	// Protocol handlers
	MCPMessage HandleInitialize(const MCPMessage &request);
	MCPMessage HandleResourcesList(const MCPMessage &request);
	MCPMessage HandleResourcesRead(const MCPMessage &request);
	MCPMessage HandleToolsList(const MCPMessage &request);
	MCPMessage HandleToolsCall(const MCPMessage &request);
	MCPMessage HandleShutdown(const MCPMessage &request);

	// Built-in tool registration
	void RegisterBuiltinTools();

	// Security and validation
	bool IsQueryAllowed(const string &query) const;
	bool ValidateAuthentication(const MCPMessage &request) const;

	// Error handling
	MCPMessage CreateErrorResponse(const Value &id, int code, const string &message) const;
};

// Pending registration for tools (before server starts)
struct PendingToolRegistration {
	string name;
	string description;
	string sql_template;
	string properties_json;
	string required_json;
	string format;
	DatabaseInstance *db_instance;
};

// Pending registration for resources (before server starts)
struct PendingResourceRegistration {
	string uri;
	string type;   // "table", "query", or "resource"
	string source; // table name, query, or content
	string format;
	string mime_type;
	string description; // for static resources
	uint32_t refresh_seconds;
	DatabaseInstance *db_instance;
};

// Global server instance management
class MCPServerManager {
public:
	static MCPServerManager &GetInstance();

	bool StartServer(const MCPServerConfig &config);
	void StopServer();
	bool IsServerRunning() const;
	MCPServer *GetServer() const {
		return server.get();
	}

	// Send request to running server (for testing with memory transport)
	MCPMessage SendRequest(const MCPMessage &request);

	// Queue registrations for when server starts
	void QueueToolRegistration(PendingToolRegistration registration);
	void QueueResourceRegistration(PendingResourceRegistration registration);

	// Get pending registration counts (for status/debugging)
	size_t GetPendingToolCount() const;
	size_t GetPendingResourceCount() const;

	// Apply pending registrations to an external server (for foreground mode)
	void ApplyPendingRegistrationsTo(MCPServer *external_server);

private:
	unique_ptr<MCPServer> server;
	mutable mutex manager_mutex;

	// Pending registrations (applied when server starts)
	vector<PendingToolRegistration> pending_tools;
	vector<PendingResourceRegistration> pending_resources;

	// Apply pending registrations to server
	void ApplyPendingRegistrations();
};

} // namespace duckdb
