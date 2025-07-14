#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/storage/storage_extension.hpp"
#include "duckdb/common/unordered_map.hpp"

namespace duckdb {

//! MCP Security Configuration
//! Manages security settings for MCP server connections
class MCPSecurityConfig {
public:
    static MCPSecurityConfig& GetInstance() {
        static MCPSecurityConfig instance;
        return instance;
    }
    
    //! Set allowed MCP command paths (colon-delimited)
    void SetAllowedCommands(const string &commands);
    
    //! Set allowed MCP URLs (space-delimited) 
    void SetAllowedUrls(const string &urls);
    
    //! Set MCP server configuration file path
    void SetServerFile(const string &file_path);
    
    //! Lock server configuration (prevent further changes)
    void LockServers(bool lock);
    
    //! Disable MCP server functionality entirely (client-only mode)
    void SetServingDisabled(bool disabled);
    
    //! Check if a command path is allowed
    bool IsCommandAllowed(const string &command_path) const;
    
    //! Check if a URL is allowed  
    bool IsUrlAllowed(const string &url) const;
    
    //! Check if servers are locked
    bool AreServersLocked() const { return servers_locked; }
    
    //! Check if commands are locked (immutable once set)
    bool AreCommandsLocked() const { return commands_locked; }
    
    //! Check if serving is disabled
    bool IsServingDisabled() const { return serving_disabled; }
    
    //! Get server file path
    string GetServerFile() const { return server_file; }
    
    //! Validate ATTACH parameters for security
    void ValidateAttachSecurity(const string &command, const vector<string> &args) const;

private:
    MCPSecurityConfig() : servers_locked(false), commands_locked(false), serving_disabled(false), server_file("./.mcp.json") {}
    
    vector<string> allowed_commands;
    vector<string> allowed_urls;
    string server_file;
    bool servers_locked;
    bool commands_locked;
    bool serving_disabled;
    
    //! Parse colon or space delimited string into vector
    vector<string> ParseDelimitedString(const string &input, char delimiter) const;
};

//! Structured MCP connection parameters
struct MCPConnectionParams {
    string command;                    // Command path or URL
    vector<string> args;              // Command arguments
    string working_dir;               // Current working directory
    string transport = "stdio";       // Transport type (stdio, tcp, websocket)
    unordered_map<string, string> env; // Environment variables
    
    // Configuration file parameters
    string config_file_path;          // Path to .mcp.json file
    string server_name;               // Server name in .mcp.json
    
    bool IsValid() const {
        // For config file mode, we need config_file_path and server_name
        if (!config_file_path.empty()) {
            return !server_name.empty();
        }
        // For direct mode, we need command and valid transport
        return !command.empty() && (transport == "stdio" || transport == "tcp" || transport == "websocket");
    }
    
    bool IsConfigFileMode() const {
        return !config_file_path.empty() && !server_name.empty();
    }
};

//! Parse structured ATTACH parameters from AttachInfo
MCPConnectionParams ParseMCPAttachParams(const class AttachInfo &info);

//! Parse .mcp.json configuration file and extract server parameters
MCPConnectionParams ParseMCPConfigFile(const string &config_file_path, const string &server_name);

} // namespace duckdb