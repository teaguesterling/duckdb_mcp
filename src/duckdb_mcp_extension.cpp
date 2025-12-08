// src/duckdb_mcp_extension.cpp
#include "duckdb_mcp_extension.hpp"
#include "duckdb_mcp_config.hpp"
#include "duckdb_mcp_security.hpp"
#include "duckdb_mcp_logging.hpp"
#include "json_utils.hpp"
#include "yyjson.hpp"
#include <cstdlib>

using namespace duckdb_yyjson;
#include "mcpfs/mcp_file_system.hpp"
#include "client/mcp_storage_extension.hpp"
#include "protocol/mcp_connection.hpp"
#include "protocol/mcp_message.hpp"
#include "protocol/mcp_template.hpp"
#include "protocol/mcp_pagination.hpp"
#include "server/mcp_server.hpp"
#include "server/resource_providers.hpp"
#include "server/tool_handlers.hpp"
#include "server/memory_transport.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/enums/set_scope.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/config.hpp"

namespace duckdb {

// Get MCP resource content
static void MCPGetResourceFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &server_vector = args.data[0];
    auto &resource_vector = args.data[1];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    auto &result_validity = FlatVector::Validity(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        if (server_vector.GetValue(i).IsNull() || resource_vector.GetValue(i).IsNull()) {
            result_validity.SetInvalid(i);
            continue;
        }
        
        string server_name = server_vector.GetValue(i).ToString();
        string resource_uri = resource_vector.GetValue(i).ToString();
        
        try {
            // Get connection from registry
            auto connection = MCPConnectionRegistry::GetInstance().GetConnection(server_name);
            if (!connection) {
                throw InvalidInputException("MCP server not attached: " + server_name);
            }
            
            if (!connection->IsInitialized()) {
                if (!connection->Initialize()) {
                    throw IOException("Failed to initialize MCP connection: " + connection->GetLastError());
                }
            }
            
            // Get raw MCP response  
            Value params = Value::STRUCT({{"uri", Value(resource_uri)}});
            auto response = connection->SendRequest(MCPMethods::RESOURCES_READ, params);
            
            if (response.IsError()) {
                result_data[i] = StringVector::AddString(result, "MCP_ERROR: " + response.error.message);
                continue;
            }
            
            // Return raw JSON response
            result_data[i] = StringVector::AddString(result, response.result.ToString());
        } catch (const std::exception &e) {
            // For debugging, return the error message instead of NULL
            result_data[i] = StringVector::AddString(result, "ERROR: " + string(e.what()));
        }
    }
}

// List MCP resources
static void MCPListResourcesFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &server_vector = args.data[0];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    auto &result_validity = FlatVector::Validity(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        if (server_vector.GetValue(i).IsNull()) {
            result_data[i] = StringVector::AddString(result, "TRACE: server_name is null");
            continue;
        }
        
        string server_name = server_vector.GetValue(i).ToString();
        
        try {
            // Get connection from registry
            auto connection = MCPConnectionRegistry::GetInstance().GetConnection(server_name);
            if (!connection) {
                throw InvalidInputException("MCP server not attached: " + server_name);
            }
            
            if (!connection->IsInitialized()) {
                if (!connection->Initialize()) {
                    throw IOException("Failed to initialize MCP connection: " + connection->GetLastError());
                }
            }
            
            // Get raw MCP response
            auto response = connection->SendRequest(MCPMethods::RESOURCES_LIST, Value::STRUCT({}));
            
            if (response.IsError()) {
                result_data[i] = StringVector::AddString(result, "MCP_ERROR: " + response.error.message);
                continue;
            }
            
            // Return the actual JSON response from MCP server
            result_data[i] = StringVector::AddString(result, response.result.ToString());
        } catch (const std::exception &e) {
            // For debugging, return the error message instead of NULL
            result_data[i] = StringVector::AddString(result, "ERROR: " + string(e.what()));
        }
    }
}

// Call MCP tool
static void MCPCallToolFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &server_vector = args.data[0];
    auto &tool_vector = args.data[1];
    auto &params_vector = args.data[2];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    auto &result_validity = FlatVector::Validity(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        if (server_vector.GetValue(i).IsNull() || tool_vector.GetValue(i).IsNull()) {
            result_validity.SetInvalid(i);
            continue;
        }
        
        string server_name = server_vector.GetValue(i).ToString();
        string tool_name = tool_vector.GetValue(i).ToString();
        string params_json = params_vector.GetValue(i).IsNull() ? "{}" : params_vector.GetValue(i).ToString();
        
        try {
            // Get connection from registry
            auto connection = MCPConnectionRegistry::GetInstance().GetConnection(server_name);
            if (!connection) {
                throw InvalidInputException("MCP server not attached: " + server_name);
            }
            
            if (!connection->IsInitialized()) {
                if (!connection->Initialize()) {
                    throw IOException("Failed to initialize MCP connection: " + connection->GetLastError());
                }
            }
            
            // Create MCP tool call params
            // Note: For now, we pass the JSON string directly as arguments
            // A more sophisticated implementation would parse the JSON into a proper Value
            Value call_params = Value::STRUCT({
                {"name", Value(tool_name)},
                {"arguments", Value(params_json)}  // Pass raw JSON string
            });
            
            // Get raw MCP response
            auto response = connection->SendRequest(MCPMethods::TOOLS_CALL, call_params);
            
            if (response.IsError()) {
                result_data[i] = StringVector::AddString(result, "MCP_ERROR: " + response.error.message);
                continue;
            }
            
            // Return raw JSON response
            result_data[i] = StringVector::AddString(result, response.result.ToString());
        } catch (const std::exception &e) {
            // For debugging, return the error message instead of NULL
            result_data[i] = StringVector::AddString(result, "ERROR: " + string(e.what()));
        }
    }
}

// List MCP tools
static void MCPListToolsFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &server_vector = args.data[0];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    auto &result_validity = FlatVector::Validity(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        if (server_vector.GetValue(i).IsNull()) {
            result_data[i] = StringVector::AddString(result, "TRACE: server_name is null");
            continue;
        }
        
        string server_name = server_vector.GetValue(i).ToString();
        
        try {
            // Get connection from registry
            auto connection = MCPConnectionRegistry::GetInstance().GetConnection(server_name);
            if (!connection) {
                throw InvalidInputException("MCP server not attached: " + server_name);
            }
            
            if (!connection->IsInitialized()) {
                if (!connection->Initialize()) {
                    throw IOException("Failed to initialize MCP connection: " + connection->GetLastError());
                }
            }
            
            // Get raw MCP response
            auto response = connection->SendRequest(MCPMethods::TOOLS_LIST, Value::STRUCT({}));
            
            if (response.IsError()) {
                result_data[i] = StringVector::AddString(result, "MCP_ERROR: " + response.error.message);
                continue;
            }
            
            // Return the actual JSON response from MCP server
            result_data[i] = StringVector::AddString(result, response.result.ToString());
        } catch (const std::exception &e) {
            // For debugging, return the error message instead of NULL
            result_data[i] = StringVector::AddString(result, "ERROR: " + string(e.what()));
        }
    }
}

