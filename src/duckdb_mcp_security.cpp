#include "duckdb_mcp_security.hpp"
#include "client/mcp_storage_extension.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/storage/storage_extension.hpp"
#include "duckdb/common/serializer/buffered_file_reader.hpp"
#include "include/json_utils.hpp"
#include <fstream>
#include <sstream>

namespace duckdb {

void MCPSecurityConfig::SetAllowedCommands(const string &commands) {
    if (servers_locked) {
        throw InvalidInputException("Cannot modify MCP settings: servers are locked");
    }
    allowed_commands = ParseDelimitedString(commands, ':');
}

void MCPSecurityConfig::SetAllowedUrls(const string &urls) {
    if (servers_locked) {
        throw InvalidInputException("Cannot modify MCP settings: servers are locked");
    }
    allowed_urls = ParseDelimitedString(urls, ' ');
}

void MCPSecurityConfig::SetServerFile(const string &file_path) {
    if (servers_locked) {
        throw InvalidInputException("Cannot modify MCP settings: servers are locked");
    }
    server_file = file_path;
}

void MCPSecurityConfig::LockServers(bool lock) {
    servers_locked = lock;
}

void MCPSecurityConfig::SetServingDisabled(bool disabled) {
    serving_disabled = disabled;
}

bool MCPSecurityConfig::IsCommandAllowed(const string &command_path) const {
    // If no allowlist is configured, allow nothing (secure by default)
    if (allowed_commands.empty()) {
        return false;
    }
    
    // Security: Only allow absolute paths to prevent relative path attacks
    if (!command_path.empty() && command_path[0] != '/') {
        return false;
    }
    
    // Security: Ensure this is an executable path only (no arguments)
    // The command_path should not contain spaces or argument separators
    if (StringUtil::Contains(command_path, " ") || 
        StringUtil::Contains(command_path, "\t") ||
        StringUtil::Contains(command_path, "\n") ||
        StringUtil::Contains(command_path, "\r")) {
        return false;
    }
    
    // Check against allowlist (exact match required)
    for (const auto &allowed : allowed_commands) {
        if (command_path == allowed) {
            return true;
        }
    }
    
    return false;
}

bool MCPSecurityConfig::IsUrlAllowed(const string &url) const {
    // If no allowlist is configured, allow nothing (secure by default)
    if (allowed_urls.empty()) {
        return false;
    }
    
    // Check against allowlist  
    for (const auto &allowed : allowed_urls) {
        if (StringUtil::StartsWith(url, allowed)) {
            return true;
        }
    }
    
    return false;
}

void MCPSecurityConfig::ValidateAttachSecurity(const string &command, const vector<string> &args) const {
    if (servers_locked) {
        throw InvalidInputException("Cannot attach MCP servers: servers are locked");
    }
    
    if (!IsCommandAllowed(command)) {
        throw InvalidInputException("MCP command not allowed: " + command + 
            ". Add to allowed_mcp_commands setting to enable.");
    }
    
    // Additional validation for command arguments
    for (const auto &arg : args) {
        // Prevent dangerous arguments
        if (StringUtil::Contains(arg, "..") || 
            StringUtil::Contains(arg, "|") ||
            StringUtil::Contains(arg, ";") ||
            StringUtil::Contains(arg, "&") ||
            StringUtil::Contains(arg, "`") ||
            StringUtil::Contains(arg, "$")) {
            throw InvalidInputException("MCP argument contains potentially unsafe characters: " + arg);
        }
    }
}

vector<string> MCPSecurityConfig::ParseDelimitedString(const string &input, char delimiter) const {
    vector<string> result;
    if (input.empty()) {
        return result;
    }
    
    stringstream ss(input);
    string item;
    
    while (getline(ss, item, delimiter)) {
        StringUtil::Trim(item);
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    
    return result;
}

MCPConnectionParams ParseMCPAttachParams(const AttachInfo &info) {
    MCPConnectionParams params;
    
    // Check for config file mode first
    if (info.options.find("FROM_CONFIG_FILE") != info.options.end()) {
        auto config_file_value = info.options.at("FROM_CONFIG_FILE");
        if (!config_file_value.IsNull()) {
            params.config_file_path = config_file_value.ToString();
            params.server_name = info.path; // The ATTACH path becomes the server name
            
            // Load parameters from config file
            auto config_params = ParseMCPConfigFile(params.config_file_path, params.server_name);
            
            // Override with any explicitly provided parameters
            if (info.options.find("TRANSPORT") != info.options.end()) {
                auto transport_value = info.options.at("TRANSPORT");
                if (!transport_value.IsNull()) {
                    config_params.transport = transport_value.ToString();
                }
            }
            
            return config_params;
        }
    }
    
    // Try structured parameters (new format)
    if (info.options.find("TRANSPORT") != info.options.end() || 
        info.options.find("ARGS") != info.options.end() ||
        info.options.find("CWD") != info.options.end() ||
        info.options.find("ENV") != info.options.end()) {
        
        // The path is used literally as command or URL
        params.command = info.path;
        
        // Parse TRANSPORT parameter
        if (info.options.find("TRANSPORT") != info.options.end()) {
            auto transport_value = info.options.at("TRANSPORT");
            if (!transport_value.IsNull()) {
                params.transport = transport_value.ToString();
            }
        }
        
        // Parse ARGS parameter
        if (info.options.find("ARGS") != info.options.end()) {
            auto args_value = info.options.at("ARGS");
            if (!args_value.IsNull() && args_value.type().id() == LogicalTypeId::LIST) {
                auto &list_values = ListValue::GetChildren(args_value);
                for (const auto &arg : list_values) {
                    if (!arg.IsNull()) {
                        params.args.push_back(arg.ToString());
                    }
                }
            }
        }
        
        // Parse CWD parameter
        if (info.options.find("CWD") != info.options.end()) {
            auto cwd_value = info.options.at("CWD");
            if (!cwd_value.IsNull()) {
                params.working_dir = cwd_value.ToString();
            }
        }
        
        // Parse ENV parameter
        if (info.options.find("ENV") != info.options.end()) {
            auto env_value = info.options.at("ENV");
            if (!env_value.IsNull() && env_value.type().id() == LogicalTypeId::STRUCT) {
                auto &struct_values = StructValue::GetChildren(env_value);
                for (size_t i = 0; i < struct_values.size(); i++) {
                    auto &key = StructType::GetChildName(env_value.type(), i);
                    auto &value = struct_values[i];
                    if (!value.IsNull()) {
                        params.env[key] = value.ToString();
                    }
                }
            }
        }
    }
    // Fallback to legacy stdio:// format for backward compatibility
    else if (StringUtil::StartsWith(StringUtil::Lower(info.path), "stdio://")) {
        string command_line = info.path.substr(8); // Remove "stdio://"
        if (!command_line.empty()) {
            // Parse command line into executable and arguments
            stringstream ss(command_line);
            string token;
            vector<string> tokens;
            while (ss >> token) {
                tokens.push_back(token);
            }
            
            if (!tokens.empty()) {
                params.command = tokens[0];
                for (size_t i = 1; i < tokens.size(); i++) {
                    params.args.push_back(tokens[i]);
                }
            }
        }
    }
    else {
        // Default: use path as literal command for stdio transport
        params.command = info.path;
        params.transport = "stdio";
    }
    
    return params;
}

MCPConnectionParams ParseMCPConfigFile(const string &config_file_path, const string &server_name) {
    MCPConnectionParams params;
    
    try {
        // Read the .mcp.json file
        std::ifstream file(config_file_path);
        if (!file.is_open()) {
            throw IOException("Could not open MCP config file: " + config_file_path);
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        string json_content = buffer.str();
        file.close();
        
        // Parse JSON using yyjson
        yyjson_doc *doc = yyjson_read(json_content.c_str(), json_content.length(), 0);
        if (!doc) {
            throw IOException("Invalid JSON in MCP config file: " + config_file_path);
        }
        
        yyjson_val *root = yyjson_doc_get_root(doc);
        if (!yyjson_is_obj(root)) {
            yyjson_doc_free(doc);
            throw IOException("MCP config file must contain a JSON object");
        }
        
        // Look for mcpServers object
        yyjson_val *mcp_servers = yyjson_obj_get(root, "mcpServers");
        if (!mcp_servers || !yyjson_is_obj(mcp_servers)) {
            yyjson_doc_free(doc);
            throw IOException("MCP config file must contain 'mcpServers' object");
        }
        
        // Find the specific server
        yyjson_val *server_config = yyjson_obj_get(mcp_servers, server_name.c_str());
        if (!server_config || !yyjson_is_obj(server_config)) {
            yyjson_doc_free(doc);
            throw IOException("Server '" + server_name + "' not found in MCP config file");
        }
        
        // Parse command
        yyjson_val *command_val = yyjson_obj_get(server_config, "command");
        if (command_val && yyjson_is_str(command_val)) {
            params.command = yyjson_get_str(command_val);
        } else {
            yyjson_doc_free(doc);
            throw IOException("Server '" + server_name + "' missing required 'command' field");
        }
        
        // Parse args (optional)
        yyjson_val *args_val = yyjson_obj_get(server_config, "args");
        if (args_val && yyjson_is_arr(args_val)) {
            size_t idx, max;
            yyjson_val *arg;
            yyjson_arr_foreach(args_val, idx, max, arg) {
                if (yyjson_is_str(arg)) {
                    params.args.push_back(yyjson_get_str(arg));
                }
            }
        }
        
        // Parse cwd (optional)
        yyjson_val *cwd_val = yyjson_obj_get(server_config, "cwd");
        if (cwd_val && yyjson_is_str(cwd_val)) {
            params.working_dir = yyjson_get_str(cwd_val);
        }
        
        // Parse env (optional)
        yyjson_val *env_val = yyjson_obj_get(server_config, "env");
        if (env_val && yyjson_is_obj(env_val)) {
            size_t idx, max;
            yyjson_val *key, *val;
            yyjson_obj_foreach(env_val, idx, max, key, val) {
                if (yyjson_is_str(key) && yyjson_is_str(val)) {
                    params.env[yyjson_get_str(key)] = yyjson_get_str(val);
                }
            }
        }
        
        // Parse transport (optional, defaults to stdio)
        yyjson_val *transport_val = yyjson_obj_get(server_config, "transport");
        if (transport_val && yyjson_is_str(transport_val)) {
            params.transport = yyjson_get_str(transport_val);
        } else {
            params.transport = "stdio"; // Default
        }
        
        yyjson_doc_free(doc);
        
        // Set config file parameters for reference
        params.config_file_path = config_file_path;
        params.server_name = server_name;
        
    } catch (const std::exception &e) {
        throw IOException("Error parsing MCP config file '" + config_file_path + "': " + string(e.what()));
    }
    
    return params;
}

} // namespace duckdb