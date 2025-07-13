#pragma once

#include "duckdb.hpp"
#include "protocol/mcp_transport.hpp"
#include "protocol/mcp_message.hpp"
#include "duckdb/main/database.hpp"
#include <atomic>
#include <thread>
#include <unordered_map>
#include <ctime>

namespace duckdb {

// Forward declarations
class ResourceProvider;
class ToolHandler;

// MCP Server configuration
struct MCPServerConfig {
    string transport = "stdio";           // "stdio", "tcp", "websocket"
    string bind_address = "localhost";    // For TCP/WebSocket
    int port = 8080;                     // For TCP/WebSocket
    bool enable_query_tool = true;       // Built-in query tool
    bool enable_describe_tool = true;    // Built-in describe tool  
    bool enable_export_tool = true;      // Built-in export tool
    vector<string> allowed_queries;      // SQL query allowlist (empty = all allowed)
    vector<string> denied_queries;       // SQL query denylist
    uint32_t max_connections = 10;       // Maximum concurrent connections
    uint32_t request_timeout_seconds = 30; // Request timeout
    bool require_auth = false;           // Authentication required
    string auth_token;                   // Bearer token for authentication
    DatabaseInstance *db_instance = nullptr; // DuckDB instance
};

// Resource registry for published resources
class ResourceRegistry {
public:
    void RegisterResource(const string &uri, unique_ptr<ResourceProvider> provider);
    void UnregisterResource(const string &uri);
    vector<string> ListResources() const;
    ResourceProvider* GetResource(const string &uri) const;
    bool ResourceExists(const string &uri) const;

private:
    mutable mutex registry_mutex;
    unordered_map<string, unique_ptr<ResourceProvider>> resources;
};

// Tool registry for custom tools
class ToolRegistry {
public:
    void RegisterTool(const string &name, unique_ptr<ToolHandler> handler);
    void UnregisterTool(const string &name);
    vector<string> ListTools() const;
    ToolHandler* GetTool(const string &name) const;
    bool ToolExists(const string &name) const;

private:
    mutable mutex registry_mutex;
    unordered_map<string, unique_ptr<ToolHandler>> tools;
};

// Main MCP Server class
class MCPServer {
public:
    MCPServer(const MCPServerConfig &config);
    ~MCPServer();

    // Server lifecycle
    bool Start();
    void Stop();
    bool IsRunning() const { return running.load(); }
    
    // Server information
    string GetStatus() const;
    uint32_t GetConnectionCount() const { return active_connections.load(); }
    time_t GetUptime() const;
    uint64_t GetRequestsServed() const { return requests_served.load(); }
    
    // Resource management
    bool PublishResource(const string &uri, unique_ptr<ResourceProvider> provider);
    bool UnpublishResource(const string &uri);
    vector<string> ListPublishedResources() const;
    
    // Tool management
    bool RegisterTool(const string &name, unique_ptr<ToolHandler> handler);
    bool UnregisterTool(const string &name);
    vector<string> ListRegisteredTools() const;
    
    // Main loop for stdio mode (blocks until process ends)
    void RunMainLoop();

private:
    MCPServerConfig config;
    atomic<bool> running;
    atomic<uint32_t> active_connections;
    atomic<uint64_t> requests_served;
    time_t start_time;
    
    ResourceRegistry resource_registry;
    ToolRegistry tool_registry;
    
    unique_ptr<std::thread> server_thread;
    
    // Request handling
    void ServerLoop();
    void HandleConnection(unique_ptr<MCPTransport> transport);
    MCPMessage HandleRequest(const MCPMessage &request);
    
    // Protocol handlers
    MCPMessage HandleInitialize(const MCPMessage &request);
    MCPMessage HandleResourcesList(const MCPMessage &request);
    MCPMessage HandleResourcesRead(const MCPMessage &request);
    MCPMessage HandleToolsList(const MCPMessage &request);
    MCPMessage HandleToolsCall(const MCPMessage &request);
    
    // Built-in tool registration
    void RegisterBuiltinTools();
    
    // Security and validation
    bool IsQueryAllowed(const string &query) const;
    bool ValidateAuthentication(const MCPMessage &request) const;
    
    // Error handling
    MCPMessage CreateErrorResponse(const Value &id, int code, const string &message) const;
};

// Global server instance management
class MCPServerManager {
public:
    static MCPServerManager& GetInstance();
    
    bool StartServer(const MCPServerConfig &config);
    void StopServer();
    bool IsServerRunning() const;
    MCPServer* GetServer() const { return server.get(); }

private:
    unique_ptr<MCPServer> server;
    mutable mutex manager_mutex;
};

} // namespace duckdb