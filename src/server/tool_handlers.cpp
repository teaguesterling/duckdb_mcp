#include "server/tool_handlers.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/parser/keyword_helper.hpp"
#include "json_utils.hpp"
#include "result_formatter.hpp"
#include <cctype>

namespace duckdb {

// Escape a string for safe inclusion in a JSON string value.
// Handles quotes, backslashes, and control characters.
static string EscapeJsonString(const string &input) {
	string result;
	result.reserve(input.size());
	for (char c : input) {
		switch (c) {
		case '"':
			result += "\\\"";
			break;
		case '\\':
			result += "\\\\";
			break;
		case '\n':
			result += "\\n";
			break;
		case '\r':
			result += "\\r";
			break;
		case '\t':
			result += "\\t";
			break;
		default:
			if (static_cast<unsigned char>(c) < 0x20) {
				// Control character - encode as \u00XX
				char buf[8];
				snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
				result += buf;
			} else {
				result += c;
			}
			break;
		}
	}
	return result;
}

// Escape single quotes in a string for SQL string literals
static string EscapeSQLString(const string &input) {
	return StringUtil::Replace(input, "'", "''");
}

//===--------------------------------------------------------------------===//
// Shared Query Type Checking
//===--------------------------------------------------------------------===//

bool IsQueryAllowedByType(DatabaseInstance &db, const string &query,
                          const vector<string> &allowed_types,
                          const vector<string> &denied_types) {
	// If no restrictions configured, allow everything
	if (allowed_types.empty() && denied_types.empty()) {
		return true;
	}

	// Parse the query to get its statement type
	Connection conn(db);
	auto prepared = conn.Prepare(query);
	if (prepared->HasError()) {
		return false; // Fail closed: unparseable queries are denied
	}

	string type_name = StringUtil::Upper(StatementTypeToString(prepared->GetStatementType()));

	// Check denylist first (exact type match)
	for (const auto &denied : denied_types) {
		if (type_name == StringUtil::Upper(denied)) {
			return false;
		}
	}

	// Check allowlist if it exists (exact type match)
	if (!allowed_types.empty()) {
		for (const auto &allowed : allowed_types) {
			if (type_name == StringUtil::Upper(allowed)) {
				return true;
			}
		}
		return false; // Not in allowlist
	}

	return true; // No allowlist restriction, and not in denylist
}

//===--------------------------------------------------------------------===//
// ToolInputSchema Implementation
//===--------------------------------------------------------------------===//

bool ToolInputSchema::ValidateInput(const Value &input) const {
	// Basic validation - check if required fields are present
	if (input.type().id() != LogicalTypeId::STRUCT) {
		return false;
	}

	auto &struct_values = StructValue::GetChildren(input);
	unordered_set<string> provided_fields;

	for (size_t i = 0; i < struct_values.size(); i++) {
		auto &key = StructType::GetChildName(input.type(), i);
		provided_fields.insert(key);
	}

	// Check if all required fields are provided
	for (const auto &required_field : required_fields) {
		if (provided_fields.find(required_field) == provided_fields.end()) {
			return false;
		}
	}

	return true;
}

Value ToolInputSchema::ToJSON() const {
	// Create properties as a STRUCT (object) with property names as keys
	// This matches JSON Schema format expected by MCP
	child_list_t<Value> prop_entries;
	for (const auto &prop : properties) {
		// Each property value is a schema object with "type" field
		prop_entries.push_back(make_pair(prop.first, Value::STRUCT({{"type", prop.second}})));
	}

	// Create the properties object (empty struct if no properties)
	Value props_obj;
	if (prop_entries.empty()) {
		props_obj = Value::STRUCT(child_list_t<Value>());
	} else {
		props_obj = Value::STRUCT(prop_entries);
	}

	return Value::STRUCT({{"type", Value(type)},
	                      {"properties", props_obj},
	                      {"required", Value::LIST(LogicalType::VARCHAR,
	                                               vector<Value>(required_fields.begin(), required_fields.end()))}});
}

ToolInputSchema ParseToolInputSchema(const string &properties_json, const string &required_json) {
	ToolInputSchema schema;
	schema.type = "object";

	// Parse properties JSON: {"param_name": "type", ...} or {"param_name": {"type": "...", ...}, ...}
	if (!properties_json.empty() && properties_json != "{}") {
		yyjson_doc *props_doc = JSONUtils::Parse(properties_json);
		yyjson_val *props_root = yyjson_doc_get_root(props_doc);
		if (props_root && yyjson_is_obj(props_root)) {
			yyjson_obj_iter iter = yyjson_obj_iter_with(props_root);
			yyjson_val *key;
			while ((key = yyjson_obj_iter_next(&iter))) {
				yyjson_val *val = yyjson_obj_iter_get_val(key);
				string prop_name = yyjson_get_str(key);
				if (yyjson_is_str(val)) {
					// Simple format: {"param": "type"}
					string prop_type = yyjson_get_str(val);
					schema.properties[prop_name] = Value(prop_type);
				} else if (yyjson_is_obj(val)) {
					// Full JSON Schema format: {"param": {"type": "string", "description": "..."}}
					// Extract just the type for our internal schema
					yyjson_val *type_val = yyjson_obj_get(val, "type");
					if (type_val && yyjson_is_str(type_val)) {
						schema.properties[prop_name] = Value(yyjson_get_str(type_val));
					}
				}
			}
		}
		JSONUtils::FreeDocument(props_doc);
	}

	// Parse required JSON: ["param1", "param2", ...]
	if (!required_json.empty() && required_json != "[]") {
		yyjson_doc *req_doc = JSONUtils::Parse(required_json);
		yyjson_val *req_root = yyjson_doc_get_root(req_doc);
		if (req_root && yyjson_is_arr(req_root)) {
			size_t idx, max;
			yyjson_val *val;
			yyjson_arr_foreach(req_root, idx, max, val) {
				if (yyjson_is_str(val)) {
					schema.required_fields.push_back(yyjson_get_str(val));
				}
			}
		}
		JSONUtils::FreeDocument(req_doc);
	}

	return schema;
}

//===--------------------------------------------------------------------===//
// QueryToolHandler Implementation
//===--------------------------------------------------------------------===//

QueryToolHandler::QueryToolHandler(DatabaseInstance &db, const vector<string> &allowed_queries,
                                   const vector<string> &denied_queries, const string &default_format)
    : db_instance(db), allowed_queries(allowed_queries), denied_queries(denied_queries),
      default_result_format(default_format) {
}

CallToolResult QueryToolHandler::Execute(const Value &arguments) {
	try {
		// Parse JSON arguments (accepts both VARCHAR JSON and STRUCT)
		JSONArgumentParser parser;
		if (!parser.Parse(arguments)) {
			return CallToolResult::Error("Invalid input: failed to parse arguments");
		}

		// Validate required fields
		if (!parser.ValidateRequired({"sql"})) {
			return CallToolResult::Error("Invalid input: missing required field 'sql'");
		}

		// Extract parameters
		string sql = parser.GetString("sql");
		string format = parser.GetString("format", default_result_format);

		if (sql.empty()) {
			return CallToolResult::Error("SQL query is required");
		}

		// Validate format
		if (format != "json" && format != "csv" && format != "markdown") {
			return CallToolResult::Error("Unsupported format '" + format + "'. Supported formats: json, markdown, csv");
		}

		// Security check: parse query and validate statement type
		if (!IsQueryAllowedByType(db_instance, sql, allowed_queries, denied_queries)) {
			return CallToolResult::Error("Query not allowed by security policy");
		}

		// Execute query
		Connection conn(db_instance);
		auto result = conn.Query(sql);

		if (result->HasError()) {
			return CallToolResult::Error("SQL error: " + result->GetError());
		}

		// Format result
		string formatted_result = FormatResult(*result, format);
		return CallToolResult::Success(Value(formatted_result));

	} catch (const std::exception &e) {
		return CallToolResult::Error("Execution error: " + string(e.what()));
	}
}

ToolInputSchema QueryToolHandler::GetInputSchema() const {
	ToolInputSchema schema;
	schema.type = "object";
	schema.properties["sql"] = Value("string");
	schema.properties["format"] = Value("string");
	schema.required_fields = {"sql"};
	return schema;
}

string QueryToolHandler::FormatResult(QueryResult &result, const string &format) const {
	// Delegate to shared ResultFormatter utility
	return ResultFormatter::Format(result, format);
}

//===--------------------------------------------------------------------===//
// DescribeToolHandler Implementation
//===--------------------------------------------------------------------===//

DescribeToolHandler::DescribeToolHandler(DatabaseInstance &db, const vector<string> &allowed_queries,
                                         const vector<string> &denied_queries)
    : db_instance(db), allowed_queries(allowed_queries), denied_queries(denied_queries) {
}

CallToolResult DescribeToolHandler::Execute(const Value &arguments) {
	try {
		// Parse JSON arguments (accepts both VARCHAR JSON and STRUCT)
		JSONArgumentParser parser;
		if (!parser.Parse(arguments)) {
			return CallToolResult::Error("Invalid input: failed to parse arguments");
		}

		// Extract parameters
		string table_name = parser.GetString("table");
		string query = parser.GetString("query");

		Value result;
		if (!table_name.empty()) {
			result = DescribeTable(table_name);
		} else if (!query.empty()) {
			result = DescribeQuery(query);
		} else {
			return CallToolResult::Error("Either 'table' or 'query' parameter is required");
		}

		return CallToolResult::Success(result);

	} catch (const std::exception &e) {
		return CallToolResult::Error("Describe error: " + string(e.what()));
	}
}

ToolInputSchema DescribeToolHandler::GetInputSchema() const {
	ToolInputSchema schema;
	schema.type = "object";
	schema.properties["table"] = Value("string");
	schema.properties["query"] = Value("string");
	// No required fields - either table or query is needed
	return schema;
}

Value DescribeToolHandler::DescribeTable(const string &table_name) const {
	// Quote table name as an identifier to prevent SQL injection
	string quoted_table = KeywordHelper::WriteOptionallyQuoted(table_name);
	string describe_query = "DESCRIBE " + quoted_table;
	Connection conn(db_instance);
	auto result = conn.Query(describe_query);

	if (result->HasError()) {
		throw IOException("Failed to describe table: " + result->GetError());
	}

	// Build JSON directly to avoid STRUCT type issues
	// Escape user-supplied table name for safe JSON inclusion
	string json = "{\"table\":\"" + EscapeJsonString(table_name) + "\",\"columns\":[";
	bool first_col = true;

	while (auto chunk = result->Fetch()) {
		for (idx_t i = 0; i < chunk->size(); i++) {
			if (!first_col) {
				json += ",";
			}
			first_col = false;

			json += "{";
			json += "\"name\":\"" + EscapeJsonString(chunk->GetValue(0, i).ToString()) + "\",";
			json += "\"type\":\"" + EscapeJsonString(chunk->GetValue(1, i).ToString()) + "\",";
			json += "\"null\":\"" + EscapeJsonString(chunk->GetValue(2, i).ToString()) + "\",";
			json += "\"key\":\"" + EscapeJsonString(chunk->GetValue(3, i).ToString()) + "\",";
			json += "\"default\":" +
			        (chunk->GetValue(4, i).IsNull() ? "null"
			                                        : "\"" + EscapeJsonString(chunk->GetValue(4, i).ToString()) + "\"") +
			        ",";
			json += "\"extra\":" +
			        (chunk->GetValue(5, i).IsNull() ? "null"
			                                        : "\"" + EscapeJsonString(chunk->GetValue(5, i).ToString()) + "\"");
			json += "}";
		}
	}
	json += "]}";

	return Value(json);
}

Value DescribeToolHandler::DescribeQuery(const string &query) const {
	// Security check: parse query and validate statement type
	if (!IsQueryAllowedByType(db_instance, query, allowed_queries, denied_queries)) {
		throw InvalidInputException("Query not allowed by security policy");
	}

	// Use DESCRIBE with subquery syntax - wrap query in parentheses
	string describe_query = "DESCRIBE (" + query + ")";
	Connection conn(db_instance);
	auto describe_result = conn.Query(describe_query);

	if (describe_result->HasError()) {
		throw IOException("Failed to describe query: " + describe_result->GetError());
	}

	// Build JSON directly to avoid STRUCT type issues
	// Escape user-supplied query for safe JSON inclusion
	string json = "{\"query\":\"" + EscapeJsonString(query) + "\",\"columns\":[";
	bool first_col = true;

	while (auto chunk = describe_result->Fetch()) {
		for (idx_t i = 0; i < chunk->size(); i++) {
			if (!first_col) {
				json += ",";
			}
			first_col = false;

			json += "{";
			json += "\"name\":\"" + EscapeJsonString(chunk->GetValue(0, i).ToString()) + "\",";
			json += "\"type\":\"" + EscapeJsonString(chunk->GetValue(1, i).ToString()) + "\"";
			json += "}";
		}
	}
	json += "]}";

	return Value(json);
}

//===--------------------------------------------------------------------===//
// ExportToolHandler Implementation
//===--------------------------------------------------------------------===//

ExportToolHandler::ExportToolHandler(DatabaseInstance &db, const vector<string> &allowed_queries,
                                     const vector<string> &denied_queries)
    : db_instance(db), allowed_queries(allowed_queries), denied_queries(denied_queries) {
}

CallToolResult ExportToolHandler::Execute(const Value &arguments) {
	try {
		// Parse JSON arguments (accepts both VARCHAR JSON and STRUCT)
		JSONArgumentParser parser;
		if (!parser.Parse(arguments)) {
			return CallToolResult::Error("Invalid input: failed to parse arguments");
		}

		// Validate required fields
		if (!parser.ValidateRequired({"query"})) {
			return CallToolResult::Error("Invalid input: missing required field 'query'");
		}

		// Extract parameters
		string query = parser.GetString("query");
		string format = parser.GetString("format", "csv");
		string output_path = parser.GetString("output");

		if (query.empty()) {
			return CallToolResult::Error("Query is required");
		}

		// Validate format based on output mode
		if (output_path.empty()) {
			// Inline return - only json and csv supported
			if (format != "json" && format != "csv") {
				return CallToolResult::Error("Unsupported format '" + format +
				                             "' for inline return. Supported formats: json, csv");
			}
		} else {
			// File export - json, csv, and parquet supported
			if (format != "json" && format != "csv" && format != "parquet") {
				return CallToolResult::Error("Unsupported format '" + format +
				                             "' for file export. Supported formats: json, csv, parquet");
			}
		}

		// Security check: parse query and validate statement type
		if (!IsQueryAllowedByType(db_instance, query, allowed_queries, denied_queries)) {
			return CallToolResult::Error("Query not allowed by security policy");
		}

		// Execute query
		Connection conn(db_instance);
		auto result = conn.Query(query);

		if (result->HasError()) {
			return CallToolResult::Error("Query error: " + result->GetError());
		}

		if (!output_path.empty()) {
			// Export to file
			if (ExportToFile(*result, format, output_path, query)) {
				return CallToolResult::Success(Value("Data exported to " + output_path));
			} else {
				return CallToolResult::Error("Failed to export to file");
			}
		} else {
			// Return formatted data
			string formatted_data = FormatData(*result, format);
			return CallToolResult::Success(Value(formatted_data));
		}

	} catch (const std::exception &e) {
		return CallToolResult::Error("Export error: " + string(e.what()));
	}
}

ToolInputSchema ExportToolHandler::GetInputSchema() const {
	ToolInputSchema schema;
	schema.type = "object";
	schema.properties["query"] = Value("string");
	schema.properties["format"] = Value("string");
	schema.properties["output"] = Value("string");
	schema.required_fields = {"query"};
	return schema;
}

bool ExportToolHandler::ExportToFile(QueryResult &result, const string &format, const string &output_path,
                                     const string &query) const {
	try {
		// Escape single quotes in the output path to prevent SQL injection
		string safe_path = EscapeSQLString(output_path);

		// Use DuckDB's COPY TO functionality
		string copy_query;
		if (format == "csv") {
			copy_query = "COPY (" + query + ") TO '" + safe_path + "' (FORMAT CSV, HEADER)";
		} else if (format == "json") {
			copy_query = "COPY (" + query + ") TO '" + safe_path + "' (FORMAT JSON)";
		} else if (format == "parquet") {
			copy_query = "COPY (" + query + ") TO '" + safe_path + "' (FORMAT PARQUET)";
		} else {
			return false; // Unsupported format
		}

		Connection conn(db_instance);
		auto copy_result = conn.Query(copy_query);

		return !copy_result->HasError();

	} catch (const std::exception &e) {
		return false;
	}
}

string ExportToolHandler::FormatData(QueryResult &result, const string &format) const {
	if (format == "json") {
		// Convert to JSON (similar to other handlers)
		string json = "[";
		bool first_row = true;

		while (auto chunk = result.Fetch()) {
			for (idx_t i = 0; i < chunk->size(); i++) {
				if (!first_row) {
					json += ",";
				}
				first_row = false;

				json += "{";
				for (idx_t col = 0; col < chunk->ColumnCount(); col++) {
					if (col > 0)
						json += ",";
					json += "\"" + EscapeJsonString(result.names[col]) + "\":";

					auto value = chunk->GetValue(col, i);
					if (value.IsNull()) {
						json += "null";
					} else {
						json += "\"" + EscapeJsonString(value.ToString()) + "\"";
					}
				}
				json += "}";
			}
		}
		json += "]";
		return json;

	} else if (format == "csv") {
		// Convert to CSV
		string csv;

		// Header
		for (idx_t col = 0; col < result.names.size(); col++) {
			if (col > 0)
				csv += ",";
			csv += result.names[col];
		}
		csv += "\n";

		// Data
		while (auto chunk = result.Fetch()) {
			for (idx_t i = 0; i < chunk->size(); i++) {
				for (idx_t col = 0; col < chunk->ColumnCount(); col++) {
					if (col > 0)
						csv += ",";
					auto value = chunk->GetValue(col, i);
					csv += value.ToString();
				}
				csv += "\n";
			}
		}
		return csv;

	} else {
		// Default to string representation
		return result.ToString();
	}
}

//===--------------------------------------------------------------------===//
// SQLToolHandler Implementation
//===--------------------------------------------------------------------===//

SQLToolHandler::SQLToolHandler(const string &name, const string &description, const string &sql_template,
                               const ToolInputSchema &input_schema, DatabaseInstance &db, const string &result_format)
    : tool_name(name), tool_description(description), sql_template(sql_template), input_schema(input_schema),
      db_instance(db), result_format(result_format) {
}

CallToolResult SQLToolHandler::Execute(const Value &arguments) {
	try {
		// Parse JSON arguments (accepts both VARCHAR JSON and STRUCT)
		JSONArgumentParser parser;
		if (!parser.Parse(arguments)) {
			return CallToolResult::Error("Invalid input: failed to parse arguments");
		}

		// Validate required fields from schema
		if (!parser.ValidateRequired(input_schema.required_fields)) {
			return CallToolResult::Error("Invalid input: missing required fields");
		}

		// Substitute parameters in SQL template
		string sql = SubstituteParameters(sql_template, parser);

		// Execute query
		Connection conn(db_instance);
		auto result = conn.Query(sql);

		if (result->HasError()) {
			return CallToolResult::Error("SQL error: " + result->GetError());
		}

		// Format result using the configured format
		string formatted_result = ResultFormatter::Format(*result, result_format);

		return CallToolResult::Success(Value(formatted_result));

	} catch (const std::exception &e) {
		return CallToolResult::Error("Tool execution error: " + string(e.what()));
	}
}

// Check if a character is a valid SQL/parameter identifier character
static bool IsIdentifierChar(char c) {
	return isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// Format a provided argument value as a safe SQL literal based on its schema type.
// Validates and escapes values to prevent SQL injection.
static string FormatArgumentValue(const string &key, const JSONArgumentParser &parser,
                                  const unordered_map<string, Value> &properties) {
	// Handle explicit JSON null → SQL NULL
	if (parser.IsNull(key)) {
		return "NULL";
	}

	string value = parser.GetValueAsString(key);

	// Determine the parameter type from input schema
	string param_type = "string"; // Default to string for safety
	auto it = properties.find(key);
	if (it != properties.end()) {
		param_type = it->second.ToString();
	}

	if (param_type == "string") {
		// Escape single quotes by doubling them, then wrap in quotes
		string escaped;
		for (char c : value) {
			if (c == '\'') {
				escaped += "''";
			} else {
				escaped += c;
			}
		}
		return "'" + escaped + "'";
	} else if (param_type == "integer" || param_type == "number") {
		// Validate that the value is actually numeric before interpolation.
		// SECURITY: Use the pos output parameter to verify the ENTIRE string was consumed.
		// std::stod("1 OR 1=1") would silently parse "1" and ignore the injection payload.
		try {
			size_t pos = 0;
			if (param_type == "integer") {
				long long int_val = std::stoll(value, &pos);
				if (pos != value.size()) {
					throw std::invalid_argument("trailing characters");
				}
				return std::to_string(int_val);
			} else {
				double numeric_val = std::stod(value, &pos);
				if (pos != value.size()) {
					throw std::invalid_argument("trailing characters");
				}
				return std::to_string(numeric_val);
			}
		} catch (const std::exception &) {
			throw InvalidInputException("Parameter '" + key + "' must be a valid " + param_type +
			                            ", got: " + value);
		}
	} else if (param_type == "boolean") {
		// Validate boolean values strictly
		if (value == "true" || value == "false") {
			return value;
		} else {
			throw InvalidInputException("Parameter '" + key + "' must be 'true' or 'false', got: " + value);
		}
	} else {
		// Unknown type - treat as string for safety
		string escaped;
		for (char c : value) {
			if (c == '\'') {
				escaped += "''";
			} else {
				escaped += c;
			}
		}
		return "'" + escaped + "'";
	}
}

string SQLToolHandler::SubstituteParameters(const string &template_sql, const JSONArgumentParser &parser) const {
	// Build a substitution map: param_name -> formatted SQL value.
	// Provided arguments are formatted by type; omitted schema properties default to NULL.
	unordered_map<string, string> substitutions;

	// Provided arguments (including explicit nulls)
	auto field_names = parser.GetFieldNames();
	for (const auto &key : field_names) {
		substitutions[key] = FormatArgumentValue(key, parser, input_schema.properties);
	}

	// Omitted optional parameters default to NULL
	for (const auto &prop : input_schema.properties) {
		if (substitutions.find(prop.first) == substitutions.end()) {
			substitutions[prop.first] = "NULL";
		}
	}

	// Single-pass scan: find $identifier tokens in the original template and
	// replace them from the map. Because we scan the template linearly and
	// append substituted values without re-scanning them, injected values
	// can never introduce new $param tokens.
	string result;
	result.reserve(template_sql.size());
	size_t i = 0;

	while (i < template_sql.size()) {
		if (template_sql[i] == '$' && i + 1 < template_sql.size() &&
		    (isalpha(static_cast<unsigned char>(template_sql[i + 1])) || template_sql[i + 1] == '_')) {
			// Extract the full identifier after $
			size_t start = i + 1;
			size_t end = start;
			while (end < template_sql.size() && IsIdentifierChar(template_sql[end])) {
				end++;
			}
			string token_name = template_sql.substr(start, end - start);

			auto it = substitutions.find(token_name);
			if (it != substitutions.end()) {
				result += it->second;
			} else {
				// Unknown $token — leave as-is
				result += template_sql.substr(i, end - i);
			}
			i = end;
		} else {
			result += template_sql[i];
			i++;
		}
	}

	return result;
}

//===--------------------------------------------------------------------===//
// ListTablesToolHandler Implementation
//===--------------------------------------------------------------------===//

ListTablesToolHandler::ListTablesToolHandler(DatabaseInstance &db) : db_instance(db) {
}

CallToolResult ListTablesToolHandler::Execute(const Value &arguments) {
	try {
		// Parse JSON arguments (accepts both VARCHAR JSON and STRUCT)
		JSONArgumentParser parser;
		if (!parser.Parse(arguments)) {
			return CallToolResult::Error("Invalid input: failed to parse arguments");
		}

		// Extract parameters (all optional)
		bool include_views = parser.GetBool("include_views", false);
		string schema_filter = parser.GetString("schema");
		string database_filter = parser.GetString("database");

		Connection conn(db_instance);

		// Build query for tables
		string tables_query = R"(
            SELECT
                database_name,
                schema_name,
                table_name,
                estimated_size as row_count_estimate,
                column_count,
                'table' as type
            FROM duckdb_tables()
            WHERE NOT internal
        )";

		// Escape filter values to prevent SQL injection
		string safe_schema = EscapeSQLString(schema_filter);
		string safe_database = EscapeSQLString(database_filter);

		if (!schema_filter.empty()) {
			tables_query += " AND schema_name = '" + safe_schema + "'";
		}
		if (!database_filter.empty()) {
			tables_query += " AND database_name = '" + safe_database + "'";
		}

		string full_query = tables_query;

		// Add views if requested
		if (include_views) {
			string views_query = R"(
                SELECT
                    database_name,
                    schema_name,
                    view_name as table_name,
                    NULL as row_count_estimate,
                    column_count,
                    'view' as type
                FROM duckdb_views()
                WHERE NOT internal
            )";

			if (!schema_filter.empty()) {
				views_query += " AND schema_name = '" + safe_schema + "'";
			}
			if (!database_filter.empty()) {
				views_query += " AND database_name = '" + safe_database + "'";
			}

			full_query = "(" + tables_query + ") UNION ALL (" + views_query + ")";
		}

		full_query += " ORDER BY database_name, schema_name, table_name";

		auto result = conn.Query(full_query);

		if (result->HasError()) {
			return CallToolResult::Error("Query error: " + result->GetError());
		}

		// Format as JSON
		string json = "[";
		bool first_row = true;

		while (auto chunk = result->Fetch()) {
			for (idx_t i = 0; i < chunk->size(); i++) {
				if (!first_row) {
					json += ",";
				}
				first_row = false;

				auto db_name = chunk->GetValue(0, i);
				auto schema_name = chunk->GetValue(1, i);
				auto table_name = chunk->GetValue(2, i);
				auto row_count = chunk->GetValue(3, i);
				auto col_count = chunk->GetValue(4, i);
				auto obj_type = chunk->GetValue(5, i);

				json += "{";
				json += "\"database\":\"" + EscapeJsonString(db_name.ToString()) + "\",";
				json += "\"schema\":\"" + EscapeJsonString(schema_name.ToString()) + "\",";
				json += "\"name\":\"" + EscapeJsonString(table_name.ToString()) + "\",";
				json += "\"type\":\"" + EscapeJsonString(obj_type.ToString()) + "\",";
				if (row_count.IsNull()) {
					json += "\"row_count_estimate\":null,";
				} else {
					json += "\"row_count_estimate\":" + row_count.ToString() + ",";
				}
				json += "\"column_count\":" + col_count.ToString();
				json += "}";
			}
		}
		json += "]";

		return CallToolResult::Success(Value(json));

	} catch (const std::exception &e) {
		return CallToolResult::Error("List tables error: " + string(e.what()));
	}
}

