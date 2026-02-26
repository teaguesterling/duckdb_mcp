#pragma once

#include <mutex>
#include <unordered_map>
#include "duckdb.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"

namespace duckdb {

// Configuration structure
struct MCPConfiguration {
	bool initialized = false;
	vector<string> include_schemas;
	vector<string> exclude_schemas;
	vector<string> include_tables;
	vector<string> exclude_tables;
	vector<string> include_macros;
	vector<string> exclude_macros;
	bool expose_system_tables = false;
	bool expose_temp_tables = true;
	bool expose_views = true;
	bool expose_macros = true;
	bool allow_write_queries = true;
	string transport = "stdio";
	unordered_map<string, Value> custom_settings;
	bool config_mode = false;
};

// Global configuration management
class MCPConfigManager {
private:
	static unordered_map<DatabaseInstance *, unique_ptr<MCPConfiguration>> configs;
	static mutex config_mutex;

public:
	static MCPConfiguration &GetConfig(DatabaseInstance &db);
	static void SetConfig(DatabaseInstance &db, unique_ptr<MCPConfiguration> config);
	static bool IsConfigMode(DatabaseInstance &db);
	static void SetConfigMode(DatabaseInstance &db, bool mode);
};

} // namespace duckdb
