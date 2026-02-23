#pragma once

#include "duckdb.hpp"
#include "duckdb/main/database.hpp"
#include "json_utils.hpp"

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

// Parse properties and required JSON into ToolInputSchema
ToolInputSchema ParseToolInputSchema(const string &properties_json, const string &required_json);

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
	                 const vector<string> &denied_queries = {}, const string &default_format = "json");

	CallToolResult Execute(const Value &arguments) override;
	string GetName() const override {
		return "query";
	}
	string GetDescription() const override {
		return "Execute a read-only SQL query and return results. "
		       "Supported formats: json (default), markdown, csv.";
	}
	ToolInputSchema GetInputSchema() const override;

private:
	DatabaseInstance &db_instance;
	vector<string> allowed_queries;
	vector<string> denied_queries;
	string default_result_format;

	bool IsQueryAllowed(const string &query) const;
	string FormatResult(QueryResult &result, const string &format) const;
};

// Describe tool handler - describes tables and queries
class DescribeToolHandler : public ToolHandler {
public:
	DescribeToolHandler(DatabaseInstance &db, const vector<string> &allowed_queries = {},
	                    const vector<string> &denied_queries = {});

	CallToolResult Execute(const Value &arguments) override;
	string GetName() const override {
		return "describe";
	}
	string GetDescription() const override {
		return "Get table or query schema information";
	}
	ToolInputSchema GetInputSchema() const override;

private:
	DatabaseInstance &db_instance;
	vector<string> allowed_queries;
	vector<string> denied_queries;

	Value DescribeTable(const string &table_name) const;
	Value DescribeQuery(const string &query) const;
	bool IsQueryAllowed(const string &query) const;
};

// Export tool handler - exports query results to various formats
class ExportToolHandler : public ToolHandler {
public:
	ExportToolHandler(DatabaseInstance &db, const vector<string> &allowed_queries = {},
	                  const vector<string> &denied_queries = {});

	CallToolResult Execute(const Value &arguments) override;
	string GetName() const override {
		return "export";
	}
	string GetDescription() const override {
		return "Export query results. Inline return supports: json, csv. "
		       "File export (with 'output' path) additionally supports: parquet.";
	}
	ToolInputSchema GetInputSchema() const override;

private:
	DatabaseInstance &db_instance;
	vector<string> allowed_queries;
	vector<string> denied_queries;

	bool ExportToFile(QueryResult &result, const string &format, const string &output_path, const string &query) const;
	string FormatData(QueryResult &result, const string &format) const;
	bool IsQueryAllowed(const string &query) const;
};

// SQL tool handler - executes predefined SQL templates with parameters
class SQLToolHandler : public ToolHandler {
public:
	SQLToolHandler(const string &name, const string &description, const string &sql_template,
	               const ToolInputSchema &input_schema, DatabaseInstance &db, const string &result_format = "json");

	CallToolResult Execute(const Value &arguments) override;
	string GetName() const override {
		return tool_name;
	}
	string GetDescription() const override {
		return tool_description;
	}
	ToolInputSchema GetInputSchema() const override {
		return input_schema;
	}

private:
	string tool_name;
	string tool_description;
	string sql_template;
	ToolInputSchema input_schema;
	DatabaseInstance &db_instance;
	string result_format;

	string SubstituteParameters(const string &template_sql, const JSONArgumentParser &parser) const;
};

// List tables tool handler - lists all tables (and optionally views) in the database
class ListTablesToolHandler : public ToolHandler {
public:
	ListTablesToolHandler(DatabaseInstance &db);

	CallToolResult Execute(const Value &arguments) override;
	string GetName() const override {
		return "list_tables";
	}
	string GetDescription() const override {
		return "List all tables in the database, optionally including views. "
		       "Returns table names, schemas, row counts, and column counts.";
	}
	ToolInputSchema GetInputSchema() const override;

private:
	DatabaseInstance &db_instance;
};

// Database info tool handler - provides comprehensive database overview
class DatabaseInfoToolHandler : public ToolHandler {
public:
	DatabaseInfoToolHandler(DatabaseInstance &db);

	CallToolResult Execute(const Value &arguments) override;
	string GetName() const override {
		return "database_info";
	}
	string GetDescription() const override {
		return "Get comprehensive database information including attached databases, "
		       "schemas, tables, views, and loaded extensions.";
	}
	ToolInputSchema GetInputSchema() const override;

private:
	DatabaseInstance &db_instance;

	Value GetDatabasesInfo() const;
	Value GetSchemasInfo() const;
	Value GetTablesInfo() const;
	Value GetViewsInfo() const;
	Value GetExtensionsInfo() const;
};

// Execute tool handler - executes DDL/DML statements (INSERT, UPDATE, DELETE, CREATE, etc.)
class ExecuteToolHandler : public ToolHandler {
public:
	ExecuteToolHandler(DatabaseInstance &db, bool allow_ddl = true, bool allow_dml = true);

	CallToolResult Execute(const Value &arguments) override;
	string GetName() const override {
		return "execute";
	}
	string GetDescription() const override {
		return "Execute DDL (CREATE, DROP, ALTER) or DML (INSERT, UPDATE, DELETE) statements. "
		       "Returns affected row count for DML, success status for DDL.";
	}
	ToolInputSchema GetInputSchema() const override;

private:
	DatabaseInstance &db_instance;
	bool allow_ddl;
	bool allow_dml;

	// Uses DuckDB's StatementType enum for robust statement classification
	bool IsDDLStatement(StatementType type) const;
	bool IsDMLStatement(StatementType type) const;
	bool IsAllowedStatement(StatementType type) const;
};

} // namespace duckdb
