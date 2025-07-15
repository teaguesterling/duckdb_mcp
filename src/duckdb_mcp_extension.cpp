// src/duckdb_mcp_extension.cpp
#include "duckdb_mcp_extension.hpp"
#include "duckdb_mcp_config.hpp"
#include "duckdb_mcp_security.hpp"
#include <cstdlib>
#include "mcpfs/mcp_file_system.hpp"
#include "client/mcp_storage_extension.hpp"
#include "protocol/mcp_connection.hpp"
#include "protocol/mcp_message.hpp"
#include "server/mcp_server.hpp"
#include "server/resource_providers.hpp"
#include "server/tool_handlers.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/enums/set_scope.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
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

// Start MCP server with inline configuration
static void MCPServerStartFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &transport_vector = args.data[0];
    auto &bind_address_vector = args.data[1];
    auto &port_vector = args.data[2];
    auto &config_json_vector = args.data[3];
    
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    auto &result_validity = FlatVector::Validity(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        try {
            // Get database instance from state
            auto &context = state.GetContext();
            auto &db_instance = DatabaseInstance::GetDatabase(context);
            
            // Parse parameters
            string transport = transport_vector.GetValue(i).IsNull() ? "stdio" : transport_vector.GetValue(i).ToString();
            string bind_address = bind_address_vector.GetValue(i).IsNull() ? "localhost" : bind_address_vector.GetValue(i).ToString();
            int port = port_vector.GetValue(i).IsNull() ? 8080 : port_vector.GetValue(i).GetValue<int32_t>();
            string config_json = config_json_vector.GetValue(i).IsNull() ? "{}" : config_json_vector.GetValue(i).ToString();
            
            // Check if serving is disabled
            auto &security = MCPSecurityConfig::GetInstance();
            if (security.IsServingDisabled()) {
                result_data[i] = StringVector::AddString(result, "ERROR: MCP server functionality is disabled (mcp_disable_serving=true)");
                continue;
            }
            
            // Check if server is already running
            auto &server_manager = MCPServerManager::GetInstance();
            if (server_manager.IsServerRunning()) {
                result_data[i] = StringVector::AddString(result, "ERROR: MCP server is already running. Stop it first with mcp_server_stop()");
                continue;
            }
            
            // Create server configuration
            MCPServerConfig server_config;
            server_config.transport = transport;
            server_config.bind_address = bind_address;
            server_config.port = port;
            server_config.db_instance = &db_instance;
            
            // TODO: Parse config_json for additional settings like:
            // - enable_query_tool, enable_describe_tool, enable_export_tool
            // - allowed_queries, denied_queries
            // - max_connections, request_timeout_seconds
            // - require_auth, auth_token
            
            // For stdio transport, check if foreground mode is requested
            if (transport == "stdio") {
                // Check environment variable for foreground mode
                const char* foreground_env = std::getenv("DUCKDB_MCP_FOREGROUND");
                bool foreground_mode = foreground_env && (strcmp(foreground_env, "1") == 0);
                
                if (foreground_mode) {
                    // Foreground mode: handle connection directly without background thread
                    MCPServer server(server_config);
                    server.SetRunning(true); // Mark as running without starting background thread
                    try {
                        server.RunMainLoop(); // Blocks forever (should never return)
                        result_data[i] = StringVector::AddString(result, "ERROR: MCP server unexpectedly returned");
                    } catch (const std::exception &e) {
                        result_data[i] = StringVector::AddString(result, "ERROR: " + string(e.what()));
                    }
                } else {
                    // Background mode: use server manager (standard pattern)
                    if (server_manager.StartServer(server_config)) {
                        result_data[i] = StringVector::AddString(result, "SUCCESS: MCP server started on stdio (background mode)");
                    } else {
                        result_data[i] = StringVector::AddString(result, "ERROR: Failed to start MCP server");
                    }
                }
            } else {
                // For non-stdio transports, use background thread as before
                if (server_manager.StartServer(server_config)) {
                    string success_msg = "SUCCESS: MCP server started on " + transport;
                    success_msg += " at " + bind_address + ":" + std::to_string(port);
                    result_data[i] = StringVector::AddString(result, success_msg);
                } else {
                    result_data[i] = StringVector::AddString(result, "ERROR: Failed to start MCP server");
                }
            }
            
        } catch (const std::exception &e) {
            result_data[i] = StringVector::AddString(result, "ERROR: " + string(e.what()));
        }
    }
}

// Stop MCP server
static void MCPServerStopFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        try {
            auto &server_manager = MCPServerManager::GetInstance();
            
            if (!server_manager.IsServerRunning()) {
                result_data[i] = StringVector::AddString(result, "ERROR: MCP server is not running");
                continue;
            }
            
            server_manager.StopServer();
            result_data[i] = StringVector::AddString(result, "SUCCESS: MCP server stopped");
            
        } catch (const std::exception &e) {
            result_data[i] = StringVector::AddString(result, "ERROR: " + string(e.what()));
        }
    }
}

