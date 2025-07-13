#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/storage/storage_extension.hpp"

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
    
    //! Check if a command path is allowed
    bool IsCommandAllowed(const string &command_path) const;
    
    //! Check if a URL is allowed  
    bool IsUrlAllowed(const string &url) const;
    
    //! Check if servers are locked
    bool AreServersLocked() const { return servers_locked; }
    
    //! Get server file path
    string GetServerFile() const { return server_file; }
    
    //! Validate ATTACH parameters for security
    void ValidateAttachSecurity(const string &command, const vector<string> &args) const;

private:
    MCPSecurityConfig() : servers_locked(false), server_file("./.mcp.json") {}
    
    vector<string> allowed_commands;
    vector<string> allowed_urls;
    string server_file;
    bool servers_locked;
    
    //! Parse colon or space delimited string into vector
    vector<string> ParseDelimitedString(const string &input, char delimiter) const;
};

//! Structured MCP connection parameters
struct MCPConnectionParams {
    string command;
    vector<string> args;
    string working_dir;
    string transport = "stdio";
    
    bool IsValid() const {
        return !command.empty() && (transport == "stdio" || transport == "tcp");
    }
};

//! Parse structured ATTACH parameters from AttachInfo
MCPConnectionParams ParseMCPAttachParams(const class AttachInfo &info);

} // namespace duckdb