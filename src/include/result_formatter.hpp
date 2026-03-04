#pragma once

#include "duckdb.hpp"

namespace duckdb {

// Shared utility for formatting QueryResult in various formats
// Used by both tool handlers and resource providers
class ResultFormatter {
public:
	// Format result as JSON array of objects
	static string FormatAsJSON(QueryResult &result);

	// Format result as JSON Lines (one JSON object per line)
	static string FormatAsJSONL(QueryResult &result);

	// Format result as CSV (RFC 4180 compliant)
	static string FormatAsCSV(QueryResult &result);

	// Quote a CSV field per RFC 4180: wrap in double quotes and escape
	// internal double quotes by doubling them, if the field contains
	// comma, double quote, newline, or carriage return.
	static string QuoteCSVField(const string &field);

	// Format result as GitHub-flavored markdown table
	// - Right-aligns numeric columns
	// - Escapes pipe characters in header names and cell values
	static string FormatAsMarkdown(QueryResult &result);

	// Format result as plain text (no headers, tab-separated columns, one row per line)
	// NULLs are rendered as empty strings
	static string FormatAsText(QueryResult &result);

	// Escape pipe characters in a string for use in markdown table cells/headers
	static string EscapeMarkdownCell(const string &input);

	// Format result in the specified format (json, jsonl, csv, markdown, text)
	// Returns empty string for unsupported formats
	static string Format(QueryResult &result, const string &format);

	// Get MIME type for a format
	static string GetMimeType(const string &format);

	// Check if a format is supported
	static bool IsFormatSupported(const string &format);

	// Get a comma-separated list of supported format names
	static string GetSupportedFormatsList();

	// Escape a string for safe inclusion in a JSON string value.
	// Handles quotes, backslashes, and control characters.
	static string EscapeJsonString(const string &input);

	// List of supported formats
	static const vector<string> SUPPORTED_FORMATS;
};

} // namespace duckdb