ToolInputSchema ListTablesToolHandler::GetInputSchema() const {
	ToolInputSchema schema;
	schema.type = "object";
	schema.properties["include_views"] = Value("boolean");
	schema.properties["schema"] = Value("string");
	schema.properties["database"] = Value("string");
	// No required fields - all are optional
	return schema;
}

//===--------------------------------------------------------------------===//
// DatabaseInfoToolHandler Implementation
//===--------------------------------------------------------------------===//

DatabaseInfoToolHandler::DatabaseInfoToolHandler(DatabaseInstance &db) : db_instance(db) {
}

CallToolResult DatabaseInfoToolHandler::Execute(const Value &arguments) {
	try {
		// Build comprehensive database info
		string json = "{";

		// Get databases info
		json += "\"databases\":";
		auto databases = GetDatabasesInfo();
		json += databases.ToString();

		// Get schemas info
		json += ",\"schemas\":";
		auto schemas = GetSchemasInfo();
		json += schemas.ToString();

		// Get tables summary
		json += ",\"tables\":";
		auto tables = GetTablesInfo();
		json += tables.ToString();

		// Get views summary
		json += ",\"views\":";
		auto views = GetViewsInfo();
		json += views.ToString();

		// Get extensions info
		json += ",\"extensions\":";
		auto extensions = GetExtensionsInfo();
		json += extensions.ToString();

		json += "}";

		return CallToolResult::Success(Value(json));

	} catch (const std::exception &e) {
		return CallToolResult::Error("Database info error: " + string(e.what()));
	}
}

