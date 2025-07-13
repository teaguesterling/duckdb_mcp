// src/duckdb_mcp_extension.cpp
#include "duckdb_mcp_extension.hpp"
#include "duckdb_mcp_config.hpp"
#include "mcpfs/mcp_file_system.hpp"
#include "client/mcp_storage_extension.hpp"
#include "protocol/mcp_connection.hpp"
#include "protocol/mcp_message.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/main/database.hpp"

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
            Value call_params = Value::STRUCT({
                {"name", Value(tool_name)},
                {"arguments", Value::STRUCT({})}  // TODO: Parse params_json properly
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

void DuckdbMcpExtension::Load(DuckDB &db) {
    // Register MCPFS file system
    auto &fs = FileSystem::GetFileSystem(*db.instance);
    fs.RegisterSubSystem(make_uniq<MCPFileSystem>());
    
    // Register MCP storage extension for ATTACH support
    auto &config = DBConfig::GetConfig(*db.instance);
    config.storage_extensions["mcp"] = MCPStorageExtension::Create();
    
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
    
    // Register configuration function
    auto config_func = ScalarFunction("mcp_configure", {LogicalType::VARCHAR}, LogicalType::VARCHAR, ConfigureMCPFunction);
    ExtensionUtil::RegisterFunction(*db.instance, config_func);
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
