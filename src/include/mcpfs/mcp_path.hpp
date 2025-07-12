#pragma once

#include "duckdb.hpp"

namespace duckdb {

// Structure representing parsed MCP path components
struct MCPPath {
    string server_name;     // Server alias from ATTACH
    string mcp_protocol;    // MCP resource scheme (file, resource, etc.)
    string resource_path;   // Path within the MCP protocol namespace
    string full_mcp_uri;    // Reconstructed MCP URI (protocol://path)
    
    bool IsValid() const {
        return !server_name.empty() && !mcp_protocol.empty();
    }
    
    string ToString() const {
        return "mcp://" + server_name + "/" + full_mcp_uri;
    }
};

// Path parsing and validation utilities
class MCPPathParser {
public:
    // Parse mcp:// URI into components
    static MCPPath ParsePath(const string &path);
    
    // Validate MCP path format
    static bool IsValidMCPPath(const string &path);
    
    // Extract just the server name from an MCP path
    static string ExtractServerName(const string &path);
    
    // Construct MCP path from components
    static string ConstructPath(const string &server_name, const string &mcp_uri);

private:
    // Internal parsing helpers
    static bool StartsWithMCP(const string &path);
    static pair<string, string> SplitServerAndProtocol(const string &remainder);
    static string NormalizePath(const string &path);
};

} // namespace duckdb