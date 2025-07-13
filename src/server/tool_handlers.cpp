#include "server/tool_handlers.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {

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
    // Create a simple schema representation
    vector<Value> props;
    for (const auto &prop : properties) {
        props.push_back(Value::STRUCT({
            {"name", Value(prop.first)},
            {"type", prop.second}
        }));
    }
    
    return Value::STRUCT({
        {"type", Value(type)},
        {"properties", Value::LIST(LogicalType::STRUCT({}), props)},
        {"required", Value::LIST(LogicalType::VARCHAR, 
                                vector<Value>(required_fields.begin(), required_fields.end()))}
    });
}

//===--------------------------------------------------------------------===//
// QueryToolHandler Implementation
//===--------------------------------------------------------------------===//

QueryToolHandler::QueryToolHandler(DatabaseInstance &db, const vector<string> &allowed_queries, 
                                  const vector<string> &denied_queries)
    : db_instance(db), allowed_queries(allowed_queries), denied_queries(denied_queries) {
}

CallToolResult QueryToolHandler::Execute(const Value &arguments) {
    try {
        // Validate input
        auto schema = GetInputSchema();
        if (!schema.ValidateInput(arguments)) {
            return CallToolResult::Error("Invalid input: missing required fields");
        }
        
        // Extract parameters
        auto &struct_values = StructValue::GetChildren(arguments);
        string sql;
        string format = "json"; // default format
        
        for (size_t i = 0; i < struct_values.size(); i++) {
            auto &key = StructType::GetChildName(arguments.type(), i);
            if (key == "sql") {
                sql = struct_values[i].ToString();
            } else if (key == "format") {
                format = struct_values[i].ToString();
            }
        }
        
        if (sql.empty()) {
            return CallToolResult::Error("SQL query is required");
        }
        
        // Security check
        if (!IsQueryAllowed(sql)) {
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

bool QueryToolHandler::IsQueryAllowed(const string &query) const {
    string lower_query = StringUtil::Lower(query);
    
    // Check denylist first
    for (const auto &denied : denied_queries) {
        if (lower_query.find(StringUtil::Lower(denied)) != string::npos) {
            return false;
        }
    }
    
    // Check allowlist if it exists
    if (!allowed_queries.empty()) {
        for (const auto &allowed : allowed_queries) {
            if (lower_query.find(StringUtil::Lower(allowed)) != string::npos) {
                return true;
            }
        }
        return false; // Not in allowlist
    }
    
    return true; // No restrictions
}

string QueryToolHandler::FormatResult(QueryResult &result, const string &format) const {
    if (format == "json") {
        // Convert to JSON
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
                    if (col > 0) json += ",";
                    json += "\"" + result.names[col] + "\":";
                    
                    auto value = chunk->GetValue(col, i);
                    if (value.IsNull()) {
                        json += "null";
                    } else {
                        json += "\"" + value.ToString() + "\"";
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
            if (col > 0) csv += ",";
            csv += result.names[col];
        }
        csv += "\n";
        
        // Data
        while (auto chunk = result.Fetch()) {
            for (idx_t i = 0; i < chunk->size(); i++) {
                for (idx_t col = 0; col < chunk->ColumnCount(); col++) {
                    if (col > 0) csv += ",";
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
// DescribeToolHandler Implementation
//===--------------------------------------------------------------------===//

DescribeToolHandler::DescribeToolHandler(DatabaseInstance &db) : db_instance(db) {
}

CallToolResult DescribeToolHandler::Execute(const Value &arguments) {
    try {
        // Extract parameters
        auto &struct_values = StructValue::GetChildren(arguments);
        string table_name;
        string query;
        
        for (size_t i = 0; i < struct_values.size(); i++) {
            auto &key = StructType::GetChildName(arguments.type(), i);
            if (key == "table") {
                table_name = struct_values[i].ToString();
            } else if (key == "query") {
                query = struct_values[i].ToString();
            }
        }
        
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
    string describe_query = "DESCRIBE " + table_name;
    Connection conn(db_instance);
    auto result = conn.Query(describe_query);
    
    if (result->HasError()) {
        throw IOException("Failed to describe table: " + result->GetError());
    }
    
    vector<Value> columns;
    while (auto chunk = result->Fetch()) {
        for (idx_t i = 0; i < chunk->size(); i++) {
            Value column = Value::STRUCT({
                {"name", chunk->GetValue(0, i)},
                {"type", chunk->GetValue(1, i)},
                {"null", chunk->GetValue(2, i)},
                {"key", chunk->GetValue(3, i)},
                {"default", chunk->GetValue(4, i)},
                {"extra", chunk->GetValue(5, i)}
            });
            columns.push_back(column);
        }
    }
    
    return Value::STRUCT({
        {"table", Value(table_name)},
        {"columns", Value::LIST(LogicalType::STRUCT({}), columns)}
    });
}

Value DescribeToolHandler::DescribeQuery(const string &query) const {
    string prepare_query = "PREPARE stmt AS " + query;
    Connection conn(db_instance);
    auto prepare_result = conn.Query(prepare_query);
    
    if (prepare_result->HasError()) {
        throw IOException("Failed to prepare query: " + prepare_result->GetError());
    }
    
    auto describe_result = conn.Query("DESCRIBE stmt");
    if (describe_result->HasError()) {
        throw IOException("Failed to describe query: " + describe_result->GetError());
    }
    
    vector<Value> columns;
    while (auto chunk = describe_result->Fetch()) {
        for (idx_t i = 0; i < chunk->size(); i++) {
            Value column = Value::STRUCT({
                {"name", chunk->GetValue(0, i)},
                {"type", chunk->GetValue(1, i)}
            });
            columns.push_back(column);
        }
    }
    
    return Value::STRUCT({
        {"query", Value(query)},
        {"columns", Value::LIST(LogicalType::STRUCT({}), columns)}
    });
}

//===--------------------------------------------------------------------===//
// ExportToolHandler Implementation
//===--------------------------------------------------------------------===//

ExportToolHandler::ExportToolHandler(DatabaseInstance &db) : db_instance(db) {
}

CallToolResult ExportToolHandler::Execute(const Value &arguments) {
    try {
        // Extract parameters
        auto &struct_values = StructValue::GetChildren(arguments);
        string query;
        string format = "csv"; // default format
        string output_path;
        
        for (size_t i = 0; i < struct_values.size(); i++) {
            auto &key = StructType::GetChildName(arguments.type(), i);
            if (key == "query") {
                query = struct_values[i].ToString();
            } else if (key == "format") {
                format = struct_values[i].ToString();
            } else if (key == "output") {
                output_path = struct_values[i].ToString();
            }
        }
        
        if (query.empty()) {
            return CallToolResult::Error("Query is required");
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

bool ExportToolHandler::ExportToFile(QueryResult &result, const string &format, const string &output_path, const string &query) const {
    try {
        // Use DuckDB's COPY TO functionality
        string copy_query;
        if (format == "csv") {
            copy_query = "COPY (" + query + ") TO '" + output_path + "' (FORMAT CSV, HEADER)";
        } else if (format == "json") {
            copy_query = "COPY (" + query + ") TO '" + output_path + "' (FORMAT JSON)";
        } else if (format == "parquet") {
            copy_query = "COPY (" + query + ") TO '" + output_path + "' (FORMAT PARQUET)";
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
                    if (col > 0) json += ",";
                    json += "\"" + result.names[col] + "\":";
                    
                    auto value = chunk->GetValue(col, i);
                    if (value.IsNull()) {
                        json += "null";
                    } else {
                        json += "\"" + value.ToString() + "\"";
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
            if (col > 0) csv += ",";
            csv += result.names[col];
        }
        csv += "\n";
        
        // Data
        while (auto chunk = result.Fetch()) {
            for (idx_t i = 0; i < chunk->size(); i++) {
                for (idx_t col = 0; col < chunk->ColumnCount(); col++) {
                    if (col > 0) csv += ",";
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
                              const ToolInputSchema &input_schema, DatabaseInstance &db)
    : tool_name(name), tool_description(description), sql_template(sql_template),
      input_schema(input_schema), db_instance(db) {
}

CallToolResult SQLToolHandler::Execute(const Value &arguments) {
    try {
        // Validate input
        if (!input_schema.ValidateInput(arguments)) {
            return CallToolResult::Error("Invalid input: missing required fields");
        }
        
        // Substitute parameters in SQL template
        string sql = SubstituteParameters(sql_template, arguments);
        
        // Execute query
        Connection conn(db_instance);
        auto result = conn.Query(sql);
        
        if (result->HasError()) {
            return CallToolResult::Error("SQL error: " + result->GetError());
        }
        
        // Return result as JSON
        string json_result = "[";
        bool first_row = true;
        
        while (auto chunk = result->Fetch()) {
            for (idx_t i = 0; i < chunk->size(); i++) {
                if (!first_row) {
                    json_result += ",";
                }
                first_row = false;
                
                json_result += "{";
                for (idx_t col = 0; col < chunk->ColumnCount(); col++) {
                    if (col > 0) json_result += ",";
                    json_result += "\"" + result->names[col] + "\":";
                    
                    auto value = chunk->GetValue(col, i);
                    if (value.IsNull()) {
                        json_result += "null";
                    } else {
                        json_result += "\"" + value.ToString() + "\"";
                    }
                }
                json_result += "}";
            }
        }
        json_result += "]";
        
        return CallToolResult::Success(Value(json_result));
        
    } catch (const std::exception &e) {
        return CallToolResult::Error("Tool execution error: " + string(e.what()));
    }
}

string SQLToolHandler::SubstituteParameters(const string &template_sql, const Value &arguments) const {
    string result = template_sql;
    
    if (arguments.type().id() == LogicalTypeId::STRUCT) {
        auto &struct_values = StructValue::GetChildren(arguments);
        
        for (size_t i = 0; i < struct_values.size(); i++) {
            auto &key = StructType::GetChildName(arguments.type(), i);
            auto value = struct_values[i].ToString();
            
            // Simple parameter substitution - replace $key with value
            string param = "$" + key;
            size_t pos = 0;
            while ((pos = result.find(param, pos)) != string::npos) {
                result.replace(pos, param.length(), value);
                pos += value.length();
            }
        }
    }
    
    return result;
}

} // namespace duckdb