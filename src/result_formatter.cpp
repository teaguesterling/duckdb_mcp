#include "result_formatter.hpp"

namespace duckdb {

const vector<string> ResultFormatter::SUPPORTED_FORMATS = {"json", "jsonl", "csv", "markdown", "text"};

bool ResultFormatter::IsFormatSupported(const string &format) {
	for (const auto &f : SUPPORTED_FORMATS) {
		if (f == format) {
			return true;
		}
	}
	return false;
}

string ResultFormatter::GetSupportedFormatsList() {
	string list;
	for (idx_t i = 0; i < SUPPORTED_FORMATS.size(); i++) {
		if (i > 0) {
			list += ", ";
		}
		list += SUPPORTED_FORMATS[i];
	}
	return list;
}

string ResultFormatter::GetMimeType(const string &format) {
	if (format == "json") {
		return "application/json";
	} else if (format == "jsonl") {
		return "application/x-ndjson";
	} else if (format == "csv") {
		return "text/csv";
	} else if (format == "markdown") {
		return "text/markdown";
	} else if (format == "text") {
		return "text/plain";
	}
	return "text/plain";
}

string ResultFormatter::EscapeJsonString(const string &input) {
	string result;
	result.reserve(input.size());
	for (char c : input) {
		switch (c) {
		case '"':
			result += "\\\"";
			break;
		case '\\':
			result += "\\\\";
			break;
		case '\n':
			result += "\\n";
			break;
		case '\r':
			result += "\\r";
			break;
		case '\t':
			result += "\\t";
			break;
		default:
			if (static_cast<unsigned char>(c) < 0x20) {
				// Control character - encode as \u00XX
				char buf[8];
				snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
				result += buf;
			} else {
				result += c;
			}
			break;
		}
	}
	return result;
}

string ResultFormatter::Format(QueryResult &result, const string &format) {
	if (format == "json") {
		return FormatAsJSON(result);
	} else if (format == "jsonl") {
		return FormatAsJSONL(result);
	} else if (format == "csv") {
		return FormatAsCSV(result);
	} else if (format == "markdown") {
		return FormatAsMarkdown(result);
	} else if (format == "text") {
		return FormatAsText(result);
	}
	// Unsupported format - return empty string
	// Caller should validate format before calling
	return "";
}

// Type-aware JSON value serialization: numbers and booleans are unquoted
static void AppendJsonValue(string &out, const Value &value, const LogicalType &type) {
	if (value.IsNull()) {
		out += "null";
		return;
	}
	if (type.IsNumeric()) {
		string s = value.ToString();
		// JSON does not support NaN/Infinity — substitute null
		if (s == "nan" || s == "-nan" || s == "inf" || s == "-inf" || s == "NaN" || s == "Infinity" ||
		    s == "-Infinity") {
			out += "null";
		} else {
			out += s;
		}
		return;
	}
	if (type.id() == LogicalTypeId::BOOLEAN) {
		out += value.GetValue<bool>() ? "true" : "false";
		return;
	}
	out += "\"" + ResultFormatter::EscapeJsonString(value.ToString()) + "\"";
}

string ResultFormatter::FormatAsJSON(QueryResult &result) {
	string json = "[";
	bool first_row = true;

	while (auto chunk = result.Fetch()) {
		for (idx_t i = 0; i < chunk->size(); i++) {
			if (!first_row) {
				json += ",";
			}
			first_row = false;

			json += "{";
			for (idx_t col = 0; col < chunk->ColumnCount(); col++) {
				if (col > 0)
					json += ",";
				json += "\"" + EscapeJsonString(result.names[col]) + "\":";
				AppendJsonValue(json, chunk->GetValue(col, i), result.types[col]);
			}
			json += "}";
		}
	}
	json += "]";
	return json;
}

string ResultFormatter::FormatAsJSONL(QueryResult &result) {
	string jsonl;

	while (auto chunk = result.Fetch()) {
		for (idx_t i = 0; i < chunk->size(); i++) {
			jsonl += "{";
			for (idx_t col = 0; col < chunk->ColumnCount(); col++) {
				if (col > 0)
					jsonl += ",";
				jsonl += "\"" + EscapeJsonString(result.names[col]) + "\":";
				AppendJsonValue(jsonl, chunk->GetValue(col, i), result.types[col]);
			}
			jsonl += "}\n";
		}
	}
	return jsonl;
}

string ResultFormatter::QuoteCSVField(const string &field) {
	bool needs_quoting = false;
	for (char c : field) {
		if (c == ',' || c == '"' || c == '\n' || c == '\r') {
			needs_quoting = true;
			break;
		}
	}
	if (!needs_quoting) {
		return field;
	}
	string result = "\"";
	for (char c : field) {
		if (c == '"') {
			result += "\"\"";
		} else {
			result += c;
		}
	}
	result += "\"";
	return result;
}

string ResultFormatter::FormatAsCSV(QueryResult &result) {
	string csv;

	// Header
	for (idx_t col = 0; col < result.names.size(); col++) {
		if (col > 0)
			csv += ",";
		csv += QuoteCSVField(result.names[col]);
	}
	csv += "\n";

	// Data
	while (auto chunk = result.Fetch()) {
		for (idx_t i = 0; i < chunk->size(); i++) {
			for (idx_t col = 0; col < chunk->ColumnCount(); col++) {
				if (col > 0)
					csv += ",";
				auto value = chunk->GetValue(col, i);
				if (value.IsNull()) {
					// NULL -> empty field (no quotes)
				} else {
					csv += QuoteCSVField(value.ToString());
				}
			}
			csv += "\n";
		}
	}
	return csv;
}

string ResultFormatter::EscapeMarkdownCell(const string &input) {
	string result = input;
	for (size_t pos = 0; (pos = result.find('|', pos)) != string::npos; pos += 2) {
		result.replace(pos, 1, "\\|");
	}
	return result;
}

string ResultFormatter::FormatAsMarkdown(QueryResult &result) {
	string md;
	idx_t num_cols = result.names.size();

	if (num_cols == 0) {
		return "(empty result)";
	}

	// Header row
	md += "|";
	for (idx_t col = 0; col < num_cols; col++) {
		md += " " + EscapeMarkdownCell(result.names[col]) + " |";
	}
	md += "\n";

	// Separator row with alignment hints
	md += "|";
	for (idx_t col = 0; col < num_cols; col++) {
		bool is_numeric = result.types[col].IsNumeric();
		if (is_numeric) {
			md += "---:|"; // Right-align numeric columns
		} else {
			md += "---|"; // Left-align text columns
		}
	}
	md += "\n";

	// Data rows
	while (auto chunk = result.Fetch()) {
		for (idx_t i = 0; i < chunk->size(); i++) {
			md += "|";
			for (idx_t col = 0; col < chunk->ColumnCount(); col++) {
				auto value = chunk->GetValue(col, i);
				string cell = value.IsNull() ? "NULL" : value.ToString();
				md += " " + EscapeMarkdownCell(cell) + " |";
			}
			md += "\n";
		}
	}

	return md;
}

string ResultFormatter::FormatAsText(QueryResult &result) {
	string text;

	while (auto chunk = result.Fetch()) {
		for (idx_t i = 0; i < chunk->size(); i++) {
			for (idx_t col = 0; col < chunk->ColumnCount(); col++) {
				if (col > 0) {
					text += "\t";
				}
				auto value = chunk->GetValue(col, i);
				if (!value.IsNull()) {
					text += value.ToString();
				}
			}
			text += "\n";
		}
	}

	return text;
}

} // namespace duckdb