// List MCP prompts
static void MCPListPromptsFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &server_vector = args.data[0];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    auto &result_validity = FlatVector::Validity(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        if (server_vector.GetValue(i).IsNull()) {
            result_data[i] = StringVector::AddString(result, "TRACE: server_name is null");
            continue;
        }
        
        string server_name = server_vector.GetValue(i).ToString();
        
        try {
            // Get connection from registry
            auto connection = MCPConnectionRegistry::GetInstance().GetConnection(server_name);
            if (!connection) {
                throw InvalidInputException("MCP server not attached: " + server_name);
            }
            
            if (!connection->IsInitialized()) {
                if (!connection->Initialize()) {
                    throw IOException("Failed to initialize MCP connection: " + connection->GetLastError());
                }
            }
            
            // Get raw MCP response
            auto response = connection->SendRequest(MCPMethods::PROMPTS_LIST, Value::STRUCT({}));
            
            if (response.IsError()) {
                result_data[i] = StringVector::AddString(result, "MCP_ERROR: " + response.error.message);
                continue;
            }
            
            // Return the actual JSON response from MCP server
            result_data[i] = StringVector::AddString(result, response.result.ToString());
        } catch (const std::exception &e) {
            // For debugging, return the error message instead of NULL
            result_data[i] = StringVector::AddString(result, "ERROR: " + string(e.what()));
        }
    }
}

// Get MCP prompt content
static void MCPGetPromptFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &server_vector = args.data[0];
    auto &prompt_vector = args.data[1];
    auto &params_vector = args.data[2];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    auto &result_validity = FlatVector::Validity(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        if (server_vector.GetValue(i).IsNull() || prompt_vector.GetValue(i).IsNull()) {
            result_validity.SetInvalid(i);
            continue;
        }
        
        string server_name = server_vector.GetValue(i).ToString();
        string prompt_name = prompt_vector.GetValue(i).ToString();
        string params_json = params_vector.GetValue(i).IsNull() ? "{}" : params_vector.GetValue(i).ToString();
        
        try {
            // Get connection from registry
            auto connection = MCPConnectionRegistry::GetInstance().GetConnection(server_name);
            if (!connection) {
                throw InvalidInputException("MCP server not attached: " + server_name);
            }
            
            if (!connection->IsInitialized()) {
                if (!connection->Initialize()) {
                    throw IOException("Failed to initialize MCP connection: " + connection->GetLastError());
                }
            }
            
            // Create MCP prompt get params
            Value call_params = Value::STRUCT({
                {"name", Value(prompt_name)},
                {"arguments", Value(params_json)}  // Pass raw JSON string for prompt arguments
            });
            
            // Get raw MCP response
            auto response = connection->SendRequest(MCPMethods::PROMPTS_GET, call_params);
            
            if (response.IsError()) {
                result_data[i] = StringVector::AddString(result, "MCP_ERROR: " + response.error.message);
                continue;
            }
            
            // Return raw JSON response
            result_data[i] = StringVector::AddString(result, response.result.ToString());
        } catch (const std::exception &e) {
            // For debugging, return the error message instead of NULL
            result_data[i] = StringVector::AddString(result, "ERROR: " + string(e.what()));
        }
    }
}

// Reconnect to an MCP server
static void MCPReconnectServerFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &server_vector = args.data[0];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    auto &result_validity = FlatVector::Validity(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        if (server_vector.GetValue(i).IsNull()) {
            result_data[i] = StringVector::AddString(result, "ERROR: server_name is null");
            continue;
        }
        
        string server_name = server_vector.GetValue(i).ToString();
        
        try {
            // Get connection from registry
            auto connection = MCPConnectionRegistry::GetInstance().GetConnection(server_name);
            if (!connection) {
                result_data[i] = StringVector::AddString(result, "ERROR: MCP server not found: " + server_name);
                continue;
            }
            
            // Force disconnect and reconnect
            connection->Disconnect();
            
            bool connected = connection->Connect();
            if (!connected) {
                result_data[i] = StringVector::AddString(result, "ERROR: Failed to reconnect to server: " + 
                    connection->GetLastError());
                continue;
            }
            
            // Re-initialize the connection
            bool initialized = connection->Initialize();
            if (!initialized) {
                result_data[i] = StringVector::AddString(result, "ERROR: Failed to re-initialize server: " + 
                    connection->GetLastError());
                continue;
            }
            
            // Return success message with connection info
            result_data[i] = StringVector::AddString(result, "SUCCESS: Reconnected to " + connection->GetConnectionInfo());
            
        } catch (const std::exception &e) {
            result_data[i] = StringVector::AddString(result, "ERROR: " + string(e.what()));
        }
    }
}

// Get MCP server connection health status
static void MCPServerHealthFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &server_vector = args.data[0];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    auto &result_validity = FlatVector::Validity(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        if (server_vector.GetValue(i).IsNull()) {
            result_data[i] = StringVector::AddString(result, "ERROR: server_name is null");
            continue;
        }
        
        string server_name = server_vector.GetValue(i).ToString();
        
        try {
            // Get connection from registry
            auto connection = MCPConnectionRegistry::GetInstance().GetConnection(server_name);
            if (!connection) {
                result_data[i] = StringVector::AddString(result, "ERROR: MCP server not found: " + server_name);
                continue;
            }
            
            // Get detailed health information
            string health_info = "Server: " + server_name + "\n";
            
            // Connection state
            auto state = connection->GetState();
            switch (state) {
                case MCPConnectionState::DISCONNECTED:
                    health_info += "State: DISCONNECTED\n";
                    break;
                case MCPConnectionState::CONNECTING:
                    health_info += "State: CONNECTING\n";
                    break;
                case MCPConnectionState::CONNECTED:
                    health_info += "State: CONNECTED\n";
                    break;
                case MCPConnectionState::INITIALIZED:
                    health_info += "State: INITIALIZED\n";
                    break;
                case MCPConnectionState::ERROR:
                    health_info += "State: ERROR\n";
                    break;
            }
            
            // Health status
            health_info += "Healthy: " + string(connection->IsHealthy() ? "true" : "false") + "\n";
            
            // Error information
            string last_error = connection->GetLastError();
            if (!last_error.empty()) {
                health_info += "Last Error: " + last_error + "\n";
                health_info += "Recoverable: " + string(connection->HasRecoverableError() ? "true" : "false") + "\n";
            }
            
            // Connection statistics
            health_info += "Consecutive Failures: " + std::to_string(connection->GetConsecutiveFailures()) + "\n";
            health_info += "Last Activity: " + std::to_string(connection->GetLastActivityTime()) + "\n";
            health_info += "Connection Info: " + connection->GetConnectionInfo();
            
            result_data[i] = StringVector::AddString(result, health_info);
            
        } catch (const std::exception &e) {
            result_data[i] = StringVector::AddString(result, "ERROR: " + string(e.what()));
        }
    }
}

