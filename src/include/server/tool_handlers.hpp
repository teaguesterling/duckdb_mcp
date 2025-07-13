#pragma once

#include "duckdb.hpp"
#include "duckdb/main/database.hpp"

namespace duckdb {

// Result structure for tool calls
struct CallToolResult {
    bool success = false;
    Value result;
    string error_message;
    
    static CallToolResult Success(const Value &result) {
        CallToolResult call_result;
        call_result.success = true;
        call_result.result = result;
        return call_result;
    }
    
    static CallToolResult Error(const string &error) {
        CallToolResult call_result;
        call_result.success = false;
        call_result.error_message = error;
        return call_result;
    }
};

// Tool input schema for validation
struct ToolInputSchema {
    string type = "object";
    unordered_map<string, Value> properties;
    vector<string> required_fields;
    
    bool ValidateInput(const Value &input) const;
    Value ToJSON() const;
};

// Abstract base class for tool handlers
class ToolHandler {
public:
    virtual ~ToolHandler() = default;
    
    // Execute tool with given arguments
    virtual CallToolResult Execute(const Value &arguments) = 0;
    
    // Tool metadata
    virtual string GetName() const = 0;
    virtual string GetDescription() const = 0;
    virtual ToolInputSchema GetInputSchema() const = 0;
};

// Query tool handler - executes SQL queries
class QueryToolHandler : public ToolHandler {
public:
    QueryToolHandler(DatabaseInstance &db, const vector<string> &allowed_queries = {}, 
                    const vector<string> &denied_queries = {});
    
    CallToolResult Execute(const Value &arguments) override;
    string GetName() const override { return "query"; }
    string GetDescription() const override { return "Execute SQL query"; }
    ToolInputSchema GetInputSchema() const override;

private:
    DatabaseInstance &db_instance;
    vector<string> allowed_queries;
    vector<string> denied_queries;
    
    bool IsQueryAllowed(const string &query) const;
    string FormatResult(QueryResult &result, const string &format) const;
};

// Describe tool handler - describes tables and queries
class DescribeToolHandler : public ToolHandler {
public:
    DescribeToolHandler(DatabaseInstance &db);
    
    CallToolResult Execute(const Value &arguments) override;
    string GetName() const override { return "describe"; }
    string GetDescription() const override { return "Get table or query schema information"; }
    ToolInputSchema GetInputSchema() const override;

private:
    DatabaseInstance &db_instance;
    
    Value DescribeTable(const string &table_name) const;
    Value DescribeQuery(const string &query) const;
};

// Export tool handler - exports query results to various formats
class ExportToolHandler : public ToolHandler {
public:
    ExportToolHandler(DatabaseInstance &db);
    
    CallToolResult Execute(const Value &arguments) override;
    string GetName() const override { return "export"; }
    string GetDescription() const override { return "Export query results to various formats"; }
    ToolInputSchema GetInputSchema() const override;

private:
    DatabaseInstance &db_instance;
    
    bool ExportToFile(QueryResult &result, const string &format, const string &output_path, const string &query) const;
    string FormatData(QueryResult &result, const string &format) const;
};

// SQL tool handler - executes predefined SQL templates with parameters
class SQLToolHandler : public ToolHandler {
public:
    SQLToolHandler(const string &name, const string &description, const string &sql_template,
                  const ToolInputSchema &input_schema, DatabaseInstance &db);
    
    CallToolResult Execute(const Value &arguments) override;
    string GetName() const override { return tool_name; }
    string GetDescription() const override { return tool_description; }
    ToolInputSchema GetInputSchema() const override { return input_schema; }

private:
    string tool_name;
    string tool_description;
    string sql_template;
    ToolInputSchema input_schema;
    DatabaseInstance &db_instance;
    
    string SubstituteParameters(const string &template_sql, const Value &arguments) const;
};

} // namespace duckdb