ToolInputSchema DatabaseInfoToolHandler::GetInputSchema() const {
	ToolInputSchema schema;
	schema.type = "object";
	// No parameters needed
	return schema;
}

Value DatabaseInfoToolHandler::GetDatabasesInfo() const {
	Connection conn(db_instance);
	auto result = conn.Query(R"(
        SELECT
            database_name,
            path,
            type,
            readonly,
            NOT internal as user_attached
        FROM duckdb_databases()
        WHERE NOT internal OR database_name = 'memory'
    )");

	if (result->HasError()) {
		return Value("[]");
	}

	string json = "[";
	bool first = true;

	while (auto chunk = result->Fetch()) {
		for (idx_t i = 0; i < chunk->size(); i++) {
			if (!first)
				json += ",";
			first = false;

			json += "{";
			json += "\"name\":\"" + EscapeJsonString(chunk->GetValue(0, i).ToString()) + "\",";

			auto path = chunk->GetValue(1, i);
			if (path.IsNull()) {
				json += "\"path\":null,";
			} else {
				json += "\"path\":\"" + EscapeJsonString(path.ToString()) + "\",";
			}

			json += "\"type\":\"" + EscapeJsonString(chunk->GetValue(2, i).ToString()) + "\",";
			json += "\"readonly\":" + string(chunk->GetValue(3, i).GetValue<bool>() ? "true" : "false") + ",";
			json += "\"user_attached\":" + string(chunk->GetValue(4, i).GetValue<bool>() ? "true" : "false");
			json += "}";
		}
	}
	json += "]";

	return Value(json);
}

