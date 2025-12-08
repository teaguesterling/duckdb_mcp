#include "server/mcp_server.hpp"
#include "server/resource_providers.hpp"
#include "server/tool_handlers.hpp"
#include "server/stdio_server_transport.hpp"
#include "duckdb_mcp_extension.hpp"
#include "duckdb_mcp_logging.hpp"
#include "protocol/mcp_transport.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb_mcp_security.hpp"
#include "json_utils.hpp"
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
    : config(config), running(false), active_connections(0), requests_received(0), responses_sent(0) {
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

    // Handle different transport types
    if (config.transport == "stdio") {
        // For background mode, start thread. For foreground mode, caller will use RunMainLoop()
        server_thread = make_uniq<std::thread>(&MCPServer::ServerLoop, this);
        return true;
    } else if (config.transport == "memory") {
        // Memory transport: no I/O thread needed
        // Server just stays running and waits for ProcessRequest() calls
        return true;
    } else {
        // TCP/WebSocket not implemented yet
        running = false;
        return false;
    }
}

bool MCPServer::StartForeground() {
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

    // For foreground mode, don't start a thread - caller will use RunMainLoop()
    return true;
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
    status += "Requests Received: " + std::to_string(requests_received.load()) + "\n";
    status += "Responses Sent: " + std::to_string(responses_sent.load()) + "\n";
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
    
    // Create server-side stdio transport using std::cin/std::cout
    auto transport = make_uniq<FdServerTransport>();

    // Connect and handle the connection in main thread (blocks until shutdown/max_requests)
    if (transport->Connect()) {
        HandleConnection(std::move(transport));
    }
}

void MCPServer::ServerLoop() {
    if (config.transport == "stdio") {
        // Create server-side stdio transport using std::cin/std::cout
        auto transport = make_uniq<FdServerTransport>();

        // Connect and handle the connection
        if (transport->Connect()) {
            HandleConnection(std::move(transport));
        }
    }
}

MCPMessage MCPServer::ProcessRequest(const MCPMessage &request) {
    // Public wrapper for HandleRequest - allows direct testing without transport
    return HandleRequest(request);
}

void MCPServer::SetTransport(unique_ptr<MCPTransport> transport) {
    test_transport = std::move(transport);
}

