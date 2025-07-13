#include "duckdb_mcp_security.hpp"
#include "client/mcp_storage_extension.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/types/list_value.hpp"
#include "duckdb/common/types/struct_value.hpp"
#include "duckdb/storage/attached_database.hpp"

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

bool MCPSecurityConfig::IsCommandAllowed(const string &command_path) const {
    // If no allowlist is configured, allow nothing (secure by default)
    if (allowed_commands.empty()) {
        return false;
    }
    
    // Normalize path for comparison
    string normalized_command = command_path;
    
    // Check against allowlist
    for (const auto &allowed : allowed_commands) {
        if (normalized_command == allowed) {
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
    
    // Try structured parameters first (new format)
    if (info.options.find("command") != info.options.end()) {
        auto command_value = info.options.at("command");
        if (!command_value.IsNull()) {
            params.command = command_value.ToString();
        }
        
        if (info.options.find("args") != info.options.end()) {
            auto args_value = info.options.at("args");
            if (!args_value.IsNull() && args_value.type().id() == LogicalTypeId::LIST) {
                // Parse list of arguments
                auto &list_values = ListValue::GetChildren(args_value);
                for (const auto &arg : list_values) {
                    if (!arg.IsNull()) {
                        params.args.push_back(arg.ToString());
                    }
                }
            }
        }
        
        if (info.options.find("working_dir") != info.options.end()) {
            auto wd_value = info.options.at("working_dir");
            if (!wd_value.IsNull()) {
                params.working_dir = wd_value.ToString();
            }
        }
        
        if (info.options.find("transport") != info.options.end()) {
            auto transport_value = info.options.at("transport");
            if (!transport_value.IsNull()) {
                params.transport = transport_value.ToString();
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
    
    return params;
}

} // namespace duckdb