Value DatabaseInfoToolHandler::GetSchemasInfo() const {
	Connection conn(db_instance);
	auto result = conn.Query(R"(
        SELECT
            database_name,
            schema_name,
            NOT internal as user_schema
        FROM duckdb_schemas()
        WHERE NOT internal OR schema_name = 'main'
    )");

	if (result->HasError()) {
		return Value("[]");
	}

	string json = "[";
	bool first = true;

	while (auto chunk = result->Fetch()) {
		for (idx_t i = 0; i < chunk->size(); i++) {
			if (!first)
				json += ",";
			first = false;

			json += "{";
			json += "\"database\":\"" + EscapeJsonString(chunk->GetValue(0, i).ToString()) + "\",";
			json += "\"name\":\"" + EscapeJsonString(chunk->GetValue(1, i).ToString()) + "\",";
			json += "\"user_schema\":" + string(chunk->GetValue(2, i).GetValue<bool>() ? "true" : "false");
			json += "}";
		}
	}
	json += "]";

	return Value(json);
}

Value DatabaseInfoToolHandler::GetTablesInfo() const {
	Connection conn(db_instance);
	auto result = conn.Query(R"(
        SELECT
            database_name,
            schema_name,
            table_name,
            estimated_size as row_count_estimate,
            column_count
        FROM duckdb_tables()
        WHERE NOT internal
        ORDER BY database_name, schema_name, table_name
    )");

	if (result->HasError()) {
		return Value("[]");
	}

	string json = "[";
	bool first = true;

	while (auto chunk = result->Fetch()) {
		for (idx_t i = 0; i < chunk->size(); i++) {
			if (!first)
				json += ",";
			first = false;

			json += "{";
			json += "\"database\":\"" + EscapeJsonString(chunk->GetValue(0, i).ToString()) + "\",";
			json += "\"schema\":\"" + EscapeJsonString(chunk->GetValue(1, i).ToString()) + "\",";
			json += "\"name\":\"" + EscapeJsonString(chunk->GetValue(2, i).ToString()) + "\",";

			auto row_count = chunk->GetValue(3, i);
			if (row_count.IsNull()) {
				json += "\"row_count_estimate\":null,";
			} else {
				json += "\"row_count_estimate\":" + row_count.ToString() + ",";
			}

			json += "\"column_count\":" + chunk->GetValue(4, i).ToString();
			json += "}";
		}
	}
	json += "]";

	return Value(json);
}

