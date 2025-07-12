// src/mcp_config.cpp
#include "duckdb_mcp_extension.hpp"
#include "duckdb_mcp_config.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {

// Static member definitions
unordered_map<DatabaseInstance*, unique_ptr<MCPConfiguration>> MCPConfigManager::configs;
mutex MCPConfigManager::config_mutex;

MCPConfiguration& MCPConfigManager::GetConfig(DatabaseInstance &db) {
    lock_guard<mutex> lock(config_mutex);
    
    auto it = configs.find(&db);
    if (it == configs.end()) {
        // Create default configuration - use make_uniq for DuckDB
        configs[&db] = make_uniq<MCPConfiguration>();
    }
    return *configs[&db];
}

void MCPConfigManager::SetConfig(DatabaseInstance &db, unique_ptr<MCPConfiguration> config) {
    lock_guard<mutex> lock(config_mutex);
    configs[&db] = std::move(config);
}

// Helper function to parse JSON arrays
static vector<string> ParseJSONArray(Connection &conn, const string &json_array) {
    vector<string> result;
    
    if (json_array.empty() || json_array == "null") {
        return result;
    }
    
    // Use DuckDB's JSON functions to parse array - format query directly
    string query = StringUtil::Format(
        "SELECT unnest(json_extract_string('%s', '$[*]'))",
        StringUtil::Replace(json_array, "'", "''").c_str()  // Escape single quotes
    );
    
    auto query_result = conn.Query(query);
    
    if (query_result->HasError()) {
        throw InvalidInputException("Failed to parse JSON array: " + query_result->GetError());
    }
    
    while (auto row = query_result->Fetch()) {
        if (!row->GetValue(0, 0).IsNull()) {
            result.push_back(row->GetValue(0, 0).ToString());
        }
    }
    
    return result;
}

void ConfigureMCPFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &context = state.GetContext();
    auto &db = DatabaseInstance::GetDatabase(context);
    
    // Get JSON configuration string
    auto &json_vector = args.data[0];
    auto json_str = json_vector.GetValue(0).ToString();
    
    // Create a connection for JSON parsing
    Connection conn(db);
    
    // Escape the JSON string for SQL
    string escaped_json = StringUtil::Replace(json_str, "'", "''");
    
    // Parse configuration using DuckDB's JSON functions
    string parse_query = StringUtil::Format(R"(
        WITH config AS (
            SELECT '%s' as json_data
        )
        SELECT 
            json_extract_string(json_data, '$.include_schemas') as include_schemas,
            json_extract_string(json_data, '$.exclude_schemas') as exclude_schemas,
            json_extract_string(json_data, '$.include_tables') as include_tables,
            json_extract_string(json_data, '$.exclude_tables') as exclude_tables,
            json_extract_string(json_data, '$.include_macros') as include_macros,
            json_extract_string(json_data, '$.exclude_macros') as exclude_macros,
            json_extract_string(json_data, '$.expose_system_tables') as expose_system_tables,
            json_extract_string(json_data, '$.expose_temp_tables') as expose_temp_tables,
            json_extract_string(json_data, '$.expose_views') as expose_views,
            json_extract_string(json_data, '$.expose_macros') as expose_macros,
            json_extract_string(json_data, '$.allow_write_queries') as allow_write_queries,
            json_extract_string(json_data, '$.transport') as transport
        FROM config
    )", escaped_json.c_str());
    
    auto parse_result = conn.Query(parse_query);
    
    if (parse_result->HasError()) {
        throw InvalidInputException("Invalid JSON configuration: " + parse_result->GetError());
    }
    
    // Create new configuration - use make_uniq
    auto new_config = make_uniq<MCPConfiguration>();
    
    if (parse_result->RowCount() > 0) {
        auto row = parse_result->Fetch();
        
        // Parse array fields
        if (!row->GetValue(0, 0).IsNull()) {
            new_config->include_schemas = ParseJSONArray(conn, row->GetValue(0, 0).ToString());
        }
        if (!row->GetValue(1, 0).IsNull()) {
            new_config->exclude_schemas = ParseJSONArray(conn, row->GetValue(1, 0).ToString());
        }
        if (!row->GetValue(2, 0).IsNull()) {
            new_config->include_tables = ParseJSONArray(conn, row->GetValue(2, 0).ToString());
        }
        if (!row->GetValue(3, 0).IsNull()) {
            new_config->exclude_tables = ParseJSONArray(conn, row->GetValue(3, 0).ToString());
        }
        if (!row->GetValue(4, 0).IsNull()) {
            new_config->include_macros = ParseJSONArray(conn, row->GetValue(4, 0).ToString());
        }
        if (!row->GetValue(5, 0).IsNull()) {
            new_config->exclude_macros = ParseJSONArray(conn, row->GetValue(5, 0).ToString());
        }
        
        // Parse boolean fields
        if (!row->GetValue(6, 0).IsNull()) {
            new_config->expose_system_tables = (row->GetValue(6, 0).ToString() == "true");
        }
        if (!row->GetValue(7, 0).IsNull()) {
            new_config->expose_temp_tables = (row->GetValue(7, 0).ToString() == "true");
        }
        if (!row->GetValue(8, 0).IsNull()) {
            new_config->expose_views = (row->GetValue(8, 0).ToString() == "true");
        }
        if (!row->GetValue(9, 0).IsNull()) {
            new_config->expose_macros = (row->GetValue(9, 0).ToString() == "true");
        }
        if (!row->GetValue(10, 0).IsNull()) {
            new_config->allow_write_queries = (row->GetValue(10, 0).ToString() == "true");
        }
        
        // Transport setting
        if (!row->GetValue(11, 0).IsNull()) {
            new_config->transport = row->GetValue(11, 0).ToString();
        }
    }
    
    new_config->initialized = true;
    
    // Store configuration
    MCPConfigManager::SetConfig(db, std::move(new_config));
    
    // Return success message
    result.SetVectorType(VectorType::CONSTANT_VECTOR);
    auto result_data = ConstantVector::GetData<string_t>(result);
    result_data[0] = StringVector::AddString(result, "MCP configuration updated");
}

} // namespace duckdb