// MCPStatus struct type definition
static LogicalType GetMCPStatusType() {
    child_list_t<LogicalType> members;
    members.push_back({"success", LogicalType::BOOLEAN});
    members.push_back({"running", LogicalType::BOOLEAN});
    members.push_back({"message", LogicalType::VARCHAR});
    members.push_back({"transport", LogicalType::VARCHAR});
    members.push_back({"listen", LogicalType::VARCHAR});
    members.push_back({"port", LogicalType::INTEGER});
    members.push_back({"background", LogicalType::BOOLEAN});
    members.push_back({"requests_received", LogicalType::UBIGINT});
    members.push_back({"responses_sent", LogicalType::UBIGINT});
    return LogicalType::STRUCT(members);
}

// Helper to create MCPStatus struct value
static Value CreateMCPStatus(bool success, bool running, const string &message,
                              const string &transport = "", const string &listen = "",
                              int port = 0, bool background = false,
                              uint64_t requests_received = 0, uint64_t responses_sent = 0) {
    child_list_t<Value> values;
    values.push_back({"success", Value::BOOLEAN(success)});
    values.push_back({"running", Value::BOOLEAN(running)});
    values.push_back({"message", Value(message)});
    values.push_back({"transport", Value(transport)});
    values.push_back({"listen", Value(listen)});
    values.push_back({"port", Value::INTEGER(port)});
    values.push_back({"background", Value::BOOLEAN(background)});
    values.push_back({"requests_received", Value::UBIGINT(requests_received)});
    values.push_back({"responses_sent", Value::UBIGINT(responses_sent)});
    return Value::STRUCT(values);
}

// Core implementation for starting MCP server - returns MCPStatus struct
static Value MCPServerStartImpl(ExpressionState &state,
                                 const string &transport, const string &bind_address,
                                 int port, const string &config_json) {
    try {
        // Get database instance from state
        auto &context = state.GetContext();
        auto &db_instance = DatabaseInstance::GetDatabase(context);

        // Check if serving is disabled
        auto &security = MCPSecurityConfig::GetInstance();
        if (security.IsServingDisabled()) {
            return CreateMCPStatus(false, false, "MCP server functionality is disabled (mcp_disable_serving=true)");
        }

        // Check if server is already running
        auto &server_manager = MCPServerManager::GetInstance();
        if (server_manager.IsServerRunning()) {
            return CreateMCPStatus(false, true, "MCP server is already running. Stop it first with mcp_server_stop()",
                                   transport, bind_address, port, true);
        }

        // Create server configuration
        MCPServerConfig server_config;
        server_config.transport = transport;
        server_config.bind_address = bind_address;
        server_config.port = port;
        server_config.db_instance = &db_instance;

        // Parse config_json for additional settings
        if (!config_json.empty() && config_json != "{}") {
            yyjson_doc *doc = JSONUtils::Parse(config_json);
            yyjson_val *root = yyjson_doc_get_root(doc);
            if (root && yyjson_is_obj(root)) {
                // Parse max_requests
                server_config.max_requests = static_cast<uint32_t>(JSONUtils::GetInt(root, "max_requests", 0));

                // Parse tool enable/disable flags
                yyjson_val *val = yyjson_obj_get(root, "enable_query_tool");
                if (val && yyjson_is_bool(val)) {
                    server_config.enable_query_tool = yyjson_get_bool(val);
                }
                val = yyjson_obj_get(root, "enable_describe_tool");
                if (val && yyjson_is_bool(val)) {
                    server_config.enable_describe_tool = yyjson_get_bool(val);
                }
                val = yyjson_obj_get(root, "enable_export_tool");
                if (val && yyjson_is_bool(val)) {
                    server_config.enable_export_tool = yyjson_get_bool(val);
                }
                val = yyjson_obj_get(root, "enable_list_tables_tool");
                if (val && yyjson_is_bool(val)) {
                    server_config.enable_list_tables_tool = yyjson_get_bool(val);
                }
                val = yyjson_obj_get(root, "enable_database_info_tool");
                if (val && yyjson_is_bool(val)) {
                    server_config.enable_database_info_tool = yyjson_get_bool(val);
                }
                val = yyjson_obj_get(root, "enable_execute_tool");
                if (val && yyjson_is_bool(val)) {
                    server_config.enable_execute_tool = yyjson_get_bool(val);
                }
                val = yyjson_obj_get(root, "background");
                if (val && yyjson_is_bool(val)) {
                    server_config.background = yyjson_get_bool(val);
                }

                // Parse default result format
                server_config.default_result_format = JSONUtils::GetString(root, "default_result_format", "json");
            }
            JSONUtils::FreeDocument(doc);
        }

        // Handle different transport types
        if (transport == "stdio") {
            if (server_config.background) {
                // Background mode: use server manager (non-blocking, starts thread)
                if (server_manager.StartServer(server_config)) {
                    return CreateMCPStatus(true, true, "MCP server started on stdio (background mode)",
                                           transport, bind_address, port, true);
                } else {
                    return CreateMCPStatus(false, false, "Failed to start MCP server",
                                           transport, bind_address, port, true);
                }
            } else {
                // Foreground mode (default): handle connection directly without background thread
                MCPServer server(server_config);
                if (!server.StartForeground()) {
                    return CreateMCPStatus(false, false, "Failed to initialize MCP server",
                                           transport, bind_address, port, false);
                }
                try {
                    server.RunMainLoop(); // Blocks until max_requests or shutdown
                    return CreateMCPStatus(true, false, "MCP server completed",
                                           transport, bind_address, port, false,
                                           server.GetRequestsReceived(), server.GetResponsesSent());
                } catch (const std::exception &e) {
                    return CreateMCPStatus(false, false, string(e.what()),
                                           transport, bind_address, port, false,
                                           server.GetRequestsReceived(), server.GetResponsesSent());
                }
            }
        } else if (transport == "memory") {
            // Memory transport is always background mode (for testing with mcp_server_send_request)
            // The server doesn't do I/O - it just waits for requests via ProcessRequest()
            server_config.background = true;
            if (server_manager.StartServer(server_config)) {
                return CreateMCPStatus(true, true, "MCP server started on memory transport (background mode)",
                                       transport, bind_address, port, true);
            } else {
                return CreateMCPStatus(false, false, "Failed to start MCP server",
                                       transport, bind_address, port, true);
            }
        } else {
            // For non-stdio/memory transports (TCP/WebSocket), use background thread
            if (server_manager.StartServer(server_config)) {
                return CreateMCPStatus(true, true, "MCP server started on " + transport,
                                       transport, bind_address, port, true);
            } else {
                return CreateMCPStatus(false, false, "Failed to start MCP server",
                                       transport, bind_address, port, true);
            }
        }

    } catch (const std::exception &e) {
        return CreateMCPStatus(false, false, string(e.what()), transport, bind_address, port, false);
    }
}