Value DatabaseInfoToolHandler::GetViewsInfo() const {
	Connection conn(db_instance);
	auto result = conn.Query(R"(
        SELECT
            database_name,
            schema_name,
            view_name,
            column_count
        FROM duckdb_views()
        WHERE NOT internal
        ORDER BY database_name, schema_name, view_name
    )");

	if (result->HasError()) {
		return Value("[]");
	}

	string json = "[";
	bool first = true;

	while (auto chunk = result->Fetch()) {
		for (idx_t i = 0; i < chunk->size(); i++) {
			if (!first)
				json += ",";
			first = false;

			json += "{";
			json += "\"database\":\"" + EscapeJsonString(chunk->GetValue(0, i).ToString()) + "\",";
			json += "\"schema\":\"" + EscapeJsonString(chunk->GetValue(1, i).ToString()) + "\",";
			json += "\"name\":\"" + EscapeJsonString(chunk->GetValue(2, i).ToString()) + "\",";
			json += "\"column_count\":" + chunk->GetValue(3, i).ToString();
			json += "}";
		}
	}
	json += "]";

	return Value(json);
}

Value DatabaseInfoToolHandler::GetExtensionsInfo() const {
	Connection conn(db_instance);
	auto result = conn.Query(R"(
        SELECT
            extension_name,
            loaded,
            installed,
            description,
            extension_version
        FROM duckdb_extensions()
        WHERE loaded OR installed
        ORDER BY extension_name
    )");

	if (result->HasError()) {
		return Value("[]");
	}

	string json = "[";
	bool first = true;

	while (auto chunk = result->Fetch()) {
		for (idx_t i = 0; i < chunk->size(); i++) {
			if (!first)
				json += ",";
			first = false;

			json += "{";
			json += "\"name\":\"" + EscapeJsonString(chunk->GetValue(0, i).ToString()) + "\",";
			json += "\"loaded\":" + string(chunk->GetValue(1, i).GetValue<bool>() ? "true" : "false") + ",";
			json += "\"installed\":" + string(chunk->GetValue(2, i).GetValue<bool>() ? "true" : "false") + ",";

			auto desc = chunk->GetValue(3, i);
			if (desc.IsNull()) {
				json += "\"description\":null,";
			} else {
				json += "\"description\":\"" + EscapeJsonString(desc.ToString()) + "\",";
			}

			auto version = chunk->GetValue(4, i);
			if (version.IsNull()) {
				json += "\"version\":null";
			} else {
				json += "\"version\":\"" + EscapeJsonString(version.ToString()) + "\"";
			}
			json += "}";
		}
	}
	json += "]";

	return Value(json);
}

