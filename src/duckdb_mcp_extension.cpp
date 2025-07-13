// src/duckdb_mcp_extension.cpp
#include "duckdb_mcp_extension.hpp"
#include "duckdb_mcp_config.hpp"
#include "duckdb_mcp_security.hpp"
#include "mcpfs/mcp_file_system.hpp"
#include "client/mcp_storage_extension.hpp"
#include "protocol/mcp_connection.hpp"
#include "protocol/mcp_message.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/enums/set_scope.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/config.hpp"

namespace duckdb {

// Simple test function to verify extension loads
static void HelloMCPFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    result.SetVectorType(VectorType::CONSTANT_VECTOR);
    auto result_data = ConstantVector::GetData<string_t>(result);
    result_data[0] = StringVector::AddString(result, "Hello from DuckDB MCP!");
}

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
    
    // Register test function
    auto hello_func = ScalarFunction("hello_mcp", {}, LogicalType::VARCHAR, HelloMCPFunction);
    ExtensionUtil::RegisterFunction(*db.instance, hello_func);
    
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
