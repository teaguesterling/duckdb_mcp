#pragma once

#include "duckdb.hpp"
#include "duckdb/common/unordered_map.hpp"

namespace duckdb {

// Forward declarations
struct MCPMessage;

// MCP pagination result structure
struct MCPPaginationResult {
	vector<Value> items; // The actual result items (resources, prompts, tools)
	string next_cursor;  // Cursor for next page (empty if no more pages)
	bool has_more_pages; // Whether there are more pages available
	idx_t total_items;   // Total items fetched so far (for progress tracking)

	MCPPaginationResult() : has_more_pages(false), total_items(0) {
	}

	// Check if this result has more pages to fetch
	bool HasNextPage() const {
		return has_more_pages && !next_cursor.empty();
	}

	// Get the cursor for the next page request
	string GetNextCursor() const {
		return next_cursor;
	}

	// Convert to DuckDB Value for SQL return
	Value ToValue() const;
	static MCPPaginationResult FromValue(const Value &value);
};

// MCP pagination request parameters
struct MCPPaginationParams {
	string cursor; // Cursor for pagination (empty for first request)
	idx_t limit;   // Optional limit hint (servers may ignore)

	MCPPaginationParams() : limit(0) {
	}
	MCPPaginationParams(const string &cursor, idx_t limit = 0) : cursor(cursor), limit(limit) {
	}

	// Check if this is the first page request
	bool IsFirstPage() const {
		return cursor.empty();
	}

	// Convert to JSON-RPC parameters
	Value ToRPCParams() const;
	static MCPPaginationParams FromRPCParams(const Value &params);
};

#ifndef __EMSCRIPTEN__

// MCP pagination iterator for client use
class MCPPaginationIterator {
private:
	string server_name;
	string method_name; // "resources/list", "prompts/list", "tools/list"
	MCPPaginationParams params;
	MCPPaginationResult current_result;
	bool is_initialized;
	bool is_finished;

public:
	MCPPaginationIterator(const string &server, const string &method);
	MCPPaginationIterator(const string &server, const string &method, const string &cursor);

	// Iterator interface
	bool HasNext() const;
	MCPPaginationResult Next();
	void Reset();

	// Convenience methods
	vector<Value> FetchAll();      // Fetch all pages into a single vector
	idx_t GetTotalFetched() const; // Get total items fetched so far

	// Error handling
	bool IsValid() const {
		return !is_finished;
	}
	string GetLastError() const;
};

// Pagination utilities
namespace MCPPagination {
// Parse pagination response from MCP server
MCPPaginationResult ParsePaginationResponse(const MCPMessage &response, const string &items_field);

// Create paginated request message
MCPMessage CreatePaginatedRequest(const string &method, const MCPPaginationParams &params, const Value &id);

// Validate cursor format (basic validation)
bool IsValidCursor(const string &cursor);

// Error codes for pagination
constexpr int INVALID_CURSOR_ERROR = -32602;
constexpr int PAGINATION_NOT_SUPPORTED_ERROR = -32601;
} // namespace MCPPagination

// Pagination-aware connection interface
class MCPConnectionWithPagination {
private:
	shared_ptr<class MCPConnection> connection;

public:
	MCPConnectionWithPagination(shared_ptr<MCPConnection> conn) : connection(conn) {
	}

	// Paginated list operations
	MCPPaginationResult ListResources(const MCPPaginationParams &params = MCPPaginationParams());
	MCPPaginationResult ListPrompts(const MCPPaginationParams &params = MCPPaginationParams());
	MCPPaginationResult ListTools(const MCPPaginationParams &params = MCPPaginationParams());

	// Iterator factory methods
	unique_ptr<MCPPaginationIterator> CreateResourcesIterator();
	unique_ptr<MCPPaginationIterator> CreatePromptsIterator();
	unique_ptr<MCPPaginationIterator> CreateToolsIterator();

	// Get underlying connection
	shared_ptr<MCPConnection> GetConnection() const {
		return connection;
	}
};

#endif // !__EMSCRIPTEN__

} // namespace duckdb
