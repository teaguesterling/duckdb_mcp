#include "result_formatter.hpp"

namespace duckdb {

const vector<string> ResultFormatter::SUPPORTED_FORMATS = {"json", "csv", "markdown"};

bool ResultFormatter::IsFormatSupported(const string &format) {
	for (const auto &f : SUPPORTED_FORMATS) {
		if (f == format) {
			return true;
		}
	}
	return false;
}

string ResultFormatter::GetMimeType(const string &format) {
	if (format == "json") {
		return "application/json";
	} else if (format == "csv") {
		return "text/csv";
	} else if (format == "markdown") {
		return "text/markdown";
	}
	return "text/plain";
}

string ResultFormatter::Format(QueryResult &result, const string &format) {
	if (format == "json") {
		return FormatAsJSON(result);
	} else if (format == "csv") {
		return FormatAsCSV(result);
	} else if (format == "markdown") {
		return FormatAsMarkdown(result);
	}
	// Unsupported format - return empty string
	// Caller should validate format before calling
	return "";
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
				json += "\"" + result.names[col] + "\":";

				auto value = chunk->GetValue(col, i);
				if (value.IsNull()) {
					json += "null";
				} else {
					json += "\"" + value.ToString() + "\"";
				}
			}
			json += "}";
		}
	}
	json += "]";
	return json;
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

string ResultFormatter::FormatAsMarkdown(QueryResult &result) {
	string md;
	idx_t num_cols = result.names.size();

	if (num_cols == 0) {
		return "(empty result)";
	}

	// Header row
	md += "|";
	for (idx_t col = 0; col < num_cols; col++) {
		md += " " + result.names[col] + " |";
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
				// Escape pipe characters in cell values
				for (size_t pos = 0; (pos = cell.find('|', pos)) != string::npos; pos += 2) {
					cell.replace(pos, 1, "\\|");
				}
				md += " " + cell + " |";
			}
			md += "\n";
		}
	}

	return md;
}

} // namespace duckdb
