#include "server/mcp_state_table_functions.hpp"
#include "mcp_instance_state.hpp"
#include "duckdb_compat.hpp"
#include "duckdb/function/table_function.hpp"

namespace duckdb {

//===--------------------------------------------------------------------===//
// mcp_tools() table function
//===--------------------------------------------------------------------===//

struct MCPToolsData : public GlobalTableFunctionState {
	vector<ToolMetadataEntry> entries;
	idx_t offset = 0;
};

static unique_ptr<FunctionData> MCPToolsBind(ClientContext &context, TableFunctionBindInput &input,
                                             vector<LogicalType> &return_types, vector<string> &names) {
	names.emplace_back("name");
	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("description");
	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("sql_template");
	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("parameters");
	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("required");
	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("format");
	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("status");
	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("is_builtin");
	return_types.emplace_back(LogicalType::BOOLEAN);
	return nullptr;
}

static unique_ptr<GlobalTableFunctionState> MCPToolsInit(ClientContext &context, TableFunctionInitInput &input) {
	auto result = make_uniq<MCPToolsData>();
	auto &state = MCPInstanceState::Get(context);
	result->entries = state.server_manager.GetToolSnapshot();
	return std::move(result);
}

static void MCPToolsScan(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.global_state->Cast<MCPToolsData>();
	idx_t count = 0;
	while (data.offset < data.entries.size() && count < STANDARD_VECTOR_SIZE) {
		auto &entry = data.entries[data.offset];
		output.SetValue(0, count, Value(entry.name));
		output.SetValue(1, count, Value(entry.description));
		output.SetValue(2, count, entry.sql_template.empty() ? Value(LogicalType::VARCHAR) : Value(entry.sql_template));
		output.SetValue(3, count,
		                entry.parameters_json.empty() ? Value(LogicalType::VARCHAR) : Value(entry.parameters_json));
		output.SetValue(4, count,
		                entry.required_json.empty() ? Value(LogicalType::VARCHAR) : Value(entry.required_json));
		output.SetValue(5, count, entry.format.empty() ? Value(LogicalType::VARCHAR) : Value(entry.format));
		output.SetValue(6, count, Value(entry.status));
		output.SetValue(7, count, Value::BOOLEAN(entry.is_builtin));
		count++;
		data.offset++;
	}
	CompatSetOutputCardinality(output, count);
}

//===--------------------------------------------------------------------===//
// mcp_resources() table function
//===--------------------------------------------------------------------===//

struct MCPResourcesData : public GlobalTableFunctionState {
	vector<ResourceMetadataEntry> entries;
	idx_t offset = 0;
};

static unique_ptr<FunctionData> MCPResourcesBind(ClientContext &context, TableFunctionBindInput &input,
                                                 vector<LogicalType> &return_types, vector<string> &names) {
	names.emplace_back("uri");
	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("type");
	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("description");
	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("mime_type");
	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("source");
	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("format");
	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("status");
	return_types.emplace_back(LogicalType::VARCHAR);
	return nullptr;
}

static unique_ptr<GlobalTableFunctionState> MCPResourcesInit(ClientContext &context, TableFunctionInitInput &input) {
	auto result = make_uniq<MCPResourcesData>();
	auto &state = MCPInstanceState::Get(context);
	result->entries = state.server_manager.GetResourceSnapshot();
	return std::move(result);
}

static void MCPResourcesScan(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.global_state->Cast<MCPResourcesData>();
	idx_t count = 0;
	while (data.offset < data.entries.size() && count < STANDARD_VECTOR_SIZE) {
		auto &entry = data.entries[data.offset];
		output.SetValue(0, count, Value(entry.uri));
		output.SetValue(1, count, entry.type.empty() ? Value(LogicalType::VARCHAR) : Value(entry.type));
		output.SetValue(2, count, entry.description.empty() ? Value(LogicalType::VARCHAR) : Value(entry.description));
		output.SetValue(3, count, entry.mime_type.empty() ? Value(LogicalType::VARCHAR) : Value(entry.mime_type));
		output.SetValue(4, count, entry.source.empty() ? Value(LogicalType::VARCHAR) : Value(entry.source));
		output.SetValue(5, count, entry.format.empty() ? Value(LogicalType::VARCHAR) : Value(entry.format));
		output.SetValue(6, count, Value(entry.status));
		count++;
		data.offset++;
	}
	CompatSetOutputCardinality(output, count);
}

//===--------------------------------------------------------------------===//
// mcp_server_config() table function
//===--------------------------------------------------------------------===//

struct MCPServerConfigData : public GlobalTableFunctionState {
	vector<pair<string, string>> entries;
	idx_t offset = 0;
};

static vector<pair<string, string>> ConfigToKVPairs(const MCPServerConfig &config, bool has_config) {
	vector<pair<string, string>> pairs;
	if (!has_config) {
		return pairs;
	}

	pairs.emplace_back("transport", config.transport);
	pairs.emplace_back("bind_address", config.bind_address);
	pairs.emplace_back("port", to_string(config.port));
	pairs.emplace_back("auth_token", config.auth_token.empty() ? "" : "****");
	pairs.emplace_back("ssl_cert_path", config.ssl_cert_path);
	pairs.emplace_back("ssl_key_path", config.ssl_key_path.empty() ? "" : "****");
	pairs.emplace_back("enable_query_tool", config.enable_query_tool ? "true" : "false");
	pairs.emplace_back("enable_describe_tool", config.enable_describe_tool ? "true" : "false");
	pairs.emplace_back("enable_export_tool", config.enable_export_tool ? "true" : "false");
	pairs.emplace_back("export_allow_file_output", config.export_allow_file_output ? "true" : "false");
	pairs.emplace_back("enable_list_tables_tool", config.enable_list_tables_tool ? "true" : "false");
	pairs.emplace_back("enable_database_info_tool", config.enable_database_info_tool ? "true" : "false");
	pairs.emplace_back("enable_execute_tool", config.enable_execute_tool ? "true" : "false");
	pairs.emplace_back("execute_allow_ddl", config.execute_allow_ddl ? "true" : "false");
	pairs.emplace_back("execute_allow_dml", config.execute_allow_dml ? "true" : "false");
	pairs.emplace_back("execute_allow_load", config.execute_allow_load ? "true" : "false");
	pairs.emplace_back("execute_allow_attach", config.execute_allow_attach ? "true" : "false");
	pairs.emplace_back("execute_allow_set", config.execute_allow_set ? "true" : "false");
	pairs.emplace_back("cors_origins", config.cors_origins);
	pairs.emplace_back("enable_health_endpoint", config.enable_health_endpoint ? "true" : "false");
	pairs.emplace_back("auth_health_endpoint", config.auth_health_endpoint ? "true" : "false");
	pairs.emplace_back("default_result_format", config.default_result_format);
	pairs.emplace_back("max_connections", to_string(config.max_connections));
	pairs.emplace_back("request_timeout_seconds", to_string(config.request_timeout_seconds));
	pairs.emplace_back("max_requests", to_string(config.max_requests));
	pairs.emplace_back("require_auth", config.require_auth ? "true" : "false");
	pairs.emplace_back("allow_direct_requests", config.allow_direct_requests ? "true" : "false");

	return pairs;
}

static unique_ptr<FunctionData> MCPServerConfigBind(ClientContext &context, TableFunctionBindInput &input,
                                                    vector<LogicalType> &return_types, vector<string> &names) {
	names.emplace_back("key");
	return_types.emplace_back(LogicalType::VARCHAR);
	names.emplace_back("value");
	return_types.emplace_back(LogicalType::VARCHAR);
	return nullptr;
}

static unique_ptr<GlobalTableFunctionState> MCPServerConfigInit(ClientContext &context, TableFunctionInitInput &input) {
	auto result = make_uniq<MCPServerConfigData>();
	auto &state = MCPInstanceState::Get(context);
	auto config = state.server_manager.GetServerConfigSnapshot();
	bool has_config = state.server_manager.HasServerConfig();
	result->entries = ConfigToKVPairs(config, has_config);
	return std::move(result);
}

static void MCPServerConfigScan(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.global_state->Cast<MCPServerConfigData>();
	idx_t count = 0;
	while (data.offset < data.entries.size() && count < STANDARD_VECTOR_SIZE) {
		auto &entry = data.entries[data.offset];
		output.SetValue(0, count, Value(entry.first));
		output.SetValue(1, count, Value(entry.second));
		count++;
		data.offset++;
	}
	CompatSetOutputCardinality(output, count);
}

//===--------------------------------------------------------------------===//
// Registration
//===--------------------------------------------------------------------===//

void RegisterMCPStateTableFunctions(ExtensionLoader &loader) {
	// mcp_tools()
	TableFunction mcp_tools("mcp_tools", {}, MCPToolsScan);
	mcp_tools.bind = MCPToolsBind;
	mcp_tools.init_global = MCPToolsInit;
	loader.RegisterFunction(mcp_tools);

	// mcp_list_tools() — alias for mcp_tools()
	TableFunction mcp_list_tools("mcp_list_tools", {}, MCPToolsScan);
	mcp_list_tools.bind = MCPToolsBind;
	mcp_list_tools.init_global = MCPToolsInit;
	loader.RegisterFunction(mcp_list_tools);

	// mcp_resources()
	TableFunction mcp_resources("mcp_resources", {}, MCPResourcesScan);
	mcp_resources.bind = MCPResourcesBind;
	mcp_resources.init_global = MCPResourcesInit;
	loader.RegisterFunction(mcp_resources);

	// mcp_server_config()
	TableFunction mcp_server_config("mcp_server_config", {}, MCPServerConfigScan);
	mcp_server_config.bind = MCPServerConfigBind;
	mcp_server_config.init_global = MCPServerConfigInit;
	loader.RegisterFunction(mcp_server_config);
}

} // namespace duckdb