//===--------------------------------------------------------------------===//
// ExecuteToolHandler Implementation
//===--------------------------------------------------------------------===//

ExecuteToolHandler::ExecuteToolHandler(DatabaseInstance &db, bool allow_ddl, bool allow_dml,
                                       bool allow_load, bool allow_attach, bool allow_set)
    : db_instance(db), allow_ddl(allow_ddl), allow_dml(allow_dml),
      allow_load(allow_load), allow_attach(allow_attach), allow_set(allow_set) {
}

CallToolResult ExecuteToolHandler::Execute(const Value &arguments) {
	try {
		// Parse JSON arguments (accepts both VARCHAR JSON and STRUCT)
		JSONArgumentParser parser;
		if (!parser.Parse(arguments)) {
			return CallToolResult::Error("Invalid input: failed to parse arguments");
		}

		// Validate required fields
		if (!parser.ValidateRequired({"statement"})) {
			return CallToolResult::Error("Invalid input: missing required field 'statement'");
		}

		string statement = parser.GetString("statement");

		if (statement.empty()) {
			return CallToolResult::Error("Statement is required");
		}

		Connection conn(db_instance);

		// Use DuckDB's prepared statement to get the actual statement type
		auto prepared = conn.Prepare(statement);
		if (prepared->HasError()) {
			return CallToolResult::Error("Parse error: " + prepared->GetError());
		}

		StatementType stmt_type = prepared->GetStatementType();

		// Security check using the parsed statement type
		if (!IsAllowedStatement(stmt_type)) {
			string type_name = StatementTypeToString(stmt_type);
			return CallToolResult::Error("Statement type '" + type_name + "' not allowed by server configuration");
		}

		// Execute the prepared statement
		auto result = prepared->Execute();

		if (result->HasError()) {
			return CallToolResult::Error("Execution error: " + result->GetError());
		}

		// Build response based on statement type
		string json = "{";
		json += "\"success\":true,";
		json += "\"statement_type\":\"" + StatementTypeToString(stmt_type) + "\",";

		if (IsDMLStatement(stmt_type)) {
			// For DML, try to get affected row count
			// DuckDB returns this in the result for INSERT/UPDATE/DELETE
			idx_t affected_rows = 0;
			if (auto chunk = result->Fetch()) {
				if (chunk->size() > 0 && chunk->ColumnCount() > 0) {
					auto val = chunk->GetValue(0, 0);
					if (!val.IsNull()) {
						affected_rows = val.GetValue<int64_t>();
					}
				}
			}
			json += "\"affected_rows\":" + std::to_string(affected_rows);
		} else {
			// For DDL, just report success
			json += "\"message\":\"Statement executed successfully\"";
		}

		json += "}";

		return CallToolResult::Success(Value(json));

	} catch (const std::exception &e) {
		return CallToolResult::Error("Execute error: " + string(e.what()));
	}
}

