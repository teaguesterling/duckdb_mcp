#pragma once

#include "duckdb.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"

namespace duckdb {

// Extension version constant - update this when releasing new versions
constexpr const char *DUCKDB_MCP_VERSION = "1.2.1";

class DuckdbMcpExtension : public Extension {
public:
    void Load(ExtensionLoader &loader) override;
    std::string Name() override { return "duckdb_mcp"; }
    std::string Version() const override { return DUCKDB_MCP_VERSION; }
};

// Function declarations for any non-static functions if needed

} // namespace duckdb