bool MCPServer::ProcessOneMessage() {
    if (!test_transport || !test_transport->IsConnected()) {
        return false;
    }

    try {
        auto request = test_transport->Receive();
        requests_received.fetch_add(1);
        auto response = HandleRequest(request);
        test_transport->Send(response);
        responses_sent.fetch_add(1);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

void MCPServer::HandleConnection(unique_ptr<MCPTransport> transport) {
    active_connections.fetch_add(1);

    try {
        while (running.load()) {
            try {
                auto request = transport->Receive();
                requests_received.fetch_add(1);
                auto response = HandleRequest(request);
                transport->Send(response);
                responses_sent.fetch_add(1);

                // If this was a shutdown request, break out of the loop
                if (request.method == MCPMethods::SHUTDOWN) {
                    break;
                }

                // Check max_requests limit (0 = unlimited)
                if (config.max_requests > 0 && requests_received.load() >= config.max_requests) {
                    running = false;
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
        {"version", Value(DUCKDB_MCP_VERSION)},
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

    // Create list with proper child type - use first element's type if non-empty
    Value resources_list;
    if (resources.empty()) {
        resources_list = Value::LIST(LogicalType::STRUCT({}), resources);
    } else {
        resources_list = Value::LIST(resources[0].type(), resources);
    }

    Value result = Value::STRUCT({
        {"resources", resources_list}
    });

    return MCPMessage::CreateResponse(result, request.id);
}

MCPMessage MCPServer::HandleResourcesRead(const MCPMessage &request) {
    // Extract URI from parameters
    // Params may be stored as JSON string or as STRUCT depending on source
    string uri;

    if (request.params.type().id() == LogicalTypeId::VARCHAR) {
        // Params stored as JSON string - parse it
        string params_str = request.params.ToString();
        yyjson_doc *doc = JSONUtils::Parse(params_str);
        yyjson_val *root = yyjson_doc_get_root(doc);
        uri = JSONUtils::GetString(root, "uri");
        JSONUtils::FreeDocument(doc);
    } else if (request.params.type().id() == LogicalTypeId::STRUCT) {
        // Params as STRUCT
        auto &struct_values = StructValue::GetChildren(request.params);
        for (size_t i = 0; i < struct_values.size(); i++) {
            auto &key = StructType::GetChildName(request.params.type(), i);
            if (key == "uri") {
                uri = struct_values[i].ToString();
                break;
            }
        }
    } else {
        return CreateErrorResponse(request.id, MCPErrorCodes::INVALID_PARAMS, "Invalid parameters format");
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

    // Build contents list with proper struct type
    child_list_t<LogicalType> contents_struct_members;
    contents_struct_members.push_back({"uri", LogicalType::VARCHAR});
    contents_struct_members.push_back({"mimeType", LogicalType::VARCHAR});
    contents_struct_members.push_back({"text", LogicalType::VARCHAR});
    LogicalType contents_struct_type = LogicalType::STRUCT(contents_struct_members);

    Value contents_item = Value::STRUCT({
        {"uri", Value(uri)},
        {"mimeType", Value(read_result.mime_type)},
        {"text", Value(read_result.content)}
    });

    Value result = Value::STRUCT({
        {"contents", Value::LIST(contents_struct_type, {contents_item})}
    });

    return MCPMessage::CreateResponse(result, request.id);
}

MCPMessage MCPServer::HandleToolsList(const MCPMessage &request) {
    auto tool_names = tool_registry.ListTools();

    vector<Value> tools;
    // Define a consistent struct type for all tools using JSON type for variable schema
    child_list_t<LogicalType> tool_struct_members;
    tool_struct_members.push_back({"name", LogicalType::VARCHAR});
    tool_struct_members.push_back({"description", LogicalType::VARCHAR});
    tool_struct_members.push_back({"inputSchema", LogicalType::JSON()});
    LogicalType tool_struct_type = LogicalType::STRUCT(tool_struct_members);

    for (const auto &name : tool_names) {
        auto handler = tool_registry.GetTool(name);
        if (handler) {
            // Serialize inputSchema to JSON string for consistent typing
            Value schema = handler->GetInputSchema().ToJSON();
            yyjson_mut_doc *doc = JSONUtils::CreateDocument();
            yyjson_mut_val *schema_json = JSONUtils::ValueToJSON(doc, schema);
            yyjson_mut_doc_set_root(doc, schema_json);
            string schema_str = JSONUtils::Serialize(doc);
            JSONUtils::FreeDocument(doc);

            // Create JSON-typed value for the schema
            Value schema_json_val(schema_str);
            schema_json_val.Reinterpret(LogicalType::JSON());

            Value tool = Value::STRUCT({
                {"name", Value(name)},
                {"description", Value(handler->GetDescription())},
                {"inputSchema", schema_json_val}
            });
            tools.push_back(tool);
        }
    }

    // Create list with the defined struct type
    Value tools_list = Value::LIST(tool_struct_type, tools);

    Value result = Value::STRUCT({
        {"tools", tools_list}
    });

    return MCPMessage::CreateResponse(result, request.id);
}

MCPMessage MCPServer::HandleToolsCall(const MCPMessage &request) {
    // Extract tool name and arguments from parameters
    // Params may be stored as JSON string or as STRUCT depending on source
    string tool_name;
    Value arguments;

    if (request.params.type().id() == LogicalTypeId::VARCHAR) {
        // Params stored as JSON string - parse it
        string params_str = request.params.ToString();
        yyjson_doc *doc = JSONUtils::Parse(params_str);
        yyjson_val *root = yyjson_doc_get_root(doc);

        tool_name = JSONUtils::GetString(root, "name");
        yyjson_val *args_val = JSONUtils::GetObject(root, "arguments");
        if (args_val) {
            size_t len;
            char *args_json = yyjson_val_write(args_val, 0, &len);
            if (args_json) {
                arguments = Value(string(args_json, len));
                free(args_json);
            }
        }
        JSONUtils::FreeDocument(doc);
    } else if (request.params.type().id() == LogicalTypeId::STRUCT) {
        // Params as STRUCT
        auto &struct_values = StructValue::GetChildren(request.params);
        for (size_t i = 0; i < struct_values.size(); i++) {
            auto &key = StructType::GetChildName(request.params.type(), i);
            if (key == "name") {
                tool_name = struct_values[i].ToString();
            } else if (key == "arguments") {
                arguments = struct_values[i];
            }
        }
    } else {
        return CreateErrorResponse(request.id, MCPErrorCodes::INVALID_PARAMS, "Invalid parameters format");
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

    // Build content list with proper struct type
    child_list_t<LogicalType> content_struct_members;
    content_struct_members.push_back({"type", LogicalType::VARCHAR});
    content_struct_members.push_back({"text", LogicalType::VARCHAR});
    LogicalType content_struct_type = LogicalType::STRUCT(content_struct_members);

    Value content_item = Value::STRUCT({
        {"type", Value("text")},
        {"text", call_result.result}
    });

    Value result = Value::STRUCT({
        {"content", Value::LIST(content_struct_type, {content_item})}
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
                                                       config.denied_queries,
                                                       config.default_result_format);
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

    if (config.enable_list_tables_tool) {
        auto list_tables_tool = make_uniq<ListTablesToolHandler>(*config.db_instance);
        tool_registry.RegisterTool("list_tables", std::move(list_tables_tool));
    }

    if (config.enable_database_info_tool) {
        auto database_info_tool = make_uniq<DatabaseInfoToolHandler>(*config.db_instance);
        tool_registry.RegisterTool("database_info", std::move(database_info_tool));
    }

    if (config.enable_execute_tool) {
        auto execute_tool = make_uniq<ExecuteToolHandler>(*config.db_instance,
                                                           config.execute_allow_ddl,
                                                           config.execute_allow_dml);
        tool_registry.RegisterTool("execute", std::move(execute_tool));
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

MCPMessage MCPServerManager::SendRequest(const MCPMessage &request) {
    lock_guard<mutex> lock(manager_mutex);

    if (!server) {
        return MCPMessage::CreateError(MCPErrorCodes::INTERNAL_ERROR,
            "No server running", request.id);
    }

    return server->ProcessRequest(request);
}

} // namespace duckdb