ToolInputSchema ExecuteToolHandler::GetInputSchema() const {
	ToolInputSchema schema;
	schema.type = "object";
	schema.properties["statement"] = Value("string");
	schema.required_fields = {"statement"};
	return schema;
}

bool ExecuteToolHandler::IsSafeDDLStatement(StatementType type) const {
	// Safe DDL: structural changes that don't load code or change settings
	switch (type) {
	case StatementType::CREATE_STATEMENT:
	case StatementType::DROP_STATEMENT:
	case StatementType::ALTER_STATEMENT:
	case StatementType::VACUUM_STATEMENT:
	case StatementType::ANALYZE_STATEMENT:
	case StatementType::TRANSACTION_STATEMENT:
		return true;
	default:
		return false;
	}
}

bool ExecuteToolHandler::IsLoadStatement(StatementType type) const {
	switch (type) {
	case StatementType::LOAD_STATEMENT:
	case StatementType::UPDATE_EXTENSIONS_STATEMENT:
		return true;
	default:
		return false;
	}
}

bool ExecuteToolHandler::IsAttachStatement(StatementType type) const {
	switch (type) {
	case StatementType::ATTACH_STATEMENT:
	case StatementType::DETACH_STATEMENT:
	case StatementType::COPY_DATABASE_STATEMENT:
		return true;
	default:
		return false;
	}
}

