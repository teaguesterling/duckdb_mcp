#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/extension/extension_loader.hpp"

namespace duckdb {

//! Register mcp_tools(), mcp_resources(), mcp_server_config(), and mcp_list_tools() table functions
void RegisterMCPStateTableFunctions(ExtensionLoader &loader);

} // namespace duckdb
