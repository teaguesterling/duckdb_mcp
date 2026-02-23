#include "mcpfs/mcp_path.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/exception.hpp"

namespace duckdb {

MCPPath MCPPathParser::ParsePath(const string &path) {
	MCPPath result;

	if (!IsValidMCPPath(path)) {
		throw InvalidInputException("Invalid MCP path format: " + path);
	}

	// Remove "mcp://" prefix
	string remainder = path.substr(6); // strlen("mcp://") = 6

	// Find first "/" to separate server name from resource URI
	auto slash_pos = remainder.find('/');
	if (slash_pos == string::npos) {
		throw InvalidInputException("MCP path missing resource separator: " + path);
	}

	result.server_name = remainder.substr(0, slash_pos);
	result.resource_uri = remainder.substr(slash_pos + 1);

	// Validate components
	if (result.server_name.empty()) {
		throw InvalidInputException("MCP path has empty server name: " + path);
	}
	if (result.resource_uri.empty()) {
		throw InvalidInputException("MCP path has empty resource URI: " + path);
	}

	return result;
}

bool MCPPathParser::IsValidMCPPath(const string &path) {
	return StartsWithMCP(path) && path.length() > 6; // Must have content after "mcp://"
}

string MCPPathParser::ExtractServerName(const string &path) {
	if (!IsValidMCPPath(path)) {
		return "";
	}

	try {
		auto parsed = ParsePath(path);
		return parsed.server_name;
	} catch (...) {
		return "";
	}
}

string MCPPathParser::ConstructPath(const string &server_name, const string &mcp_uri) {
	if (server_name.empty() || mcp_uri.empty()) {
		throw InvalidInputException("Cannot construct MCP path with empty components");
	}

	return "mcp://" + server_name + "/" + mcp_uri;
}

bool MCPPathParser::StartsWithMCP(const string &path) {
	return StringUtil::StartsWith(StringUtil::Lower(path), "mcp://");
}

pair<string, string> MCPPathParser::SplitServerAndProtocol(const string &remainder) {
	auto slash_pos = remainder.find('/');
	if (slash_pos == string::npos) {
		return {"", ""};
	}

	string server_name = remainder.substr(0, slash_pos);
	string protocol_part = remainder.substr(slash_pos + 1);

	return {server_name, protocol_part};
}

string MCPPathParser::NormalizePath(const string &path) {
	// Basic path normalization - remove double slashes, etc.
	string normalized = path;

	// Replace multiple consecutive slashes with single slash
	while (normalized.find("//") != string::npos) {
		StringUtil::Replace(normalized, "//", "/");
	}

	return normalized;
}

} // namespace duckdb
