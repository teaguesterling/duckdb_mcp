#include "server/resource_providers.hpp"
#include "duckdb/common/exception.hpp"
#include "json_utils.hpp"
#include "result_formatter.hpp"
#include <ctime>

namespace duckdb {

//===--------------------------------------------------------------------===//
// TableResourceProvider Implementation
//===--------------------------------------------------------------------===//

TableResourceProvider::TableResourceProvider(const string &table_name, const string &format, DatabaseInstance &db)
    : table_name(table_name), format(format), db_instance(db) {
}

ReadResourceResult TableResourceProvider::Read() {
	try {
		string query = "SELECT * FROM " + table_name;
		Connection conn(db_instance);
		auto result = conn.Query(query);

		if (result->HasError()) {
			return ReadResourceResult::Error("Query error: " + result->GetError());
		}

		string content = FormatResult(*result);
		return ReadResourceResult::Success(content, GetMimeType());

	} catch (const std::exception &e) {
		return ReadResourceResult::Error("Failed to read table: " + string(e.what()));
	}
}

string TableResourceProvider::GetMimeType() const {
	// Arrow format is resource-specific, others use shared utility
	if (format == "arrow") {
		return "application/vnd.apache.arrow.file";
	}
	return ResultFormatter::GetMimeType(format);
}

size_t TableResourceProvider::GetSize() const {
	// Size is dynamic, return 0 (unknown)
	return 0;
}

string TableResourceProvider::GetDescription() const {
	return "Table '" + table_name + "' in " + format + " format";
}

string TableResourceProvider::FormatResult(QueryResult &result) const {
	// Delegate to shared ResultFormatter utility
	return ResultFormatter::Format(result, format);
}

//===--------------------------------------------------------------------===//
// QueryResourceProvider Implementation
//===--------------------------------------------------------------------===//

QueryResourceProvider::QueryResourceProvider(const string &query, const string &format, DatabaseInstance &db,
                                             uint32_t refresh_interval_seconds)
    : query(query), format(format), db_instance(db), refresh_interval_seconds(refresh_interval_seconds),
      last_refresh_time(0), content_cached(false) {
}

ReadResourceResult QueryResourceProvider::Read() {
	lock_guard<mutex> lock(cache_mutex);

	if (!content_cached || ShouldRefresh()) {
		Refresh();
	}

	if (!content_cached) {
		return ReadResourceResult::Error("Failed to execute query");
	}

	return ReadResourceResult::Success(cached_content, GetMimeType());
}

string QueryResourceProvider::GetMimeType() const {
	// Arrow format is resource-specific, others use shared utility
	if (format == "arrow") {
		return "application/vnd.apache.arrow.file";
	}
	return ResultFormatter::GetMimeType(format);
}

size_t QueryResourceProvider::GetSize() const {
	// Size is dynamic, return 0 (unknown)
	return 0;
}

string QueryResourceProvider::GetDescription() const {
	return "Query result in " + format + " format (refresh: " + std::to_string(refresh_interval_seconds) + "s)";
}

bool QueryResourceProvider::ShouldRefresh() const {
	if (refresh_interval_seconds == 0) {
		return false; // No refresh needed
	}

	time_t now = time(nullptr);
	return (now - last_refresh_time) >= refresh_interval_seconds;
}

void QueryResourceProvider::Refresh() {
	try {
		cached_content = ExecuteQuery();
		content_cached = true;
		last_refresh_time = time(nullptr);
	} catch (const std::exception &e) {
		content_cached = false;
		// Log error but don't throw
	}
}

string QueryResourceProvider::ExecuteQuery() const {
	Connection conn(db_instance);
	auto result = conn.Query(query);

	if (result->HasError()) {
		throw IOException("Query error: " + result->GetError());
	}

	return FormatResult(*result);
}

string QueryResourceProvider::FormatResult(QueryResult &result) const {
	// Delegate to shared ResultFormatter utility
	return ResultFormatter::Format(result, format);
}

//===--------------------------------------------------------------------===//
// StaticResourceProvider Implementation
//===--------------------------------------------------------------------===//

StaticResourceProvider::StaticResourceProvider(const string &content, const string &mime_type,
                                               const string &description)
    : content(content), mime_type(mime_type), description(description) {
}

ReadResourceResult StaticResourceProvider::Read() {
	return ReadResourceResult::Success(content, mime_type);
}

string StaticResourceProvider::GetMimeType() const {
	return mime_type;
}

size_t StaticResourceProvider::GetSize() const {
	return content.size();
}

string StaticResourceProvider::GetDescription() const {
	return description;
}

} // namespace duckdb