// Start MCP server with just transport (simplest form for stdio)
static void MCPServerStartSimpleFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &transport_vector = args.data[0];

    for (idx_t i = 0; i < args.size(); i++) {
        string transport = transport_vector.GetValue(i).IsNull() ? "stdio" : transport_vector.GetValue(i).ToString();
        Value status = MCPServerStartImpl(state, transport, "localhost", 0, "{}");
        result.SetValue(i, status);
    }
}

// Start MCP server with transport and config JSON
static void MCPServerStartConfigFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &transport_vector = args.data[0];
    auto &config_json_vector = args.data[1];

    for (idx_t i = 0; i < args.size(); i++) {
        string transport = transport_vector.GetValue(i).IsNull() ? "stdio" : transport_vector.GetValue(i).ToString();
        string config_json = config_json_vector.GetValue(i).IsNull() ? "{}" : config_json_vector.GetValue(i).ToString();
        Value status = MCPServerStartImpl(state, transport, "localhost", 0, config_json);
        result.SetValue(i, status);
    }
}

// Start MCP server with full configuration (transport, bind_address, port, config_json)
static void MCPServerStartFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &transport_vector = args.data[0];
    auto &bind_address_vector = args.data[1];
    auto &port_vector = args.data[2];
    auto &config_json_vector = args.data[3];

    for (idx_t i = 0; i < args.size(); i++) {
        string transport = transport_vector.GetValue(i).IsNull() ? "stdio" : transport_vector.GetValue(i).ToString();
        string bind_address = bind_address_vector.GetValue(i).IsNull() ? "localhost" : bind_address_vector.GetValue(i).ToString();
        int port = port_vector.GetValue(i).IsNull() ? 8080 : port_vector.GetValue(i).GetValue<int32_t>();
        string config_json = config_json_vector.GetValue(i).IsNull() ? "{}" : config_json_vector.GetValue(i).ToString();
        Value status = MCPServerStartImpl(state, transport, bind_address, port, config_json);
        result.SetValue(i, status);
    }
}

// Stop MCP server (no parameters - returns error if not running)
static void MCPServerStopFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    for (idx_t i = 0; i < args.size(); i++) {
        try {
            auto &server_manager = MCPServerManager::GetInstance();

            if (!server_manager.IsServerRunning()) {
                result.SetValue(i, CreateMCPStatus(false, false, "MCP server is not running"));
                continue;
            }

            server_manager.StopServer();
            result.SetValue(i, CreateMCPStatus(true, false, "MCP server stopped"));

        } catch (const std::exception &e) {
            result.SetValue(i, CreateMCPStatus(false, false, string(e.what())));
        }
    }
}

// Stop MCP server with force option - always succeeds, ensures clean state
static void MCPServerStopForceFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &force_vector = args.data[0];

    for (idx_t i = 0; i < args.size(); i++) {
        try {
            bool force = force_vector.GetValue(i).IsNull() ? false : force_vector.GetValue(i).GetValue<bool>();
            auto &server_manager = MCPServerManager::GetInstance();

            bool was_running = server_manager.IsServerRunning();
            if (was_running) {
                server_manager.StopServer();
            }

            if (force) {
                // Force mode: always return success, used for test setup/teardown
                // Always say "state cleared" for consistent test behavior regardless of prior state
                result.SetValue(i, CreateMCPStatus(true, false, "MCP server state cleared (forced)"));
            } else {
                // Normal mode: return error if wasn't running
                if (was_running) {
                    result.SetValue(i, CreateMCPStatus(true, false, "MCP server stopped"));
                } else {
                    result.SetValue(i, CreateMCPStatus(false, false, "MCP server is not running"));
                }
            }

        } catch (const std::exception &e) {
            result.SetValue(i, CreateMCPStatus(false, false, string(e.what())));
        }
    }
}

// Get MCP server status
static void MCPServerStatusFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    for (idx_t i = 0; i < args.size(); i++) {
        try {
            auto &server_manager = MCPServerManager::GetInstance();

            if (!server_manager.IsServerRunning()) {
                result.SetValue(i, CreateMCPStatus(true, false, "Server is stopped"));
                continue;
            }

            auto server = server_manager.GetServer();
            if (server) {
                // Server is running - get detailed status with statistics
                result.SetValue(i, CreateMCPStatus(true, true, server->GetStatus(),
                                                   "", "", 0, true,
                                                   server->GetRequestsReceived(),
                                                   server->GetResponsesSent()));
            } else {
                result.SetValue(i, CreateMCPStatus(false, false, "Server manager inconsistency"));
            }

        } catch (const std::exception &e) {
            result.SetValue(i, CreateMCPStatus(false, false, string(e.what())));
        }
    }
}

// Test MCP server protocol handling (for unit testing)
// Takes a JSON-RPC request string, processes it through the MCP server, returns response JSON
static void MCPServerTestFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &request_vector = args.data[0];

    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);

    for (idx_t i = 0; i < args.size(); i++) {
        try {
            // Get database instance from state
            auto &context = state.GetContext();
            auto &db_instance = DatabaseInstance::GetDatabase(context);

            // Parse the JSON-RPC request
            string request_json = request_vector.GetValue(i).ToString();
            MCPMessage request = MCPMessage::FromJSON(request_json);

            // Create a server with default config
            MCPServerConfig server_config;
            server_config.transport = "stdio";
            server_config.db_instance = &db_instance;

            MCPServer server(server_config);
            if (!server.StartForeground()) {
                result_data[i] = StringVector::AddString(result,
                    R"({"jsonrpc":"2.0","error":{"code":-32603,"message":"Failed to initialize server"},"id":null})");
                continue;
            }

            // Process the request directly (no transport needed)
            MCPMessage response = server.ProcessRequest(request);

            // Return the response as JSON
            try {
                string json_response = response.ToJSON();
                result_data[i] = StringVector::AddString(result, json_response);
            } catch (const std::exception &e) {
                result_data[i] = StringVector::AddString(result,
                    R"({"jsonrpc":"2.0","error":{"code":-32603,"message":"ToJSON failed: )" +
                    string(e.what()) + R"("},"id":null})");
            }

        } catch (const std::exception &e) {
            // Return a JSON-RPC error response
            string error_response = R"({"jsonrpc":"2.0","error":{"code":-32603,"message":")" +
                                   string(e.what()) + R"("},"id":null})";
            result_data[i] = StringVector::AddString(result, error_response);
        }
    }
}