// Get MCP server status
static void MCPServerStatusFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    result.SetVectorType(VectorType::FLAT_VECTOR);
    auto result_data = FlatVector::GetData<string_t>(result);
    
    for (idx_t i = 0; i < args.size(); i++) {
        try {
            auto &server_manager = MCPServerManager::GetInstance();
            
            if (!server_manager.IsServerRunning()) {
                result_data[i] = StringVector::AddString(result, "Server Status: STOPPED");
                continue;
            }
            
            auto server = server_manager.GetServer();
            if (server) {
                string status = server->GetStatus();
                result_data[i] = StringVector::AddString(result, status);
            } else {
                result_data[i] = StringVector::AddString(result, "ERROR: Server manager inconsistency");
            }
            
        } catch (const std::exception &e) {
            result_data[i] = StringVector::AddString(result, "ERROR: " + string(e.what()));
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

// Callback functions for MCP configuration settings
static void SetAllowedMCPCommands(ClientContext &context, SetScope scope, Value &parameter) {
    auto &security = MCPSecurityConfig::GetInstance();
    security.SetAllowedCommands(parameter.ToString());
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

void DuckdbMcpExtension::Load(DuckDB &db) {
    // Register MCPFS file system
    auto &fs = FileSystem::GetFileSystem(*db.instance);
    fs.RegisterSubSystem(make_uniq<MCPFileSystem>());
    
    // Register MCP storage extension for ATTACH support
    auto &config = DBConfig::GetConfig(*db.instance);
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
    ExtensionUtil::RegisterFunction(*db.instance, get_resource_func);
    
    auto list_resources_func = ScalarFunction("mcp_list_resources", 
        {LogicalType::VARCHAR}, LogicalType::JSON(), MCPListResourcesFunction);
    ExtensionUtil::RegisterFunction(*db.instance, list_resources_func);
    
    auto call_tool_func = ScalarFunction("mcp_call_tool", 
        {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::JSON(), MCPCallToolFunction);
    ExtensionUtil::RegisterFunction(*db.instance, call_tool_func);
    
    // Register MCP tool functions
    auto list_tools_func = ScalarFunction("mcp_list_tools", 
        {LogicalType::VARCHAR}, LogicalType::JSON(), MCPListToolsFunction);
    ExtensionUtil::RegisterFunction(*db.instance, list_tools_func);
    
    // Register MCP prompt functions
    auto list_prompts_func = ScalarFunction("mcp_list_prompts", 
        {LogicalType::VARCHAR}, LogicalType::JSON(), MCPListPromptsFunction);
    ExtensionUtil::RegisterFunction(*db.instance, list_prompts_func);
    
    auto get_prompt_func = ScalarFunction("mcp_get_prompt", 
        {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR}, LogicalType::JSON(), MCPGetPromptFunction);
    ExtensionUtil::RegisterFunction(*db.instance, get_prompt_func);
    
    // Register MCP connection management functions
    auto reconnect_func = ScalarFunction("mcp_reconnect_server", 
        {LogicalType::VARCHAR}, LogicalType::VARCHAR, MCPReconnectServerFunction);
    ExtensionUtil::RegisterFunction(*db.instance, reconnect_func);
    
    auto health_func = ScalarFunction("mcp_server_health", 
        {LogicalType::VARCHAR}, LogicalType::VARCHAR, MCPServerHealthFunction);
    ExtensionUtil::RegisterFunction(*db.instance, health_func);
    
    // Register MCP server functions
    auto server_start_func = ScalarFunction("mcp_server_start", 
        {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::INTEGER, LogicalType::VARCHAR}, 
        LogicalType::VARCHAR, MCPServerStartFunction);
    ExtensionUtil::RegisterFunction(*db.instance, server_start_func);
    
    auto server_stop_func = ScalarFunction("mcp_server_stop", 
        {}, LogicalType::VARCHAR, MCPServerStopFunction);
    ExtensionUtil::RegisterFunction(*db.instance, server_stop_func);
    
    auto server_status_func = ScalarFunction("mcp_server_status", 
        {}, LogicalType::VARCHAR, MCPServerStatusFunction);
    ExtensionUtil::RegisterFunction(*db.instance, server_status_func);
    
    // Register resource publishing functions
    auto publish_table_func = ScalarFunction("mcp_publish_table", 
        {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR}, 
        LogicalType::VARCHAR, MCPPublishTableFunction);
    ExtensionUtil::RegisterFunction(*db.instance, publish_table_func);
    
    auto publish_query_func = ScalarFunction("mcp_publish_query", 
        {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::INTEGER}, 
        LogicalType::VARCHAR, MCPPublishQueryFunction);
    ExtensionUtil::RegisterFunction(*db.instance, publish_query_func);
}

extern "C" {

DUCKDB_EXTENSION_API void duckdb_mcp_init(duckdb::DatabaseInstance &db) {
    duckdb::DuckDB db_wrapper(db);
    db_wrapper.LoadExtension<DuckdbMcpExtension>();
}

DUCKDB_EXTENSION_API const char *duckdb_mcp_version() {
    return duckdb::DuckDB::LibraryVersion();
}

}

} // namespace duckdb
