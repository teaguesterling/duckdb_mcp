#pragma once

#ifdef __EMSCRIPTEN__

#include "duckdb.hpp"
#include <vector>
#include <string>

namespace duckdb {

class MCPServer;

struct WebMCPConfig {
	bool wrap_resources = true;  // Expose resources as a "read_resource" tool
	bool wrap_prompts = true;    // Expose prompts as a "get_prompt" tool
};

//! WebMCP transport bridges DuckDB-WASM MCP tools to the browser's
//! navigator.modelContext API (W3C WebMCP draft).
//!
//! This is NOT a subclass of MCPTransport. WebMCP is callback-driven:
//! the browser calls our execute handler, we call ProcessRequest()
//! synchronously and return. It's closer to the memory transport pattern
//! but event-driven from the browser side.
class WebMCPTransport {
public:
	WebMCPTransport(MCPServer *server, const WebMCPConfig &config);
	~WebMCPTransport();

	//! Check if navigator.modelContext is available
	static bool IsAvailable();

	//! Discover MCP tools/resources/prompts and register with navigator.modelContext
	bool Activate();

	//! Unregister all tools and clearContext()
	void Deactivate();

	//! Whether the transport is active
	bool IsActive() const;

	//! Re-scan ToolRegistry, diff & update WebMCP registrations
	void SyncTools();

	//! Called from JS execute callback. Routes to MCPServer::ProcessRequest().
	//! Returns JSON string with the tool result content.
	string HandleToolCall(const string &tool_name, const string &arguments_json);

	//! List tools registered by other scripts on the page (via interceptor)
	//! Returns JSON array string
	static string ListPageTools();

	MCPServer *GetServer() const { return server; }

private:
	MCPServer *server;
	WebMCPConfig config;
	bool active = false;
	vector<string> registered_tool_names;

	//! Register a single tool with navigator.modelContext
	bool RegisterWebMCPTool(const string &name, const string &description,
	                         const string &schema_json, bool read_only);

	//! Unregister a single tool from navigator.modelContext
	void UnregisterWebMCPTool(const string &name);

	//! Register wrapper tools for resources and prompts
	void RegisterResourceWrapper();
	void RegisterPromptWrapper();
};

//! Global WebMCP transport instance (singleton, like MCPServerManager)
//! Needed so the extern "C" callback can find the transport
WebMCPTransport *GetActiveWebMCPTransport();
void SetActiveWebMCPTransport(WebMCPTransport *transport);

} // namespace duckdb

#endif // __EMSCRIPTEN__
