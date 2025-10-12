#pragma once

#include "duckdb.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"

namespace duckdb {

class DuckdbMcpExtension : public Extension {
public:
    void Load(ExtensionLoader &loader) override;
    std::string Name() override { return "duckdb_mcp"; }
    std::string Version() const override { return "1.0.0"; }
};

// Function declarations for any non-static functions if needed

} // namespace duckdb
