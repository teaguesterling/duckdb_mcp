#include "protocol/mcp_pagination.hpp"
#include "protocol/mcp_message.hpp"
#ifndef __EMSCRIPTEN__
#include "protocol/mcp_connection.hpp"
#include "client/mcp_storage_extension.hpp"
#endif
#include "duckdb_mcp_logging.hpp"
#include "duckdb/common/exception.hpp"
#include "yyjson.hpp"

using namespace duckdb_yyjson;

namespace duckdb {

// MCPPaginationResult implementation
Value MCPPaginationResult::ToValue() const {
	vector<Value> children;

	// Convert items array to DuckDB LIST
	children.push_back(Value::LIST(LogicalType::JSON(), items));

	// Add pagination metadata
	children.push_back(Value(next_cursor));
	children.push_back(Value::BOOLEAN(has_more_pages));
	children.push_back(Value::BIGINT(total_items));

	return Value::STRUCT(LogicalType::STRUCT({{"items", LogicalType::LIST(LogicalType::JSON())},
	                                          {"next_cursor", LogicalType::VARCHAR},
	                                          {"has_more_pages", LogicalType::BOOLEAN},
	                                          {"total_items", LogicalType::BIGINT}}),
	                     children);
}

MCPPaginationResult MCPPaginationResult::FromValue(const Value &value) {
	if (value.type().id() != LogicalTypeId::STRUCT) {
		throw InvalidInputException("Expected STRUCT type for pagination result");
	}

	MCPPaginationResult result;
	auto &struct_children = StructValue::GetChildren(value);

	for (idx_t i = 0; i < struct_children.size(); i++) {
		auto field_name = StructType::GetChildName(value.type(), i);
		auto &child = struct_children[i];

		if (field_name == "items" && child.type().id() == LogicalTypeId::LIST) {
			auto &list_children = ListValue::GetChildren(child);
			result.items = list_children;
		} else if (field_name == "next_cursor" && child.type() == LogicalType::VARCHAR) {
			result.next_cursor = child.ToString();
		} else if (field_name == "has_more_pages" && child.type() == LogicalType::BOOLEAN) {
			result.has_more_pages = child.GetValue<bool>();
		} else if (field_name == "total_items" && child.type() == LogicalType::BIGINT) {
			result.total_items = child.GetValue<int64_t>();
		}
	}

	return result;
}

// MCPPaginationParams implementation
Value MCPPaginationParams::ToRPCParams() const {
	// Create JSON object for RPC parameters
	yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
	yyjson_mut_val *root = yyjson_mut_obj(doc);
	yyjson_mut_doc_set_root(doc, root);

	// Add cursor if provided
	if (!cursor.empty()) {
		yyjson_mut_obj_add_str(doc, root, "cursor", cursor.c_str());
	}

	// Add limit hint if provided (servers may ignore)
	if (limit > 0) {
		yyjson_mut_obj_add_uint(doc, root, "limit", limit);
	}

	// Convert to JSON string
	const char *json_str = yyjson_mut_write(doc, 0, nullptr);
	Value result = Value(json_str);
	yyjson_mut_doc_free(doc);

	return result;
}

MCPPaginationParams MCPPaginationParams::FromRPCParams(const Value &params) {
	MCPPaginationParams result;

	if (params.type() == LogicalType::VARCHAR) {
		string json_str = params.ToString();
		yyjson_doc *doc = yyjson_read(json_str.c_str(), json_str.length(), 0);

		if (doc) {
			yyjson_val *root = yyjson_doc_get_root(doc);
			if (yyjson_is_obj(root)) {
				// Extract cursor
				yyjson_val *cursor_val = yyjson_obj_get(root, "cursor");
				if (cursor_val && yyjson_is_str(cursor_val)) {
					result.cursor = yyjson_get_str(cursor_val);
				}

				// Extract limit hint
				yyjson_val *limit_val = yyjson_obj_get(root, "limit");
				if (limit_val && yyjson_is_uint(limit_val)) {
					result.limit = yyjson_get_uint(limit_val);
				}
			}
			yyjson_doc_free(doc);
		}
	}

	return result;
}

#ifndef __EMSCRIPTEN__

// MCPPaginationIterator implementation
MCPPaginationIterator::MCPPaginationIterator(const string &server, const string &method)
    : server_name(server), method_name(method), is_initialized(false), is_finished(false) {
}

MCPPaginationIterator::MCPPaginationIterator(const string &server, const string &method, const string &cursor)
    : server_name(server), method_name(method), params(cursor), is_initialized(false), is_finished(false) {
}

bool MCPPaginationIterator::HasNext() const {
	if (!is_initialized) {
		return true; // First request
	}
	return current_result.HasNextPage() && !is_finished;
}

MCPPaginationResult MCPPaginationIterator::Next() {
	if (is_finished) {
		throw InvalidInputException("Pagination iterator is finished");
	}

	try {
		// Get connection from registry
		auto connection = MCPConnectionRegistry::GetInstance().GetConnection(server_name);
		if (!connection) {
			throw InvalidInputException("MCP server not attached: " + server_name);
		}

		// Send request with current parameters
		auto request = MCPPagination::CreatePaginatedRequest(method_name, params, Value::BIGINT(1));
		auto response = connection->SendRequest(method_name, params.ToRPCParams());

		if (response.IsError()) {
			throw IOException("MCP request failed: " + response.error.message);
		}

		// Parse the paginated response
		string items_field;
		if (method_name == "resources/list")
			items_field = "resources";
		else if (method_name == "prompts/list")
			items_field = "prompts";
		else if (method_name == "tools/list")
			items_field = "tools";
		else
			throw InvalidInputException("Unsupported pagination method: " + method_name);

		current_result = MCPPagination::ParsePaginationResponse(response, items_field);

		// Update state
		is_initialized = true;
		if (!current_result.HasNextPage()) {
			is_finished = true;
		} else {
			params.cursor = current_result.GetNextCursor();
		}

		MCP_LOG_DEBUG("PAGINATION", "Fetched page with %llu items, next_cursor: %s", current_result.items.size(),
		              current_result.next_cursor.c_str());

		return current_result;

	} catch (const std::exception &e) {
		is_finished = true;
		MCP_LOG_ERROR("PAGINATION", "Iterator failed: %s", e.what());
		throw;
	}
}

void MCPPaginationIterator::Reset() {
	params.cursor = "";
	is_initialized = false;
	is_finished = false;
	current_result = MCPPaginationResult();
}

vector<Value> MCPPaginationIterator::FetchAll() {
	vector<Value> all_items;

	Reset(); // Start from beginning

	while (HasNext()) {
		auto result = Next();
		for (const auto &item : result.items) {
			all_items.push_back(item);
		}
	}

	MCP_LOG_INFO("PAGINATION", "Fetched all %llu items from %s", all_items.size(), method_name.c_str());
	return all_items;
}

idx_t MCPPaginationIterator::GetTotalFetched() const {
	return current_result.total_items;
}

string MCPPaginationIterator::GetLastError() const {
	// TODO: Implement error tracking
	return "";
}

#endif // !__EMSCRIPTEN__

// Pagination utilities implementation
namespace MCPPagination {

MCPPaginationResult ParsePaginationResponse(const MCPMessage &response, const string &items_field) {
	MCPPaginationResult result;

	if (response.IsError()) {
		throw IOException("Cannot parse pagination response: " + response.error.message);
	}

	try {
		// Parse the JSON response
		string json_str = response.result.ToString();
		yyjson_doc *doc = yyjson_read(json_str.c_str(), json_str.length(), 0);

		if (!doc) {
			throw InvalidInputException("Invalid JSON in MCP response");
		}

		yyjson_val *root = yyjson_doc_get_root(doc);
		if (!yyjson_is_obj(root)) {
			yyjson_doc_free(doc);
			throw InvalidInputException("Expected object in MCP response");
		}

		// Extract items array
		yyjson_val *items_val = yyjson_obj_get(root, items_field.c_str());
		if (items_val && yyjson_is_arr(items_val)) {
			yyjson_arr_iter iter;
			yyjson_arr_iter_init(items_val, &iter);
			yyjson_val *item;

			while ((item = yyjson_arr_iter_next(&iter))) {
				// Convert each item to DuckDB Value
				size_t len;
				char *item_json = yyjson_val_write(item, 0, &len);
				result.items.push_back(Value(item_json));
				free(item_json);
			}
		}

		// Extract nextCursor
		yyjson_val *cursor_val = yyjson_obj_get(root, "nextCursor");
		if (cursor_val && yyjson_is_str(cursor_val)) {
			result.next_cursor = yyjson_get_str(cursor_val);
			result.has_more_pages = !result.next_cursor.empty();
		}

		result.total_items = result.items.size();

		yyjson_doc_free(doc);
		return result;

	} catch (const std::exception &e) {
		MCP_LOG_ERROR("PAGINATION", "Failed to parse pagination response: %s", e.what());
		throw InvalidInputException("Failed to parse pagination response: " + string(e.what()));
	}
}

MCPMessage CreatePaginatedRequest(const string &method, const MCPPaginationParams &params, const Value &id) {
	return MCPMessage::CreateRequest(method, params.ToRPCParams(), id);
}

bool IsValidCursor(const string &cursor) {
	// Basic cursor validation - non-empty and reasonable length
	return !cursor.empty() && cursor.length() < 1024;
}

} // namespace MCPPagination

#ifndef __EMSCRIPTEN__

// MCPConnectionWithPagination implementation
MCPPaginationResult MCPConnectionWithPagination::ListResources(const MCPPaginationParams &params) {
	auto request = MCPPagination::CreatePaginatedRequest("resources/list", params, Value::BIGINT(1));
	auto response = connection->SendRequest("resources/list", params.ToRPCParams());

	if (response.IsError()) {
		throw IOException("Resources list request failed: " + response.error.message);
	}

	return MCPPagination::ParsePaginationResponse(response, "resources");
}

MCPPaginationResult MCPConnectionWithPagination::ListPrompts(const MCPPaginationParams &params) {
	auto request = MCPPagination::CreatePaginatedRequest("prompts/list", params, Value::BIGINT(2));
	auto response = connection->SendRequest("prompts/list", params.ToRPCParams());

	if (response.IsError()) {
		throw IOException("Prompts list request failed: " + response.error.message);
	}

	return MCPPagination::ParsePaginationResponse(response, "prompts");
}

MCPPaginationResult MCPConnectionWithPagination::ListTools(const MCPPaginationParams &params) {
	auto request = MCPPagination::CreatePaginatedRequest("tools/list", params, Value::BIGINT(3));
	auto response = connection->SendRequest("tools/list", params.ToRPCParams());

	if (response.IsError()) {
		throw IOException("Tools list request failed: " + response.error.message);
	}

	return MCPPagination::ParsePaginationResponse(response, "tools");
}

unique_ptr<MCPPaginationIterator> MCPConnectionWithPagination::CreateResourcesIterator() {
	return make_uniq<MCPPaginationIterator>(connection->GetServerName(), "resources/list");
}

unique_ptr<MCPPaginationIterator> MCPConnectionWithPagination::CreatePromptsIterator() {
	return make_uniq<MCPPaginationIterator>(connection->GetServerName(), "prompts/list");
}

unique_ptr<MCPPaginationIterator> MCPConnectionWithPagination::CreateToolsIterator() {
	return make_uniq<MCPPaginationIterator>(connection->GetServerName(), "tools/list");
}

#endif // !__EMSCRIPTEN__

} // namespace duckdb
