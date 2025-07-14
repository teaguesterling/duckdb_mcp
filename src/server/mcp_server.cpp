#include "server/mcp_server.hpp"
#include "server/resource_providers.hpp"
#include "server/tool_handlers.hpp"
#include "server/stdio_server_transport.hpp"
#include "protocol/mcp_transport.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb_mcp_security.hpp"
#include <ctime>

namespace duckdb {

//===--------------------------------------------------------------------===//
// ResourceRegistry Implementation
//===--------------------------------------------------------------------===//

void ResourceRegistry::RegisterResource(const string &uri, unique_ptr<ResourceProvider> provider) {
    lock_guard<mutex> lock(registry_mutex);
    resources[uri] = std::move(provider);
}

void ResourceRegistry::UnregisterResource(const string &uri) {
    lock_guard<mutex> lock(registry_mutex);
    resources.erase(uri);
}

vector<string> ResourceRegistry::ListResources() const {
    lock_guard<mutex> lock(registry_mutex);
    vector<string> uris;
    for (const auto &pair : resources) {
        uris.push_back(pair.first);
    }
    return uris;
}

ResourceProvider* ResourceRegistry::GetResource(const string &uri) const {
    lock_guard<mutex> lock(registry_mutex);
    auto it = resources.find(uri);
    return it != resources.end() ? it->second.get() : nullptr;
}

bool ResourceRegistry::ResourceExists(const string &uri) const {
    lock_guard<mutex> lock(registry_mutex);
    return resources.find(uri) != resources.end();
}

//===--------------------------------------------------------------------===//
// ToolRegistry Implementation
//===--------------------------------------------------------------------===//

void ToolRegistry::RegisterTool(const string &name, unique_ptr<ToolHandler> handler) {
    lock_guard<mutex> lock(registry_mutex);
    tools[name] = std::move(handler);
}

void ToolRegistry::UnregisterTool(const string &name) {
    lock_guard<mutex> lock(registry_mutex);
    tools.erase(name);
}

vector<string> ToolRegistry::ListTools() const {
    lock_guard<mutex> lock(registry_mutex);
    vector<string> names;
    for (const auto &pair : tools) {
        names.push_back(pair.first);
    }
    return names;
}

ToolHandler* ToolRegistry::GetTool(const string &name) const {
    lock_guard<mutex> lock(registry_mutex);
    auto it = tools.find(name);
    return it != tools.end() ? it->second.get() : nullptr;
}

bool ToolRegistry::ToolExists(const string &name) const {
    lock_guard<mutex> lock(registry_mutex);
    return tools.find(name) != tools.end();
}

//===--------------------------------------------------------------------===//
// MCPServer Implementation
//===--------------------------------------------------------------------===//

MCPServer::MCPServer(const MCPServerConfig &config) 
    : config(config), running(false), active_connections(0), requests_served(0) {
    start_time = time(nullptr);
}

MCPServer::~MCPServer() {
    Stop();
}

bool MCPServer::Start() {
    if (running.load()) {
        return true; // Already running
    }
    
    if (!config.db_instance) {
        return false; // No database instance provided
    }
    
    // Check if serving is disabled
    auto &security = MCPSecurityConfig::GetInstance();
    if (security.IsServingDisabled()) {
        return false; // Server functionality is disabled
    }
    
    // Register built-in tools
    RegisterBuiltinTools();
    
    running = true;
    start_time = time(nullptr);
    
    // For stdio transport in background mode, start server thread
    if (config.transport == "stdio") {
        // For background mode, start thread. For foreground mode, caller will use RunMainLoop()
        server_thread = make_uniq<std::thread>(&MCPServer::ServerLoop, this);
        return true;
    } else {
        // TCP/WebSocket not implemented yet
        running = false;
        return false;
    }
}

void MCPServer::Stop() {
    if (!running.load()) {
        return;
    }
    
    running = false;
    
    if (server_thread && server_thread->joinable()) {
        server_thread->join();
    }
    
    server_thread.reset();
}

string MCPServer::GetStatus() const {
    if (!running.load()) {
        return "STOPPED";
    }
    
    string status = "RUNNING\n";
    status += "Transport: " + config.transport + "\n";
    status += "Connections: " + std::to_string(active_connections.load()) + "\n";
    status += "Requests Served: " + std::to_string(requests_served.load()) + "\n";
    status += "Uptime: " + std::to_string(GetUptime()) + " seconds\n";
    status += "Resources: " + std::to_string(resource_registry.ListResources().size()) + "\n";
    status += "Tools: " + std::to_string(tool_registry.ListTools().size());
    
    return status;
}

time_t MCPServer::GetUptime() const {
    if (!running.load()) {
        return 0;
    }
    return time(nullptr) - start_time;
}

bool MCPServer::PublishResource(const string &uri, unique_ptr<ResourceProvider> provider) {
    resource_registry.RegisterResource(uri, std::move(provider));
    return true;
}

bool MCPServer::UnpublishResource(const string &uri) {
    resource_registry.UnregisterResource(uri);
    return true;
}

vector<string> MCPServer::ListPublishedResources() const {
    return resource_registry.ListResources();
}

bool MCPServer::RegisterTool(const string &name, unique_ptr<ToolHandler> handler) {
    tool_registry.RegisterTool(name, std::move(handler));
    return true;
}

bool MCPServer::UnregisterTool(const string &name) {
    tool_registry.UnregisterTool(name);
    return true;
}

vector<string> MCPServer::ListRegisteredTools() const {
    return tool_registry.ListTools();
}

void MCPServer::RunMainLoop() {
    if (config.transport != "stdio") {
        throw InvalidInputException("RunMainLoop() is only supported for stdio transport");
    }
    
    if (!running.load()) {
        throw InvalidInputException("Server must be started before calling RunMainLoop()");
    }
    
    // Create server-side file descriptor transport using stdin/stdout
    auto transport = make_uniq<FdServerTransport>();
    
    // Connect and handle the connection in main thread (blocks forever)
    if (transport->Connect()) {
        HandleConnection(std::move(transport));
    }
}

void MCPServer::ServerLoop() {
    if (config.transport == "stdio") {
        // Create server-side file descriptor transport using stdin/stdout
        auto transport = make_uniq<FdServerTransport>();
        
        // Connect and handle the connection
        if (transport->Connect()) {
            HandleConnection(std::move(transport));
        }
    }
}

void MCPServer::HandleConnection(unique_ptr<MCPTransport> transport) {
    active_connections.fetch_add(1);
    
    try {
        while (running.load()) {
            try {
                auto request = transport->Receive();
                auto response = HandleRequest(request);
                transport->Send(response);
                requests_served.fetch_add(1);
                
                // If this was a shutdown request, break out of the loop
                if (request.method == MCPMethods::SHUTDOWN) {
                    break;
                }
            } catch (const std::exception &e) {
                // Log error and continue
                break;
            }
        }
    } catch (...) {
        // Connection error
    }
    
    active_connections.fetch_sub(1);
}

MCPMessage MCPServer::HandleRequest(const MCPMessage &request) {
    try {
        // Validate authentication if required
        if (config.require_auth && !ValidateAuthentication(request)) {
            return CreateErrorResponse(request.id, -32001, "Authentication required");
        }
        
        // Route request based on method
        if (request.method == MCPMethods::INITIALIZE) {
            return HandleInitialize(request);
        } else if (request.method == MCPMethods::RESOURCES_LIST) {
            return HandleResourcesList(request);
        } else if (request.method == MCPMethods::RESOURCES_READ) {
            return HandleResourcesRead(request);
        } else if (request.method == MCPMethods::TOOLS_LIST) {
            return HandleToolsList(request);
        } else if (request.method == MCPMethods::TOOLS_CALL) {
            return HandleToolsCall(request);
        } else if (request.method == MCPMethods::SHUTDOWN) {
            return HandleShutdown(request);
        } else {
            return CreateErrorResponse(request.id, MCPErrorCodes::METHOD_NOT_FOUND, 
                                     "Method not found: " + request.method);
        }
    } catch (const std::exception &e) {
        return CreateErrorResponse(request.id, MCPErrorCodes::INTERNAL_ERROR, 
                                 "Internal error: " + string(e.what()));
    }
}

MCPMessage MCPServer::HandleInitialize(const MCPMessage &request) {
    // Return server capabilities
    Value capabilities = Value::STRUCT({
        {"resources", Value::BOOLEAN(true)},
        {"tools", Value::BOOLEAN(true)},
        {"prompts", Value::BOOLEAN(false)}, // Not implemented yet
        {"sampling", Value::BOOLEAN(false)} // Not implemented yet
    });
    
    Value server_info = Value::STRUCT({
        {"name", Value("DuckDB MCP Server")},
        {"version", Value("1.0.0")},
        {"capabilities", capabilities}
    });
    
    return MCPMessage::CreateResponse(server_info, request.id);
}

MCPMessage MCPServer::HandleResourcesList(const MCPMessage &request) {
    auto resource_uris = resource_registry.ListResources();
    
    vector<Value> resources;
    for (const auto &uri : resource_uris) {
        auto provider = resource_registry.GetResource(uri);
        if (provider) {
            Value resource = Value::STRUCT({
                {"uri", Value(uri)},
                {"name", Value(uri)}, // Use URI as name for now
                {"description", Value(provider->GetDescription())},
                {"mimeType", Value(provider->GetMimeType())}
            });
            resources.push_back(resource);
        }
    }
    
    Value result = Value::STRUCT({
        {"resources", Value::LIST(LogicalType::STRUCT({}), resources)}
    });
    
    return MCPMessage::CreateResponse(result, request.id);
}

MCPMessage MCPServer::HandleResourcesRead(const MCPMessage &request) {
    // Extract URI from parameters
    if (request.params.type().id() != LogicalTypeId::STRUCT) {
        return CreateErrorResponse(request.id, MCPErrorCodes::INVALID_PARAMS, "Invalid parameters");
    }
    
    auto &struct_values = StructValue::GetChildren(request.params);
    string uri;
    
    for (size_t i = 0; i < struct_values.size(); i++) {
        auto &key = StructType::GetChildName(request.params.type(), i);
        if (key == "uri") {
            uri = struct_values[i].ToString();
            break;
        }
    }
    
    if (uri.empty()) {
        return CreateErrorResponse(request.id, MCPErrorCodes::INVALID_PARAMS, "Missing uri parameter");
    }
    
    auto provider = resource_registry.GetResource(uri);
    if (!provider) {
        return CreateErrorResponse(request.id, MCPErrorCodes::RESOURCE_NOT_FOUND, "Resource not found: " + uri);
    }
    
    auto read_result = provider->Read();
    if (!read_result.success) {
        return CreateErrorResponse(request.id, MCPErrorCodes::INTERNAL_ERROR, read_result.error_message);
    }
    
    Value result = Value::STRUCT({
        {"contents", Value::LIST(LogicalType::STRUCT({}), {
            Value::STRUCT({
                {"uri", Value(uri)},
                {"mimeType", Value(read_result.mime_type)},
                {"text", Value(read_result.content)}
            })
        })}
    });
    
    return MCPMessage::CreateResponse(result, request.id);
}

MCPMessage MCPServer::HandleToolsList(const MCPMessage &request) {
    auto tool_names = tool_registry.ListTools();
    
    vector<Value> tools;
    for (const auto &name : tool_names) {
        auto handler = tool_registry.GetTool(name);
        if (handler) {
            Value tool = Value::STRUCT({
                {"name", Value(name)},
                {"description", Value(handler->GetDescription())},
                {"inputSchema", handler->GetInputSchema().ToJSON()}
            });
            tools.push_back(tool);
        }
    }
    
    Value result = Value::STRUCT({
        {"tools", Value::LIST(LogicalType::STRUCT({}), tools)}
    });
    
    return MCPMessage::CreateResponse(result, request.id);
}

MCPMessage MCPServer::HandleToolsCall(const MCPMessage &request) {
    // Extract tool name and arguments from parameters
    if (request.params.type().id() != LogicalTypeId::STRUCT) {
        return CreateErrorResponse(request.id, MCPErrorCodes::INVALID_PARAMS, "Invalid parameters");
    }
    
    auto &struct_values = StructValue::GetChildren(request.params);
    string tool_name;
    Value arguments;
    
    for (size_t i = 0; i < struct_values.size(); i++) {
        auto &key = StructType::GetChildName(request.params.type(), i);
        if (key == "name") {
            tool_name = struct_values[i].ToString();
        } else if (key == "arguments") {
            arguments = struct_values[i];
        }
    }
    
    if (tool_name.empty()) {
        return CreateErrorResponse(request.id, MCPErrorCodes::INVALID_PARAMS, "Missing tool name");
    }
    
    auto handler = tool_registry.GetTool(tool_name);
    if (!handler) {
        return CreateErrorResponse(request.id, MCPErrorCodes::TOOL_NOT_FOUND, "Tool not found: " + tool_name);
    }
    
    auto call_result = handler->Execute(arguments);
    if (!call_result.success) {
        return CreateErrorResponse(request.id, MCPErrorCodes::INVALID_TOOL_INPUT, call_result.error_message);
    }
    
    Value result = Value::STRUCT({
        {"content", Value::LIST(LogicalType::STRUCT({}), {
            Value::STRUCT({
                {"type", Value("text")},
                {"text", call_result.result}
            })
        })}
    });
    
    return MCPMessage::CreateResponse(result, request.id);
}

MCPMessage MCPServer::HandleShutdown(const MCPMessage &request) {
    // Gracefully shut down the server
    running = false;
    
    Value result = Value::STRUCT({
        {"status", Value("shutting down")},
        {"message", Value("Server shutdown initiated")}
    });
    
    return MCPMessage::CreateResponse(result, request.id);
}

void MCPServer::RegisterBuiltinTools() {
    if (config.enable_query_tool) {
        auto query_tool = make_uniq<QueryToolHandler>(*config.db_instance, 
                                                       config.allowed_queries,
                                                       config.denied_queries);
        tool_registry.RegisterTool("query", std::move(query_tool));
    }
    
    if (config.enable_describe_tool) {
        auto describe_tool = make_uniq<DescribeToolHandler>(*config.db_instance);
        tool_registry.RegisterTool("describe", std::move(describe_tool));
    }
    
    if (config.enable_export_tool) {
        auto export_tool = make_uniq<ExportToolHandler>(*config.db_instance);
        tool_registry.RegisterTool("export", std::move(export_tool));
    }
}

bool MCPServer::IsQueryAllowed(const string &query) const {
    // If no allowlist, everything is allowed
    if (config.allowed_queries.empty() && config.denied_queries.empty()) {
        return true;
    }
    
    // Check denylist first
    for (const auto &denied : config.denied_queries) {
        if (query.find(denied) != string::npos) {
            return false;
        }
    }
    
    // Check allowlist if it exists
    if (!config.allowed_queries.empty()) {
        for (const auto &allowed : config.allowed_queries) {
            if (query.find(allowed) != string::npos) {
                return true;
            }
        }
        return false; // Not in allowlist
    }
    
    return true; // No restrictions
}

bool MCPServer::ValidateAuthentication(const MCPMessage &request) const {
    if (!config.require_auth) {
        return true;
    }
    
    // TODO: Implement proper authentication validation
    // For now, just check if auth_token is provided in the message
    return !config.auth_token.empty();
}

MCPMessage MCPServer::CreateErrorResponse(const Value &id, int code, const string &message) const {
    return MCPMessage::CreateError(code, message, id);
}

//===--------------------------------------------------------------------===//
// MCPServerManager Implementation
//===--------------------------------------------------------------------===//

MCPServerManager& MCPServerManager::GetInstance() {
    static MCPServerManager instance;
    return instance;
}

bool MCPServerManager::StartServer(const MCPServerConfig &config) {
    lock_guard<mutex> lock(manager_mutex);
    
    if (server && server->IsRunning()) {
        return false; // Server already running
    }
    
    server = make_uniq<MCPServer>(config);
    return server->Start();
}

void MCPServerManager::StopServer() {
    lock_guard<mutex> lock(manager_mutex);
    
    if (server) {
        server->Stop();
        server.reset();
    }
}

bool MCPServerManager::IsServerRunning() const {
    lock_guard<mutex> lock(manager_mutex);
    return server && server->IsRunning();
}

} // namespace duckdb