// Send MCP request to running server
// mcp_server_send_request(request_json) - requires a server to be running via mcp_server_start
static void MCPServerSendRequestFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &request_vector = args.data[0];

    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);

    for (idx_t i = 0; i < args.size(); i++) {
        try {
            // Check if a server is running
            auto &server_manager = MCPServerManager::GetInstance();
            if (!server_manager.IsServerRunning()) {
                result_data[i] = StringVector::AddString(result,
                    R"json({"jsonrpc":"2.0","error":{"code":-32603,"message":"No MCP server running. Start one with mcp_server_start('memory')"},"id":null})json");
                continue;
            }

            // Parse the JSON-RPC request
            string request_json = request_vector.GetValue(i).ToString();
            MCPMessage request = MCPMessage::FromJSON(request_json);

            // Route to running server
            MCPMessage response = server_manager.SendRequest(request);
            try {
                string json_response = response.ToJSON();
                result_data[i] = StringVector::AddString(result, json_response);
            } catch (const std::exception &e) {
                result_data[i] = StringVector::AddString(result,
                    R"json({"jsonrpc":"2.0","error":{"code":-32603,"message":"ToJSON failed: )json" +
                    string(e.what()) + R"json("},"id":null})json");
            }

        } catch (const std::exception &e) {
            string error_response = R"json({"jsonrpc":"2.0","error":{"code":-32603,"message":")json" +
                                   string(e.what()) + R"json("},"id":null})json";
            result_data[i] = StringVector::AddString(result, error_response);
        }
    }
}

// Publish table as MCP resource
static void MCPPublishTableFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &table_vector = args.data[0];
    auto &uri_vector = args.data[1];
    auto &format_vector = args.data[2];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        try {
            auto &server_manager = MCPServerManager::GetInstance();
            
            if (!server_manager.IsServerRunning()) {
                result_data[i] = StringVector::AddString(result, "ERROR: MCP server is not running");
                continue;
            }
            
            // Parse parameters
            string table_name = table_vector.GetValue(i).ToString();
            string resource_uri = uri_vector.GetValue(i).IsNull() ? 
                ("data://tables/" + table_name) : uri_vector.GetValue(i).ToString();
            string format = format_vector.GetValue(i).IsNull() ? "json" : format_vector.GetValue(i).ToString();
            
            // Get database instance
            auto &context = state.GetContext();
            auto &db_instance = DatabaseInstance::GetDatabase(context);
            
            // Create resource provider
            auto provider = make_uniq<TableResourceProvider>(table_name, format, db_instance);
            
            // Publish resource
            auto server = server_manager.GetServer();
            if (server->PublishResource(resource_uri, std::move(provider))) {
                result_data[i] = StringVector::AddString(result, "SUCCESS: Published table '" + table_name + 
                    "' as resource '" + resource_uri + "' in " + format + " format");
            } else {
                result_data[i] = StringVector::AddString(result, "ERROR: Failed to publish table");
            }
            
        } catch (const std::exception &e) {
            result_data[i] = StringVector::AddString(result, "ERROR: " + string(e.what()));
        }
    }
}

// Publish query as MCP resource
static void MCPPublishQueryFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &query_vector = args.data[0];
    auto &uri_vector = args.data[1];
    auto &format_vector = args.data[2];
    auto &refresh_vector = args.data[3];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        try {
            auto &server_manager = MCPServerManager::GetInstance();
            
            if (!server_manager.IsServerRunning()) {
                result_data[i] = StringVector::AddString(result, "ERROR: MCP server is not running");
                continue;
            }
            
            // Parse parameters
            string query = query_vector.GetValue(i).ToString();
            string resource_uri = uri_vector.GetValue(i).ToString();
            string format = format_vector.GetValue(i).IsNull() ? "json" : format_vector.GetValue(i).ToString();
            uint32_t refresh_seconds = refresh_vector.GetValue(i).IsNull() ? 0 : refresh_vector.GetValue(i).GetValue<uint32_t>();
            
            // Get database instance
            auto &context = state.GetContext();
            auto &db_instance = DatabaseInstance::GetDatabase(context);
            
            // Create resource provider
            auto provider = make_uniq<QueryResourceProvider>(query, format, db_instance, refresh_seconds);
            
            // Publish resource
            auto server = server_manager.GetServer();
            if (server->PublishResource(resource_uri, std::move(provider))) {
                string refresh_info = refresh_seconds > 0 ? 
                    " (refresh every " + std::to_string(refresh_seconds) + "s)" : " (no refresh)";
                result_data[i] = StringVector::AddString(result, "SUCCESS: Published query as resource '" + 
                    resource_uri + "' in " + format + " format" + refresh_info);
            } else {
                result_data[i] = StringVector::AddString(result, "ERROR: Failed to publish query");
            }
            
        } catch (const std::exception &e) {
            result_data[i] = StringVector::AddString(result, "ERROR: " + string(e.what()));
        }
    }
}

// MCP diagnostics function
static void MCPGetDiagnosticsFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    auto &result_validity = FlatVector::Validity(result);
    
    try {
        auto &logger = MCPLogger::GetInstance();
        
        // Create diagnostics JSON
        yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
        yyjson_mut_val *root = yyjson_mut_obj(doc);
        yyjson_mut_doc_set_root(doc, root);
        
        // Log level
        string level_str;
        switch (logger.GetLogLevel()) {
            case MCPLogLevel::TRACE: level_str = "trace"; break;
            case MCPLogLevel::DEBUG: level_str = "debug"; break;
            case MCPLogLevel::INFO:  level_str = "info"; break;
            case MCPLogLevel::WARN:  level_str = "warn"; break;
            case MCPLogLevel::ERROR: level_str = "error"; break;
            case MCPLogLevel::OFF:   level_str = "off"; break;
            default: level_str = "unknown"; break;
        }
        
        yyjson_mut_obj_add_str(doc, root, "log_level", level_str.c_str());
        yyjson_mut_obj_add_str(doc, root, "extension_version", DUCKDB_MCP_VERSION);
        yyjson_mut_obj_add_bool(doc, root, "logging_available", true);
        
        // Convert to string
        const char *json_str = yyjson_mut_write(doc, 0, nullptr);
        string json_result(json_str);
        free((void*)json_str);
        yyjson_mut_doc_free(doc);
        
        result_data[0] = StringVector::AddString(result, json_result);
        
    } catch (const std::exception &e) {
        result_data[0] = StringVector::AddString(result, 
            StringUtil::Format("{\"error\": \"Failed to get diagnostics: %s\"}", e.what()));
    }
}

// Callback functions for MCP configuration settings
static void SetAllowedMCPCommands(ClientContext &context, SetScope scope, Value &parameter) {
    auto &security = MCPSecurityConfig::GetInstance();
    security.SetAllowedCommands(parameter.ToString());
}

