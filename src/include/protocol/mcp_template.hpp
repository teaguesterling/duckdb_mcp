#pragma once

#include "duckdb.hpp"
#include "duckdb/common/unordered_map.hpp"
#include "duckdb/common/unordered_set.hpp"
#include "yyjson.hpp"
#include <vector>

using namespace duckdb_yyjson;

namespace duckdb {

// Forward declaration
struct MCPMessage;

// Template argument definition
struct MCPTemplateArgument {
	string name;
	string description;
	bool required;

	MCPTemplateArgument(const string &name, const string &description = "", bool required = false)
	    : name(name), description(description), required(required) {
	}

	Value ToValue() const;
	static MCPTemplateArgument FromValue(const Value &value);
};

// Template definition
struct MCPTemplate {
	string name;
	string description;
	vector<MCPTemplateArgument> arguments;
	string template_content;

	MCPTemplate() = default;
	MCPTemplate(const string &name, const string &description = "", const string &template_content = "")
	    : name(name), description(description), template_content(template_content) {
	}

	// Add an argument to the template
	void AddArgument(const string &name, const string &description = "", bool required = false);

	// Validate that all required arguments are provided
	bool ValidateArguments(const unordered_map<string, string> &args, string &error_msg) const;

	// Render template with provided arguments using DuckDB's fmt
	string Render(const unordered_map<string, string> &args) const;

	// Convert to/from DuckDB Value for JSON serialization
	Value ToValue() const;
	static MCPTemplate FromValue(const Value &value);
};

// Template manager for handling MCP template operations
class MCPTemplateManager {
private:
	unordered_map<string, MCPTemplate> templates;
	mutable mutex template_mutex;

public:
	MCPTemplateManager() = default;
	~MCPTemplateManager() = default;

	// Template management
	void RegisterTemplate(const MCPTemplate &template_def);
	void UnregisterTemplate(const string &name);
	bool HasTemplate(const string &name) const;

	// Get template operations
	MCPTemplate GetTemplate(const string &name) const;
	vector<MCPTemplate> ListTemplates() const;

	// Template rendering
	string RenderTemplate(const string &name, const unordered_map<string, string> &args) const;

	// MCP Protocol message handlers
	MCPMessage HandlePromptsList(const MCPMessage &request) const;
	MCPMessage HandlePromptsGet(const MCPMessage &request) const;

	// Clear all templates (useful for testing)
	void Clear();

	// Get singleton instance
	static MCPTemplateManager &GetInstance();
};

} // namespace duckdb
