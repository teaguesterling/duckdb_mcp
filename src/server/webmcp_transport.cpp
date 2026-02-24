#ifdef __EMSCRIPTEN__

#include "server/webmcp_transport.hpp"
#include "server/mcp_server.hpp"
#include "server/tool_handlers.hpp"
#include "server/resource_providers.hpp"
#include "protocol/mcp_message.hpp"
#include "json_utils.hpp"
#include "yyjson.hpp"
#include "duckdb/common/exception.hpp"
#include <emscripten.h>

using namespace duckdb_yyjson;

namespace duckdb {

//===--------------------------------------------------------------------===//
// Global singleton for JS callback routing
//===--------------------------------------------------------------------===//

static WebMCPTransport *g_active_webmcp_transport = nullptr;

WebMCPTransport *GetActiveWebMCPTransport() {
	return g_active_webmcp_transport;
}

void SetActiveWebMCPTransport(WebMCPTransport *transport) {
	g_active_webmcp_transport = transport;
}

//===--------------------------------------------------------------------===//
// EM_JS functions — JS interop for navigator.modelContext
//===--------------------------------------------------------------------===//

// Check if navigator.modelContext is available
EM_JS(int, webmcp_check_available, (), {
	if (typeof navigator !== 'undefined' && navigator.modelContext) {
		return 1;
	}
	return 0;
});

// Register a tool with navigator.modelContext
// The execute callback calls back into WASM via Module._webmcp_handle_tool_call
EM_JS(int, webmcp_register_tool_js, (const char *name_ptr, const char *desc_ptr,
                                      const char *schema_ptr, int read_only), {
	if (!navigator.modelContext) return 0;
	try {
		var name = UTF8ToString(name_ptr);
		var description = UTF8ToString(desc_ptr);
		var schemaStr = UTF8ToString(schema_ptr);
		var inputSchema = JSON.parse(schemaStr);
		var isReadOnly = read_only !== 0;

		navigator.modelContext.registerTool({
			name: name,
			description: description,
			inputSchema: inputSchema,
			annotations: {
				readOnlyHint: isReadOnly
			},
			execute: async function(input) {
				// Convert input to JSON string for C++
				var argsJson = JSON.stringify(input);
				var namePtr = stringToNewUTF8(name);
				var argsPtr = stringToNewUTF8(argsJson);

				// Call into WASM synchronously
				var resultPtr = Module._webmcp_handle_tool_call(namePtr, argsPtr);
				var resultStr = UTF8ToString(resultPtr);

				// Free allocated strings
				Module._free(namePtr);
				Module._free(argsPtr);
				Module._free(resultPtr);

				// Parse the result
				var result = JSON.parse(resultStr);
				return result;
			}
		});
		return 1;
	} catch (e) {
		console.error('WebMCP registerTool failed:', e);
		return 0;
	}
});

// Unregister a tool from navigator.modelContext
EM_JS(void, webmcp_unregister_tool_js, (const char *name_ptr), {
	if (!navigator.modelContext) return;
	try {
		var name = UTF8ToString(name_ptr);
		navigator.modelContext.unregisterTool(name);
	} catch (e) {
		console.error('WebMCP unregisterTool failed:', e);
	}
});

// Clear all tools from navigator.modelContext
EM_JS(void, webmcp_clear_context_js, (), {
	if (!navigator.modelContext) return;
	try {
		navigator.modelContext.clearContext();
	} catch (e) {
		console.error('WebMCP clearContext failed:', e);
	}
});

// List tools registered by other page scripts (requires webmcp_client.js interceptor)
EM_JS(const char*, webmcp_list_page_tools_js, (), {
	try {
		if (typeof window !== 'undefined' && window.__duckdb_webmcp_catalog) {
			var tools = window.__duckdb_webmcp_catalog.listTools();
			var json = JSON.stringify(tools);
			return stringToNewUTF8(json);
		}
	} catch (e) {
		console.error('WebMCP listPageTools failed:', e);
	}
	return stringToNewUTF8("[]");
});

//===--------------------------------------------------------------------===//
// Extern "C" callback — JS execute handler calls this
//===--------------------------------------------------------------------===//

} // namespace duckdb