static void SetMCPLogLevel(ClientContext &context, SetScope scope, Value &parameter) {
    auto level_str = parameter.ToString();
    MCPLogLevel level = MCPLogLevel::WARN; // default
    
    if (level_str == "trace" || level_str == "TRACE") level = MCPLogLevel::TRACE;
    else if (level_str == "debug" || level_str == "DEBUG") level = MCPLogLevel::DEBUG;
    else if (level_str == "info" || level_str == "INFO") level = MCPLogLevel::INFO;
    else if (level_str == "warn" || level_str == "WARN") level = MCPLogLevel::WARN;
    else if (level_str == "error" || level_str == "ERROR") level = MCPLogLevel::ERROR;
    else if (level_str == "off" || level_str == "OFF") level = MCPLogLevel::OFF;
    
    MCPLogger::GetInstance().SetLogLevel(level);
}

static void SetMCPLogFile(ClientContext &context, SetScope scope, Value &parameter) {
    MCPLogger::GetInstance().SetLogFile(parameter.ToString());
}

static void SetMCPConsoleLogging(ClientContext &context, SetScope scope, Value &parameter) {
    MCPLogger::GetInstance().EnableConsoleLogging(parameter.GetValue<bool>());
}

static void SetAllowedMCPUrls(ClientContext &context, SetScope scope, Value &parameter) {
    auto &security = MCPSecurityConfig::GetInstance();
    security.SetAllowedUrls(parameter.ToString());
}

static void SetMCPServerFile(ClientContext &context, SetScope scope, Value &parameter) {
    auto &security = MCPSecurityConfig::GetInstance();
    security.SetServerFile(parameter.ToString());
}

static void SetMCPLockServers(ClientContext &context, SetScope scope, Value &parameter) {
    auto &security = MCPSecurityConfig::GetInstance();
    bool lock = parameter.GetValue<bool>();
    security.LockServers(lock);
}

static void SetMCPDisableServing(ClientContext &context, SetScope scope, Value &parameter) {
    auto &security = MCPSecurityConfig::GetInstance();
    bool disable = parameter.GetValue<bool>();
    security.SetServingDisabled(disable);
}

// MCP-Compliant Pagination Functions

// List resources with optional cursor (MCP-compliant)
static void MCPListResourcesWithCursorFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &server_vector = args.data[0];
    auto &cursor_vector = args.data[1];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        if (server_vector.GetValue(i).IsNull()) {
            result_data[i] = StringVector::AddString(result, "null");
            continue;
        }
        
        string server_name = server_vector.GetValue(i).ToString();
        string cursor = cursor_vector.GetValue(i).IsNull() ? "" : cursor_vector.GetValue(i).ToString();
        
        try {
            auto connection = MCPConnectionRegistry::GetInstance().GetConnection(server_name);
            if (!connection) {
                throw InvalidInputException("MCP server not attached: " + server_name);
            }
            
            // Use tool call for pagination instead of modifying standard MCP methods
            Value call_params = Value::STRUCT({
                {"name", Value("list_resources_paginated")},
                {"arguments", Value(cursor.empty() ? "{}" : "{\"cursor\": \"" + cursor + "\"}")}
            });
            
            // Send MCP tool call for pagination
            auto response = connection->SendRequest(MCPMethods::TOOLS_CALL, call_params);
            
            if (response.IsError()) {
                throw IOException("MCP list resources failed: " + response.error.message);
            }
            
            // Return raw JSON response (same as existing functions)
            result_data[i] = StringVector::AddString(result, response.result.ToString());
            
        } catch (const std::exception &e) {
            result_data[i] = StringVector::AddString(result, "{\"error\": \"" + string(e.what()) + "\"}");
        }
    }
}

// List tools with optional cursor (MCP-compliant)
static void MCPListToolsWithCursorFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &server_vector = args.data[0];
    auto &cursor_vector = args.data[1];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        if (server_vector.GetValue(i).IsNull()) {
            result_data[i] = StringVector::AddString(result, "null");
            continue;
        }
        
        string server_name = server_vector.GetValue(i).ToString();
        string cursor = cursor_vector.GetValue(i).IsNull() ? "" : cursor_vector.GetValue(i).ToString();
        
        try {
            auto connection = MCPConnectionRegistry::GetInstance().GetConnection(server_name);
            if (!connection) {
                throw InvalidInputException("MCP server not attached: " + server_name);
            }
            
            // Use tool call for pagination
            Value call_params = Value::STRUCT({
                {"name", Value("list_tools_paginated")},
                {"arguments", Value(cursor.empty() ? "{}" : "{\"cursor\": \"" + cursor + "\"}")}
            });
            
            // Send MCP tool call for pagination
            auto response = connection->SendRequest(MCPMethods::TOOLS_CALL, call_params);
            
            if (response.IsError()) {
                throw IOException("MCP list tools failed: " + response.error.message);
            }
            
            // Return raw JSON response (same as existing functions)
            result_data[i] = StringVector::AddString(result, response.result.ToString());
            
        } catch (const std::exception &e) {
            result_data[i] = StringVector::AddString(result, "{\"error\": \"" + string(e.what()) + "\"}");
        }
    }
}

// List prompts with optional cursor (MCP-compliant)
static void MCPListPromptsWithCursorFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &server_vector = args.data[0];
    auto &cursor_vector = args.data[1];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        if (server_vector.GetValue(i).IsNull()) {
            result_data[i] = StringVector::AddString(result, "null");
            continue;
        }
        
        string server_name = server_vector.GetValue(i).ToString();
        string cursor = cursor_vector.GetValue(i).IsNull() ? "" : cursor_vector.GetValue(i).ToString();
        
        try {
            auto connection = MCPConnectionRegistry::GetInstance().GetConnection(server_name);
            if (!connection) {
                throw InvalidInputException("MCP server not attached: " + server_name);
            }
            
            // Use tool call for pagination
            Value call_params = Value::STRUCT({
                {"name", Value("list_prompts_paginated")},
                {"arguments", Value(cursor.empty() ? "{}" : "{\"cursor\": \"" + cursor + "\"}")}
            });
            
            // Send MCP tool call for pagination
            auto response = connection->SendRequest(MCPMethods::TOOLS_CALL, call_params);
            
            if (response.IsError()) {
                throw IOException("MCP list prompts failed: " + response.error.message);
            }
            
            // Return raw JSON response (same as existing functions)
            result_data[i] = StringVector::AddString(result, response.result.ToString());
            
        } catch (const std::exception &e) {
            result_data[i] = StringVector::AddString(result, "{\"error\": \"" + string(e.what()) + "\"}");
        }
    }
}

// MCP Template Functions

static void MCPRegisterPromptTemplateFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &name_vector = args.data[0];
    auto &description_vector = args.data[1];
    auto &template_vector = args.data[2];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    auto &result_validity = FlatVector::Validity(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        if (name_vector.GetValue(i).IsNull() || description_vector.GetValue(i).IsNull() ||
            template_vector.GetValue(i).IsNull()) {
            result_validity.SetInvalid(i);
            continue;
        }
        
        string name = name_vector.GetValue(i).ToString();
        string description = description_vector.GetValue(i).ToString();
        string template_content = template_vector.GetValue(i).ToString();
        
        try {
            MCPTemplate template_def(name, description, template_content);
            
            auto &manager = MCPTemplateManager::GetInstance();
            manager.RegisterTemplate(template_def);
            
            result_data[i] = StringVector::AddString(result, "Template registered: " + name);
            
        } catch (const std::exception &e) {
            result_data[i] = StringVector::AddString(result, "Error: " + string(e.what()));
        }
    }
}

static void MCPListPromptTemplatesFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    auto &result_validity = FlatVector::Validity(result);
    
    try {
        auto &manager = MCPTemplateManager::GetInstance();
        auto templates = manager.ListTemplates();
        
        // Create JSON array of templates
        yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
        yyjson_mut_val *root = yyjson_mut_arr(doc);
        yyjson_mut_doc_set_root(doc, root);
        
        for (const auto &template_def : templates) {
            yyjson_mut_val *template_obj = yyjson_mut_obj(doc);
            yyjson_mut_obj_add_str(doc, template_obj, "name", template_def.name.c_str());
            yyjson_mut_obj_add_str(doc, template_obj, "description", template_def.description.c_str());
            
            // Add arguments array
            yyjson_mut_val *args_arr = yyjson_mut_arr(doc);
            for (const auto &arg : template_def.arguments) {
                yyjson_mut_val *arg_obj = yyjson_mut_obj(doc);
                yyjson_mut_obj_add_str(doc, arg_obj, "name", arg.name.c_str());
                yyjson_mut_obj_add_str(doc, arg_obj, "description", arg.description.c_str());
                yyjson_mut_obj_add_bool(doc, arg_obj, "required", arg.required);
                yyjson_mut_arr_append(args_arr, arg_obj);
            }
            yyjson_mut_obj_add_val(doc, template_obj, "arguments", args_arr);
            
            yyjson_mut_arr_append(root, template_obj);
        }
        
        const char *json = yyjson_mut_write(doc, 0, nullptr);
        result_data[0] = StringVector::AddString(result, json);
        
        yyjson_mut_doc_free(doc);
        
    } catch (const std::exception &e) {
        result_data[0] = StringVector::AddString(result, "Error: " + string(e.what()));
    }
}

static void MCPRenderPromptTemplateFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &name_vector = args.data[0];
    auto &args_vector = args.data[1];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    auto &result_validity = FlatVector::Validity(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        if (name_vector.GetValue(i).IsNull()) {
            result_validity.SetInvalid(i);
            continue;
        }
        
        string name = name_vector.GetValue(i).ToString();
        unordered_map<string, string> template_args;
        
        // Parse arguments JSON if provided
        if (!args_vector.GetValue(i).IsNull()) {
            string args_json = args_vector.GetValue(i).ToString();
            
            yyjson_doc *doc = yyjson_read(args_json.c_str(), args_json.length(), 0);
            if (doc) {
                yyjson_val *root = yyjson_doc_get_root(doc);
                if (yyjson_is_obj(root)) {
                    yyjson_obj_iter iter = yyjson_obj_iter_with(root);
                    yyjson_val *key, *val;
                    while ((key = yyjson_obj_iter_next(&iter))) {
                        val = yyjson_obj_iter_get_val(key);
                        if (yyjson_is_str(val)) {
                            template_args[yyjson_get_str(key)] = yyjson_get_str(val);
                        }
                    }
                }
                yyjson_doc_free(doc);
            }
        }
        
        try {
            auto &manager = MCPTemplateManager::GetInstance();
            string rendered = manager.RenderTemplate(name, template_args);
            result_data[i] = StringVector::AddString(result, rendered);
            
        } catch (const std::exception &e) {
            result_data[i] = StringVector::AddString(result, "Error: " + string(e.what()));
        }
    }
}

