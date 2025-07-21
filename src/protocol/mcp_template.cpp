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
    return Value::STRUCT(LogicalType::STRUCT({
        {"name", LogicalType::VARCHAR},
        {"description", LogicalType::VARCHAR},
        {"required", LogicalType::BOOLEAN}
    }), children);
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
            MCP_LOG_WARN("TEMPLATE", "Unknown argument '%s' provided for template '%s'", 
                        provided_arg.first.c_str(), name.c_str());
        }
    }
    
    return true;
}

string MCPTemplate::Render(const unordered_map<string, string> &args) const {
    try {
        // Simple template rendering using fmt with named arguments
        // We'll use a basic approach: replace {arg_name} with actual values
        string result = template_content;
        
        // Use regex to find and replace template variables
        std::regex template_regex(R"(\{([a-zA-Z_][a-zA-Z0-9_]*)\})");
        std::smatch match;
        
        // Keep replacing until no more matches
        string temp_result = result;
        while (std::regex_search(temp_result, match, template_regex)) {
            string var_name = match[1].str();
            string replacement = "";
            
            // Find the argument value
            auto arg_it = args.find(var_name);
            if (arg_it != args.end()) {
                replacement = arg_it->second;
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
                // For optional arguments, leave empty or keep original
                replacement = "";
            }
            
            // Replace the first occurrence
            size_t pos = temp_result.find(match[0].str());
            if (pos != string::npos) {
                temp_result.replace(pos, match[0].length(), replacement);
            }
        }
        
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
    
    auto arg_struct_type = LogicalType::STRUCT({
        {"name", LogicalType::VARCHAR},
        {"description", LogicalType::VARCHAR},
        {"required", LogicalType::BOOLEAN}
    });
    children.push_back(Value::LIST(arg_struct_type, arg_values));
    
    return Value::STRUCT(LogicalType::STRUCT({
        {"name", LogicalType::VARCHAR},
        {"description", LogicalType::VARCHAR},
        {"arguments", LogicalType::LIST(arg_struct_type)}
    }), children);
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
MCPTemplateManager& MCPTemplateManager::GetInstance() {
    static MCPTemplateManager instance;
    return instance;
}

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
    // TODO: Implement proper MCP protocol integration
    // For now, return a simple error response
    return MCPMessage::CreateError(-32601, "Method not implemented", request.id, Value("Templates list not yet integrated with MCP protocol"));
}

MCPMessage MCPTemplateManager::HandlePromptsGet(const MCPMessage &request) const {
    // TODO: Implement proper MCP protocol integration
    // For now, return a simple error response
    return MCPMessage::CreateError(-32601, "Method not implemented", request.id, Value("Template get not yet integrated with MCP protocol"));
}

void MCPTemplateManager::Clear() {
    lock_guard<mutex> lock(template_mutex);
    templates.clear();
    MCP_LOG_INFO("TEMPLATE", "Cleared all templates");
}

} // namespace duckdb