bool ExecuteToolHandler::IsSetStatement(StatementType type) const {
	switch (type) {
	case StatementType::SET_STATEMENT:
	case StatementType::VARIABLE_SET_STATEMENT:
	case StatementType::PRAGMA_STATEMENT:
		return true;
	default:
		return false;
	}
}

bool ExecuteToolHandler::IsDMLStatement(StatementType type) const {
	switch (type) {
	case StatementType::INSERT_STATEMENT:
	case StatementType::UPDATE_STATEMENT:
	case StatementType::DELETE_STATEMENT:
	case StatementType::MERGE_INTO_STATEMENT:
		return true;
	default:
		return false;
	}
}

bool ExecuteToolHandler::IsAllowedStatement(StatementType type) const {
	// Block SELECT-like statements (should use 'query' tool instead)
	if (type == StatementType::SELECT_STATEMENT) {
		return false;
	}

	// Check DML permissions
	if (IsDMLStatement(type) && !allow_dml) {
		return false;
	}

	// Check safe DDL permissions
	if (IsSafeDDLStatement(type) && !allow_ddl) {
		return false;
	}

	// Check dangerous DDL subcategories (each requires its own flag)
	if (IsLoadStatement(type) && !allow_load) {
		return false;
	}
	if (IsAttachStatement(type) && !allow_attach) {
		return false;
	}
	if (IsSetStatement(type) && !allow_set) {
		return false;
	}

	// Block other query-like statements that should use the query tool
	switch (type) {
	case StatementType::EXPLAIN_STATEMENT:
	case StatementType::RELATION_STATEMENT:
	case StatementType::CALL_STATEMENT: // CALL can return results, use query tool
		return false;
	default:
		break;
	}

	return true;
}

} // namespace duckdb