static void LoadInternal(ExtensionLoader &loader) {
    auto &db = loader.GetDatabaseInstance();
    
    // Register MCPFS file system
    auto &fs = FileSystem::GetFileSystem(db);
    fs.RegisterSubSystem(make_uniq<MCPFileSystem>());
    
    // Register MCP storage extension for ATTACH support
    auto &config = DBConfig::GetConfig(db);
    config.storage_extensions["mcp"] = MCPStorageExtension::Create();
    
    // Register MCP configuration options
    config.AddExtensionOption("allowed_mcp_commands", 
        "Colon-delimited list of executable paths allowed for MCP servers (security: executable paths only, no arguments)", 
        LogicalType::VARCHAR, Value(""), SetAllowedMCPCommands);
    
    config.AddExtensionOption("allowed_mcp_urls", 
        "Space-delimited list of URL prefixes allowed for MCP servers", 
        LogicalType::VARCHAR, Value(""), SetAllowedMCPUrls);
    
    config.AddExtensionOption("mcp_server_file", 
        "Path to MCP server configuration file", 
        LogicalType::VARCHAR, Value("./.mcp.json"), SetMCPServerFile);
    
    config.AddExtensionOption("mcp_lock_servers", 
        "Lock MCP server configuration to prevent runtime changes (security feature)", 
        LogicalType::BOOLEAN, Value(false), SetMCPLockServers);
    
    config.AddExtensionOption("mcp_disable_serving", 
        "Disable MCP server functionality entirely (client-only mode)", 
        LogicalType::BOOLEAN, Value(false), SetMCPDisableServing);
    
    // Register MCP logging configuration options
    config.AddExtensionOption("mcp_log_level", 
        "MCP logging level (trace, debug, info, warn, error, off)", 
        LogicalType::VARCHAR, Value("warn"), SetMCPLogLevel);
    
    config.AddExtensionOption("mcp_log_file", 
        "Path to MCP log file (empty for no file logging)", 
        LogicalType::VARCHAR, Value(""), SetMCPLogFile);
    
    config.AddExtensionOption("mcp_console_logging", 
        "Enable MCP logging to console/stderr", 
        LogicalType::BOOLEAN, Value(false), SetMCPConsoleLogging);
    
    // Initialize default security settings
    auto &security = MCPSecurityConfig::GetInstance();
    // Set secure defaults - no commands or URLs allowed initially
    security.SetAllowedCommands("");
    security.SetAllowedUrls("");
    security.SetServerFile("./.mcp.json");
    security.LockServers(false);
    
    // Register MCP resource functions
    auto get_resource_func = ScalarFunction("mcp_get_resource", 
        {LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::JSON(), MCPGetResourceFunction);
    loader.RegisterFunction(get_resource_func);
    
    // Create overloaded versions for list_resources (with and without cursor)
    auto list_resources_func_simple = ScalarFunction("mcp_list_resources", 
        {LogicalType::VARCHAR}, LogicalType::JSON(), MCPListResourcesFunction);
    auto list_resources_func_cursor = ScalarFunction("mcp_list_resources", 
        {LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::JSON(), MCPListResourcesWithCursorFunction);
    
    loader.RegisterFunction(list_resources_func_simple);
    loader.RegisterFunction(list_resources_func_cursor);
    
    auto call_tool_func = ScalarFunction("mcp_call_tool", 
        {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::JSON(), MCPCallToolFunction);
    loader.RegisterFunction(call_tool_func);
    
    // Register MCP tool functions (with and without cursor)
    auto list_tools_func_simple = ScalarFunction("mcp_list_tools", 
        {LogicalType::VARCHAR}, LogicalType::JSON(), MCPListToolsFunction);
    auto list_tools_func_cursor = ScalarFunction("mcp_list_tools", 
        {LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::JSON(), MCPListToolsWithCursorFunction);
    loader.RegisterFunction(list_tools_func_simple);
    loader.RegisterFunction(list_tools_func_cursor);
    
    // Register MCP prompt functions (with and without cursor)
    auto list_prompts_func_simple = ScalarFunction("mcp_list_prompts", 
        {LogicalType::VARCHAR}, LogicalType::JSON(), MCPListPromptsFunction);
    auto list_prompts_func_cursor = ScalarFunction("mcp_list_prompts", 
        {LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::JSON(), MCPListPromptsWithCursorFunction);
    loader.RegisterFunction(list_prompts_func_simple);
    loader.RegisterFunction(list_prompts_func_cursor);
    
    auto get_prompt_func = ScalarFunction("mcp_get_prompt", 
        {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::JSON(), MCPGetPromptFunction);
    loader.RegisterFunction(get_prompt_func);
    
    // Register MCP connection management functions
    auto reconnect_func = ScalarFunction("mcp_reconnect_server", 
        {LogicalType::VARCHAR}, LogicalType::VARCHAR, MCPReconnectServerFunction);
    loader.RegisterFunction(reconnect_func);
    
    auto health_func = ScalarFunction("mcp_server_health", 
        {LogicalType::VARCHAR}, LogicalType::VARCHAR, MCPServerHealthFunction);
    loader.RegisterFunction(health_func);
    
    // Register MCP server functions (multiple overloads for convenience)
    // All server management functions return MCPStatus struct type
    LogicalType mcp_status_type = GetMCPStatusType();

    // mcp_server_start(transport) - simplest form for stdio
    auto server_start_simple_func = ScalarFunction("mcp_server_start",
        {LogicalType::VARCHAR},
        mcp_status_type, MCPServerStartSimpleFunction);
    loader.RegisterFunction(server_start_simple_func);

    // mcp_server_start(transport, config_json) - with config for stdio
    auto server_start_config_func = ScalarFunction("mcp_server_start",
        {LogicalType::VARCHAR, LogicalType::VARCHAR},
        mcp_status_type, MCPServerStartConfigFunction);
    loader.RegisterFunction(server_start_config_func);

    // mcp_server_start(transport, bind_address, port, config_json) - full form
    auto server_start_func = ScalarFunction("mcp_server_start",
        {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::INTEGER, LogicalType::VARCHAR},
        mcp_status_type, MCPServerStartFunction);
    loader.RegisterFunction(server_start_func);

    auto server_stop_func = ScalarFunction("mcp_server_stop",
        {}, mcp_status_type, MCPServerStopFunction);
    loader.RegisterFunction(server_stop_func);

    // mcp_server_stop(force) - with force option for test setup/teardown
    auto server_stop_force_func = ScalarFunction("mcp_server_stop",
        {LogicalType::BOOLEAN}, mcp_status_type, MCPServerStopForceFunction);
    loader.RegisterFunction(server_stop_force_func);

    auto server_status_func = ScalarFunction("mcp_server_status",
        {}, mcp_status_type, MCPServerStatusFunction);
    loader.RegisterFunction(server_status_func);

    // Register MCP server test function (for unit testing protocol handling)
    auto server_test_func = ScalarFunction("mcp_server_test",
        {LogicalType::VARCHAR}, LogicalType::VARCHAR, MCPServerTestFunction);
    loader.RegisterFunction(server_test_func);

    // Register MCP server send request function - sends request to running server
    // mcp_server_send_request(request_json) - requires server to be started first
    auto send_request_func = ScalarFunction("mcp_server_send_request",
        {LogicalType::VARCHAR}, LogicalType::VARCHAR, MCPServerSendRequestFunction);
    loader.RegisterFunction(send_request_func);

    // Register resource publishing functions
    auto publish_table_func = ScalarFunction("mcp_publish_table", 
        {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR}, 
        LogicalType::VARCHAR, MCPPublishTableFunction);
    loader.RegisterFunction(publish_table_func);
    
    auto publish_query_func = ScalarFunction("mcp_publish_query", 
        {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::INTEGER}, 
        LogicalType::VARCHAR, MCPPublishQueryFunction);
    loader.RegisterFunction(publish_query_func);
    
    // Register MCP template functions
    auto register_prompt_template_func = ScalarFunction("mcp_register_prompt_template",
        {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR}, 
        LogicalType::VARCHAR, MCPRegisterPromptTemplateFunction);
    loader.RegisterFunction(register_prompt_template_func);
    
    auto list_prompt_templates_func = ScalarFunction("mcp_list_prompt_templates",
        {}, LogicalType::JSON(), MCPListPromptTemplatesFunction);
    loader.RegisterFunction(list_prompt_templates_func);
    
    auto render_prompt_template_func = ScalarFunction("mcp_render_prompt_template",
        {LogicalType::VARCHAR, LogicalType::JSON()}, 
        LogicalType::VARCHAR, MCPRenderPromptTemplateFunction);
    loader.RegisterFunction(render_prompt_template_func);
    
    
    // Register MCP diagnostics functions
    auto diagnostics_func = ScalarFunction("mcp_get_diagnostics", 
        {}, LogicalType::JSON(), MCPGetDiagnosticsFunction);
    loader.RegisterFunction(diagnostics_func);
}

void DuckdbMcpExtension::Load(ExtensionLoader &loader) {
    LoadInternal(loader);
}

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(duckdb_mcp, loader) {
    duckdb::LoadInternal(loader);
}


DUCKDB_EXTENSION_API const char *duckdb_mcp_version() {
    return duckdb::DUCKDB_MCP_VERSION;
}

}

} // namespace duckdb
