#pragma once

#include "duckdb.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"

namespace duckdb {

class DuckdbMcpExtension : public Extension {
public:
    void Load(DuckDB &db) override;
    std::string Name() override { return "duckdb_mcp"; }
};

// Function declarations
void ConfigureMCPFunction(DataChunk &args, ExpressionState &state, Vector &result);
void StartMCPFunction(DataChunk &args, ExpressionState &state, Vector &result);

} // namespace duckdb