extern "C" {

EMSCRIPTEN_KEEPALIVE
char* webmcp_handle_tool_call(const char *tool_name, const char *args_json) {
	using namespace duckdb;

	auto *transport = GetActiveWebMCPTransport();
	if (!transport || !transport->GetServer()) {
		// Return error as JSON
		const char *error = "{\"error\": \"No active WebMCP transport\"}";
		char *result = (char *)malloc(strlen(error) + 1);
		strcpy(result, error);
		return result;
	}

	try {
		string result = transport->HandleToolCall(string(tool_name), string(args_json));
		char *c_result = (char *)malloc(result.size() + 1);
		memcpy(c_result, result.c_str(), result.size() + 1);
		return c_result;
	} catch (const std::exception &e) {
		string error = "{\"error\": \"" + string(e.what()) + "\"}";
		char *c_result = (char *)malloc(error.size() + 1);
		memcpy(c_result, error.c_str(), error.size() + 1);
		return c_result;
	}
}

} // extern "C"

namespace duckdb {

//===--------------------------------------------------------------------===//
// WebMCPTransport Implementation
//===--------------------------------------------------------------------===//

WebMCPTransport::WebMCPTransport(MCPServer *server, const WebMCPConfig &config)
    : server(server), config(config), active(false) {
}

WebMCPTransport::~WebMCPTransport() {
	Deactivate();
}

bool WebMCPTransport::IsAvailable() {
	return webmcp_check_available() == 1;
}

bool WebMCPTransport::Activate() {
	if (active) {
		return true;
	}

	if (!server) {
		return false;
	}

	if (!IsAvailable()) {
		return false;
	}

	// Set ourselves as the active transport for JS callbacks
	SetActiveWebMCPTransport(this);

	// Sync all current tools to WebMCP
	SyncTools();

	active = true;
	return true;
}

void WebMCPTransport::Deactivate() {
	if (!active) {
		return;
	}

	// Unregister all tools
	for (const auto &name : registered_tool_names) {
		UnregisterWebMCPTool(name);
	}
	registered_tool_names.clear();

	// Clear context as well
	webmcp_clear_context_js();

	// Clear global pointer
	if (GetActiveWebMCPTransport() == this) {
		SetActiveWebMCPTransport(nullptr);
	}

	active = false;
}

bool WebMCPTransport::IsActive() const {
	return active;
}

void WebMCPTransport::SyncTools() {
	if (!server) {
		return;
	}

	// Get current MCP tools
	auto tool_names = server->ListRegisteredTools();

	// Build set of desired tool names
	unordered_set<string> desired_tools(tool_names.begin(), tool_names.end());

	// Add wrapper tool names if configured
	if (config.wrap_resources) {
		desired_tools.insert("read_resource");
	}
	if (config.wrap_prompts) {
		desired_tools.insert("get_prompt");
	}

	// Unregister tools that are no longer in the registry
	vector<string> to_remove;
	for (const auto &name : registered_tool_names) {
		if (desired_tools.find(name) == desired_tools.end()) {
			UnregisterWebMCPTool(name);
			to_remove.push_back(name);
		}
	}
	for (const auto &name : to_remove) {
		registered_tool_names.erase(
		    std::remove(registered_tool_names.begin(), registered_tool_names.end(), name),
		    registered_tool_names.end());
	}

	// Build set of already-registered names for quick lookup
	unordered_set<string> already_registered(registered_tool_names.begin(), registered_tool_names.end());

	// Register new MCP tools
	for (const auto &name : tool_names) {
		if (already_registered.find(name) != already_registered.end()) {
			continue; // Already registered
		}

		// Get tool handler to extract metadata
		// We need to go through the server's tool list handler to get schema
		// Build a tools/call JSON-RPC to get tool info
		auto tool_names_list = server->ListRegisteredTools();

		// For each tool, we need description and schema.
		// Use ProcessRequest with tools/list to get the tool metadata.
		MCPMessage list_request;
		list_request.jsonrpc = "2.0";
		list_request.method = "tools/list";
		list_request.id = Value(1);
		list_request.params = Value::STRUCT({});

		MCPMessage list_response = server->ProcessRequest(list_request);

		// Parse the response to find our specific tool
		string response_json = list_response.ToJSON();
		yyjson_doc *doc = JSONUtils::Parse(response_json);
		yyjson_val *root = yyjson_doc_get_root(doc);
		yyjson_val *result_obj = yyjson_obj_get(root, "result");
		yyjson_val *tools_arr = result_obj ? yyjson_obj_get(result_obj, "tools") : nullptr;

		if (tools_arr && yyjson_is_arr(tools_arr)) {
			size_t idx, max;
			yyjson_val *tool_val;
			yyjson_arr_foreach(tools_arr, idx, max, tool_val) {
				const char *tool_name_str = yyjson_get_str(yyjson_obj_get(tool_val, "name"));
				if (!tool_name_str || string(tool_name_str) != name) {
					continue;
				}

				const char *desc_str = yyjson_get_str(yyjson_obj_get(tool_val, "description"));
				string description = desc_str ? desc_str : "";

				yyjson_val *schema_val = yyjson_obj_get(tool_val, "inputSchema");
				string schema_json = "{}";
				if (schema_val) {
					size_t len;
					char *schema_str = yyjson_val_write(schema_val, 0, &len);
					if (schema_str) {
						schema_json = string(schema_str, len);
						free(schema_str);
					}
				}

				// Determine read-only hint based on tool name
				// query, describe, list_tables, database_info, read_resource = read-only
				// export, execute = not read-only
				bool read_only = (name == "query" || name == "describe" ||
				                  name == "list_tables" || name == "database_info" ||
				                  name == "read_resource" || name == "get_prompt");

				if (RegisterWebMCPTool(name, description, schema_json, read_only)) {
					registered_tool_names.push_back(name);
				}
				break;
			}
		}

		JSONUtils::FreeDocument(doc);
	}

	// Register wrapper tools
	if (config.wrap_resources && already_registered.find("read_resource") == already_registered.end()) {
		RegisterResourceWrapper();
	}
	if (config.wrap_prompts && already_registered.find("get_prompt") == already_registered.end()) {
		RegisterPromptWrapper();
	}
}

string WebMCPTransport::HandleToolCall(const string &tool_name, const string &arguments_json) {
	if (!server) {
		return "{\"error\": \"No server\"}";
	}

	// Build a tools/call JSON-RPC request
	// The params need to be a JSON string with "name" and "arguments"
	yyjson_mut_doc *doc = JSONUtils::CreateDocument();
	yyjson_mut_val *params = yyjson_mut_obj(doc);
	yyjson_mut_obj_add_str(doc, params, "name", tool_name.c_str());

	// Parse arguments_json into a JSON value and add it
	yyjson_doc *args_doc = yyjson_read(arguments_json.c_str(), arguments_json.length(), 0);
	if (args_doc) {
		yyjson_val *args_root = yyjson_doc_get_root(args_doc);
		yyjson_mut_val *args_mut = yyjson_val_mut_copy(doc, args_root);
		yyjson_mut_obj_add_val(doc, params, "arguments", args_mut);
		yyjson_doc_free(args_doc);
	} else {
		// If we can't parse the args, pass as empty object
		yyjson_mut_val *empty_obj = yyjson_mut_obj(doc);
		yyjson_mut_obj_add_val(doc, params, "arguments", empty_obj);
	}

	yyjson_mut_doc_set_root(doc, params);
	string params_json = JSONUtils::Serialize(doc);
	JSONUtils::FreeDocument(doc);

	// Create MCP message
	MCPMessage request;
	request.jsonrpc = "2.0";
	request.method = "tools/call";
	request.id = Value(1);
	request.params = Value(params_json);

	// Process through the server
	MCPMessage response = server->ProcessRequest(request);

	// Extract content from response
	string response_json = response.ToJSON();

	// Parse response to extract the text content
	yyjson_doc *resp_doc = JSONUtils::Parse(response_json);
	yyjson_val *resp_root = yyjson_doc_get_root(resp_doc);

	string result_text;

	// Check for error
	yyjson_val *error_val = yyjson_obj_get(resp_root, "error");
	if (error_val) {
		const char *err_msg = yyjson_get_str(yyjson_obj_get(error_val, "message"));
		result_text = err_msg ? err_msg : "Unknown error";
		JSONUtils::FreeDocument(resp_doc);

		// Return error as structured result for WebMCP
		yyjson_mut_doc *err_doc = JSONUtils::CreateDocument();
		yyjson_mut_val *err_root = yyjson_mut_obj(err_doc);
		yyjson_mut_obj_add_str(err_doc, err_root, "error", result_text.c_str());
		yyjson_mut_doc_set_root(err_doc, err_root);
		string err_json = JSONUtils::Serialize(err_doc);
		JSONUtils::FreeDocument(err_doc);
		return err_json;
	}

	// Extract content[0].text from the result
	yyjson_val *result_obj = yyjson_obj_get(resp_root, "result");
	yyjson_val *content_arr = result_obj ? yyjson_obj_get(result_obj, "content") : nullptr;
	if (content_arr && yyjson_is_arr(content_arr)) {
		yyjson_val *first_item = yyjson_arr_get_first(content_arr);
		if (first_item) {
			const char *text = yyjson_get_str(yyjson_obj_get(first_item, "text"));
			if (text) {
				result_text = text;
			}
		}
	}

	JSONUtils::FreeDocument(resp_doc);

	// Return result as structured JSON for WebMCP execute callback
	yyjson_mut_doc *result_doc = JSONUtils::CreateDocument();
	yyjson_mut_val *result_root = yyjson_mut_obj(result_doc);
	yyjson_mut_obj_add_str(result_doc, result_root, "content", result_text.c_str());
	yyjson_mut_doc_set_root(result_doc, result_root);
	string final_json = JSONUtils::Serialize(result_doc);
	JSONUtils::FreeDocument(result_doc);

	return final_json;
}

string WebMCPTransport::ListPageTools() {
	const char *json = webmcp_list_page_tools_js();
	string result(json);
	free((void *)json);
	return result;
}

bool WebMCPTransport::RegisterWebMCPTool(const string &name, const string &description,
                                          const string &schema_json, bool read_only) {
	return webmcp_register_tool_js(name.c_str(), description.c_str(),
	                                schema_json.c_str(), read_only ? 1 : 0) == 1;
}

void WebMCPTransport::UnregisterWebMCPTool(const string &name) {
	webmcp_unregister_tool_js(name.c_str());
}

void WebMCPTransport::RegisterResourceWrapper() {
	// Build description with available resource URIs
	auto resource_uris = server->ListPublishedResources();

	string description = "Read a published MCP resource by URI. Available resources: ";
	if (resource_uris.empty()) {
		description += "(none)";
	} else {
		for (size_t i = 0; i < resource_uris.size(); i++) {
			if (i > 0) description += ", ";
			description += resource_uris[i];
		}
	}

	// Build input schema
	string schema_json = R"({
		"type": "object",
		"properties": {
			"uri": {
				"type": "string",
				"description": "The URI of the resource to read"
			}
		},
		"required": ["uri"]
	})";

	if (RegisterWebMCPTool("read_resource", description, schema_json, true)) {
		registered_tool_names.push_back("read_resource");
	}
}

void WebMCPTransport::RegisterPromptWrapper() {
	string description = "Get an MCP prompt template by name. "
	                     "Use webmcp_list_page_tools() or tools/list to discover available prompts.";

	string schema_json = R"({
		"type": "object",
		"properties": {
			"name": {
				"type": "string",
				"description": "The name of the prompt template"
			},
			"arguments": {
				"type": "object",
				"description": "Arguments to pass to the prompt template"
			}
		},
		"required": ["name"]
	})";

	if (RegisterWebMCPTool("get_prompt", description, schema_json, true)) {
		registered_tool_names.push_back("get_prompt");
	}
}

} // namespace duckdb

#endif // __EMSCRIPTEN__
