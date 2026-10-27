#pragma once

#include "duckdb.hpp"

namespace duckdb {

// Structure representing parsed MCP path components
struct MCPPath {
	string server_name;  // Server alias from ATTACH
	string resource_uri; // Complete MCP resource URI to pass to server

	bool IsValid() const {
		return !server_name.empty() && !resource_uri.empty();
	}

	string ToString() const {
		return "mcp://" + server_name + "/" + resource_uri;
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

	// Readers recognise a file by its extension, so a resource path carries the
	// one matching its declared mimeType (issue #100). This covers only the
	// types that ruling names -- .md, .json, .html -- and is deliberately not a
	// general mimetype table. Returns "" when no extension applies.
	static string ExtensionForMimeType(const string &mime_type);

	// `uri` with the extension for `mime_type` appended, or `uri` unchanged when
	// none applies, it already ends in that extension, or it has no plain
	// trailing segment to extend (a query, a fragment, or a trailing '/').
	static string WithExtension(const string &uri, const string &mime_type);

	// The reverse of WithExtension: true, with `stem` and `extension` set, when
	// `uri` ends in one of the extensions above.
	static bool SplitExtension(const string &uri, string &stem, string &extension);

private:
	// Internal parsing helpers
	static bool StartsWithMCP(const string &path);
	static pair<string, string> SplitServerAndProtocol(const string &remainder);
	static string NormalizePath(const string &path);
};

} // namespace duckdb
