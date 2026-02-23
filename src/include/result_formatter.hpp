#pragma once

#include "duckdb.hpp"

namespace duckdb {

// Shared utility for formatting QueryResult in various formats
// Used by both tool handlers and resource providers
class ResultFormatter {
public:
	// Format result as JSON array of objects
	static string FormatAsJSON(QueryResult &result);

	// Format result as CSV
	static string FormatAsCSV(QueryResult &result);

	// Format result as GitHub-flavored markdown table
	// - Right-aligns numeric columns
	// - Escapes pipe characters in cell values
	static string FormatAsMarkdown(QueryResult &result);

	// Format result in the specified format (json, csv, markdown)
	// Returns empty string for unsupported formats
	static string Format(QueryResult &result, const string &format);

	// Get MIME type for a format
	static string GetMimeType(const string &format);

	// Check if a format is supported
	static bool IsFormatSupported(const string &format);

	// List of supported formats
	static const vector<string> SUPPORTED_FORMATS;
};

} // namespace duckdb
