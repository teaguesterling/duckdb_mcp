// src/duckdb_mcp_extension.cpp
#include "duckdb_mcp_extension.hpp"
#include "duckdb_mcp_config.hpp"
#include "mcpfs/mcp_file_system.hpp"
#include "client/mcp_storage_extension.hpp"
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
