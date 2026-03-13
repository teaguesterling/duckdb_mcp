#include "protocol/mcp_template.hpp"
#include "protocol/mcp_message.hpp"
#include "duckdb_mcp_logging.hpp"
#include "duckdb/common/exception.hpp"
#include "fmt/format.h"
#include <regex>

namespace duckdb {

// MCPTemplateArgument implementation
Value MCPTemplateArgument::ToValue() const {
	vector<Value> children;
	children.push_back(Value(name));
	children.push_back(Value(description));
	children.push_back(Value::BOOLEAN(required));
	return Value::STRUCT(LogicalType::STRUCT({{"name", LogicalType::VARCHAR},
	                                          {"description", LogicalType::VARCHAR},
	                                          {"required", LogicalType::BOOLEAN}}),
	                     children);
}

MCPTemplateArgument MCPTemplateArgument::FromValue(const Value &value) {
	if (value.type().id() != LogicalTypeId::STRUCT) {
		throw InvalidInputException("Expected STRUCT type for template argument");
	}

	auto &struct_children = StructValue::GetChildren(value);
	string name, description;
	bool required = false;

	for (idx_t i = 0; i < struct_children.size(); i++) {
		auto &child = struct_children[i];
		auto field_name = StructType::GetChildName(value.type(), i);

		if (field_name == "name" && child.type() == LogicalType::VARCHAR) {
			name = child.ToString();
		} else if (field_name == "description" && child.type() == LogicalType::VARCHAR) {
			description = child.ToString();
		} else if (field_name == "required" && child.type() == LogicalType::BOOLEAN) {
			required = child.GetValue<bool>();
		}
	}

	return MCPTemplateArgument(name, description, required);
}

// MCPTemplate implementation
void MCPTemplate::AddArgument(const string &name, const string &description, bool required) {
	arguments.emplace_back(name, description, required);
}

bool MCPTemplate::ValidateArguments(const unordered_map<string, string> &args, string &error_msg) const {
	// Check that all required arguments are provided
	for (const auto &arg_def : arguments) {
		if (arg_def.required && args.find(arg_def.name) == args.end()) {
			error_msg = "Missing required argument: " + arg_def.name;
			return false;
		}
	}

	// Check for unknown arguments (optional - could be made configurable)
	unordered_set<string> known_args;
	for (const auto &arg_def : arguments) {
		known_args.insert(arg_def.name);
	}

	for (const auto &provided_arg : args) {
		if (known_args.find(provided_arg.first) == known_args.end()) {
			MCP_LOG_WARN("TEMPLATE", "Unknown argument '%s' provided for template '%s'", provided_arg.first.c_str(),
			             name.c_str());
		}
	}

	return true;
}

string MCPTemplate::Render(const unordered_map<string, string> &args) const {
	try {
		// Simple template rendering using fmt with named arguments
		// We'll use a basic approach: replace {arg_name} with actual values
		string result = template_content;

		// Single-pass replacement of template variables to avoid infinite loops
		// when replacement values themselves contain {var} patterns.
		std::regex template_regex(R"(\{([a-zA-Z_][a-zA-Z0-9_]*)\})");

		string temp_result;
		string::const_iterator search_start = result.cbegin();
		std::smatch match;

		while (std::regex_search(search_start, result.cend(), match, template_regex)) {
			// Append text before the match
			temp_result.append(search_start, search_start + match.position());

			string var_name = match[1].str();

			auto arg_it = args.find(var_name);
			if (arg_it != args.end()) {
				temp_result.append(arg_it->second);
			} else {
				// Check if it's a required argument
				bool is_required = false;
				for (const auto &arg_def : arguments) {
					if (arg_def.name == var_name && arg_def.required) {
						is_required = true;
						break;
					}
				}

				if (is_required) {
					throw InvalidInputException("Missing required template variable: " + var_name);
				}
				// For optional arguments, replace with empty string
			}

			search_start += match.position() + match.length();
		}
		// Append remaining text after last match
		temp_result.append(search_start, result.cend());

		MCP_LOG_DEBUG("TEMPLATE", "Rendered template '%s' successfully", name.c_str());
		return temp_result;

	} catch (const std::exception &e) {
		MCP_LOG_ERROR("TEMPLATE", "Failed to render template '%s': %s", name.c_str(), e.what());
		throw InvalidInputException("Template rendering failed: " + string(e.what()));
	}
}

Value MCPTemplate::ToValue() const {
	vector<Value> children;
	children.push_back(Value(name));
	children.push_back(Value(description));

	// Convert arguments to array
	vector<Value> arg_values;
	for (const auto &arg : arguments) {
		arg_values.push_back(arg.ToValue());
	}

	auto arg_struct_type = LogicalType::STRUCT(
	    {{"name", LogicalType::VARCHAR}, {"description", LogicalType::VARCHAR}, {"required", LogicalType::BOOLEAN}});
	children.push_back(Value::LIST(arg_struct_type, arg_values));

	return Value::STRUCT(LogicalType::STRUCT({{"name", LogicalType::VARCHAR},
	                                          {"description", LogicalType::VARCHAR},
	                                          {"arguments", LogicalType::LIST(arg_struct_type)}}),
	                     children);
}

MCPTemplate MCPTemplate::FromValue(const Value &value) {
	if (value.type().id() != LogicalTypeId::STRUCT) {
		throw InvalidInputException("Expected STRUCT type for template");
	}

	auto &struct_children = StructValue::GetChildren(value);
	string name, description;
	vector<MCPTemplateArgument> arguments;

	for (idx_t i = 0; i < struct_children.size(); i++) {
		auto &child = struct_children[i];
		auto field_name = StructType::GetChildName(value.type(), i);

		if (field_name == "name" && child.type() == LogicalType::VARCHAR) {
			name = child.ToString();
		} else if (field_name == "description" && child.type() == LogicalType::VARCHAR) {
			description = child.ToString();
		} else if (field_name == "arguments" && child.type().id() == LogicalTypeId::LIST) {
			auto &list_children = ListValue::GetChildren(child);
			for (const auto &arg_value : list_children) {
				arguments.push_back(MCPTemplateArgument::FromValue(arg_value));
			}
		}
	}

	auto result = MCPTemplate(name, description);
	result.arguments = move(arguments);
	return result;
}

// MCPTemplateManager implementation

void MCPTemplateManager::RegisterTemplate(const MCPTemplate &template_def) {
	lock_guard<mutex> lock(template_mutex);
	templates[template_def.name] = template_def;
	MCP_LOG_INFO("TEMPLATE", "Registered template: %s", template_def.name.c_str());
}

void MCPTemplateManager::UnregisterTemplate(const string &name) {
	lock_guard<mutex> lock(template_mutex);
	auto it = templates.find(name);
	if (it != templates.end()) {
		templates.erase(it);
		MCP_LOG_INFO("TEMPLATE", "Unregistered template: %s", name.c_str());
	}
}

bool MCPTemplateManager::HasTemplate(const string &name) const {
	lock_guard<mutex> lock(template_mutex);
	return templates.find(name) != templates.end();
}

MCPTemplate MCPTemplateManager::GetTemplate(const string &name) const {
	lock_guard<mutex> lock(template_mutex);
	auto it = templates.find(name);
	if (it == templates.end()) {
		throw InvalidInputException("Template not found: " + name);
	}
	return it->second;
}

vector<MCPTemplate> MCPTemplateManager::ListTemplates() const {
	lock_guard<mutex> lock(template_mutex);
	vector<MCPTemplate> result;
	result.reserve(templates.size());
	for (const auto &pair : templates) {
		result.push_back(pair.second);
	}
	return result;
}

string MCPTemplateManager::RenderTemplate(const string &name, const unordered_map<string, string> &args) const {
	auto template_def = GetTemplate(name);

	string error_msg;
	if (!template_def.ValidateArguments(args, error_msg)) {
		throw InvalidInputException("Template validation failed: " + error_msg);
	}

	return template_def.Render(args);
}

MCPMessage MCPTemplateManager::HandlePromptsList(const MCPMessage &request) const {
	auto template_list = ListTemplates();

	// Build prompt entries with proper struct type
	child_list_t<LogicalType> arg_struct_members;
	arg_struct_members.push_back({"name", LogicalType::VARCHAR});
	arg_struct_members.push_back({"description", LogicalType::VARCHAR});
	arg_struct_members.push_back({"required", LogicalType::BOOLEAN});
	LogicalType arg_struct_type = LogicalType::STRUCT(arg_struct_members);

	child_list_t<LogicalType> prompt_struct_members;
	prompt_struct_members.push_back({"name", LogicalType::VARCHAR});
	prompt_struct_members.push_back({"description", LogicalType::VARCHAR});
	prompt_struct_members.push_back({"arguments", LogicalType::LIST(arg_struct_type)});
	LogicalType prompt_struct_type = LogicalType::STRUCT(prompt_struct_members);

	vector<Value> prompts;
	for (const auto &tmpl : template_list) {
		vector<Value> arg_values;
		for (const auto &arg : tmpl.arguments) {
			arg_values.push_back(Value::STRUCT({{"name", Value(arg.name)},
			                                    {"description", Value(arg.description)},
			                                    {"required", Value::BOOLEAN(arg.required)}}));
		}

		Value args_list = Value::LIST(arg_struct_type, arg_values);
		prompts.push_back(Value::STRUCT(
		    {{"name", Value(tmpl.name)}, {"description", Value(tmpl.description)}, {"arguments", args_list}}));
	}

	Value prompts_list = Value::LIST(prompt_struct_type, prompts);
	Value result = Value::STRUCT({{"prompts", prompts_list}});
	return MCPMessage::CreateResponse(result, request.id);
}

MCPMessage MCPTemplateManager::HandlePromptsGet(const MCPMessage &request) const {
	// Extract name and arguments from request params
	string name;
	unordered_map<string, string> args;

	if (request.params.type().id() == LogicalTypeId::VARCHAR) {
		string params_str = request.params.ToString();
		yyjson_doc *doc = yyjson_read(params_str.c_str(), params_str.size(), 0);
		if (!doc) {
			return MCPMessage::CreateError(-32602, "Invalid JSON in prompt params", request.id);
		}
		yyjson_val *root = yyjson_doc_get_root(doc);
		if (root) {
			yyjson_val *name_val = yyjson_obj_get(root, "name");
			if (name_val && yyjson_is_str(name_val)) {
				name = yyjson_get_str(name_val);
			}
			yyjson_val *args_val = yyjson_obj_get(root, "arguments");
			if (args_val && yyjson_is_obj(args_val)) {
				yyjson_obj_iter iter = yyjson_obj_iter_with(args_val);
				yyjson_val *key;
				while ((key = yyjson_obj_iter_next(&iter))) {
					yyjson_val *val = yyjson_obj_iter_get_val(key);
					if (yyjson_is_str(val)) {
						args[yyjson_get_str(key)] = yyjson_get_str(val);
					}
				}
			}
		}
		yyjson_doc_free(doc);
	} else if (request.params.type().id() == LogicalTypeId::STRUCT) {
		auto &struct_values = StructValue::GetChildren(request.params);
		for (size_t i = 0; i < struct_values.size(); i++) {
			auto &key = StructType::GetChildName(request.params.type(), i);
			if (key == "name") {
				name = struct_values[i].ToString();
			} else if (key == "arguments") {
				// Parse arguments from nested struct or JSON string
				if (struct_values[i].type().id() == LogicalTypeId::VARCHAR) {
					string args_str = struct_values[i].ToString();
					yyjson_doc *doc = yyjson_read(args_str.c_str(), args_str.size(), 0);
					if (!doc) {
						return MCPMessage::CreateError(-32602, "Invalid JSON in prompt arguments", request.id);
					}
					yyjson_val *root = yyjson_doc_get_root(doc);
					if (root && yyjson_is_obj(root)) {
						yyjson_obj_iter iter = yyjson_obj_iter_with(root);
						yyjson_val *akey;
						while ((akey = yyjson_obj_iter_next(&iter))) {
							yyjson_val *val = yyjson_obj_iter_get_val(akey);
							if (yyjson_is_str(val)) {
								args[yyjson_get_str(akey)] = yyjson_get_str(val);
							}
						}
					}
					yyjson_doc_free(doc);
				}
			}
		}
	}

	if (name.empty()) {
		return MCPMessage::CreateError(-32602, "Missing prompt name", request.id);
	}

	try {
		string rendered = RenderTemplate(name, args);

		// Build MCP messages array response
		child_list_t<LogicalType> content_struct_members;
		content_struct_members.push_back({"type", LogicalType::VARCHAR});
		content_struct_members.push_back({"text", LogicalType::VARCHAR});
		LogicalType content_struct_type = LogicalType::STRUCT(content_struct_members);

		child_list_t<LogicalType> msg_struct_members;
		msg_struct_members.push_back({"role", LogicalType::VARCHAR});
		msg_struct_members.push_back({"content", content_struct_type});
		LogicalType msg_struct_type = LogicalType::STRUCT(msg_struct_members);

		Value content = Value::STRUCT({{"type", Value("text")}, {"text", Value(rendered)}});
		Value message = Value::STRUCT({{"role", Value("user")}, {"content", content}});
		Value messages_list = Value::LIST(msg_struct_type, {message});
		Value result = Value::STRUCT({{"messages", messages_list}});

		return MCPMessage::CreateResponse(result, request.id);
	} catch (const InvalidInputException &e) {
		// Template not found or validation failure
		return MCPMessage::CreateError(-32602, string(e.what()), request.id);
	} catch (const std::exception &e) {
		return MCPMessage::CreateError(-32603, string(e.what()), request.id);
	}
}

void MCPTemplateManager::Clear() {
	lock_guard<mutex> lock(template_mutex);
	templates.clear();
	MCP_LOG_INFO("TEMPLATE", "Cleared all templates");
}

} // namespace duckdb
