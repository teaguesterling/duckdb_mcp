#include "protocol/mcp_message.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/connection.hpp"
#include "json_utils.hpp"

namespace duckdb {

MCPMessage MCPMessage::CreateRequest(const string &method, const Value &params, const Value &id) {
	MCPMessage msg;
	msg.type = MCPMessageType::REQUEST;
	msg.method = method;
	msg.params = params;
	msg.id = id;
	return msg;
}

MCPMessage MCPMessage::CreateNotification(const string &method, const Value &params) {
	MCPMessage msg;
	msg.type = MCPMessageType::NOTIFICATION;
	msg.method = method;
	msg.params = params;
	return msg;
}

MCPMessage MCPMessage::CreateResponse(const Value &result, const Value &id) {
	MCPMessage msg;
	msg.type = MCPMessageType::RESPONSE;
	msg.result = result;
	msg.id = id;
	return msg;
}

MCPMessage MCPMessage::CreateError(int code, const string &message, const Value &id, const Value &data) {
	MCPMessage msg;
	// Use RESPONSE type since JSON-RPC 2.0 uses the same structure for success/error responses
	// The has_error flag and error fields indicate this is an error response
	msg.type = MCPMessageType::RESPONSE;
	msg.has_error = true;
	msg.error.code = code;
	msg.error.message = message;
	msg.error.data = data;
	msg.id = id;
	return msg;
}

string MCPMessage::ToJSON() const {
	// Use proper JSON serialization with yyjson
	yyjson_mut_doc *doc = JSONUtils::CreateDocument();
	if (!doc) {
		throw InternalException("Failed to create JSON document");
	}

	try {
		yyjson_mut_val *msg = JSONUtils::CreateMCPMessage(doc);
		yyjson_mut_doc_set_root(doc, msg);

		if (type == MCPMessageType::REQUEST || type == MCPMessageType::NOTIFICATION) {
			JSONUtils::AddString(doc, msg, "method", method);

			// Handle parameters
			if (!params.IsNull()) {
				yyjson_mut_val *params_obj = nullptr;

				if (method == "initialize") {
					// Create proper MCP initialize parameters
					params_obj = JSONUtils::CreateObject(doc);
					JSONUtils::AddString(doc, params_obj, "protocolVersion", "2024-11-05");

					yyjson_mut_val *client_info = JSONUtils::CreateObject(doc);
					JSONUtils::AddString(doc, client_info, "name", "DuckDB MCP Extension");
					JSONUtils::AddString(doc, client_info, "version", "0.1.0");
					JSONUtils::AddObject(doc, params_obj, "clientInfo", client_info);

					yyjson_mut_val *capabilities = JSONUtils::CreateObject(doc);
					JSONUtils::AddObject(doc, capabilities, "roots", JSONUtils::CreateObject(doc));
					JSONUtils::AddObject(doc, capabilities, "sampling", JSONUtils::CreateObject(doc));
					JSONUtils::AddObject(doc, params_obj, "capabilities", capabilities);

				} else if (method == "resources/read") {
					// Extract URI from params struct
					params_obj = JSONUtils::CreateObject(doc);
					if (params.type().id() == LogicalTypeId::STRUCT) {
						auto &struct_values = StructValue::GetChildren(params);
						for (size_t i = 0; i < struct_values.size(); i++) {
							auto &key = StructType::GetChildName(params.type(), i);
							if (key == "uri") {
								JSONUtils::AddString(doc, params_obj, "uri", struct_values[i].ToString());
								break;
							}
						}
					}

				} else if (method == "tools/call") {
					// Extract tool name and arguments from params struct
					params_obj = JSONUtils::CreateObject(doc);
					if (params.type().id() == LogicalTypeId::STRUCT) {
						string tool_name;
						auto &struct_values = StructValue::GetChildren(params);
						for (size_t i = 0; i < struct_values.size(); i++) {
							auto &key = StructType::GetChildName(params.type(), i);
							if (key == "name") {
								tool_name = struct_values[i].ToString();
								JSONUtils::AddString(doc, params_obj, "name", tool_name);
							} else if (key == "arguments") {
								string arg_str = struct_values[i].ToString();
								// Try to parse arguments as JSON, fallback to empty object
								yyjson_doc *arg_doc = nullptr;
								yyjson_val *arg_val = nullptr;
								try {
									if (!arg_str.empty() && arg_str != "{}") {
										arg_doc = JSONUtils::Parse(arg_str);
										arg_val = yyjson_doc_get_root(arg_doc);
									}
								} catch (...) {
									// Invalid JSON, create empty object
								}

								if (arg_val) {
									// Copy the parsed JSON value
									yyjson_mut_val *arg_copy = yyjson_val_mut_copy(doc, arg_val);
									JSONUtils::AddObject(doc, params_obj, "arguments", arg_copy);
									JSONUtils::FreeDocument(arg_doc);
								} else {
									// Add empty object as default
									JSONUtils::AddObject(doc, params_obj, "arguments", JSONUtils::CreateObject(doc));
								}
							}
						}
					}
				} else {
					// Default empty params object
					params_obj = JSONUtils::CreateObject(doc);
				}

				if (params_obj) {
					JSONUtils::AddObject(doc, msg, "params", params_obj);
				}
			}

			// Add ID for requests
			if (type == MCPMessageType::REQUEST && !id.IsNull()) {
				yyjson_mut_val *id_val = JSONUtils::ValueToJSON(doc, id);
				JSONUtils::AddObject(doc, msg, "id", id_val);
			}

		} else if (type == MCPMessageType::RESPONSE) {
			// Handle response
			if (has_error) {
				yyjson_mut_val *error_obj = JSONUtils::CreateObject(doc);
				JSONUtils::AddInt(doc, error_obj, "code", error.code);
				JSONUtils::AddString(doc, error_obj, "message", error.message);

				if (!error.data.IsNull()) {
					yyjson_mut_val *data_val = JSONUtils::ValueToJSON(doc, error.data);
					JSONUtils::AddObject(doc, error_obj, "data", data_val);
				}

				JSONUtils::AddObject(doc, msg, "error", error_obj);
			} else {
				yyjson_mut_val *result_val = JSONUtils::ValueToJSON(doc, result);
				JSONUtils::AddObject(doc, msg, "result", result_val);
			}

			// Add ID
			if (!id.IsNull()) {
				yyjson_mut_val *id_val = JSONUtils::ValueToJSON(doc, id);
				JSONUtils::AddObject(doc, msg, "id", id_val);
			}
		}

		string json_str = JSONUtils::Serialize(doc);
		JSONUtils::FreeDocument(doc);
		return json_str;

	} catch (...) {
		JSONUtils::FreeDocument(doc);
		throw;
	}
}

MCPMessage MCPMessage::FromJSON(const string &json) {
	// Use proper JSON parsing with yyjson
	yyjson_doc *doc = nullptr;
	try {
		doc = JSONUtils::Parse(json);
		yyjson_val *root = yyjson_doc_get_root(doc);
		if (!root || !yyjson_is_obj(root)) {
			throw InvalidInputException("Invalid JSON message format");
		}

		MCPMessage msg;
		msg.jsonrpc = JSONUtils::GetString(root, "jsonrpc", "2.0");

		// Determine message type
		string method = JSONUtils::GetString(root, "method");
		yyjson_val *id_val = yyjson_obj_get(root, "id");
		yyjson_val *error_val = JSONUtils::GetObject(root, "error");
		yyjson_val *result_val = yyjson_obj_get(root, "result");

		if (!method.empty()) {
			// Request or notification
			msg.method = method;
			if (id_val) {
				msg.type = MCPMessageType::REQUEST;
				// Convert JSON value to DuckDB Value
				if (yyjson_is_str(id_val)) {
					msg.id = Value(JSONUtils::GetString(root, "id"));
				} else if (yyjson_is_int(id_val)) {
					msg.id = Value(JSONUtils::GetInt(root, "id"));
				} else {
					msg.id = Value(JSONUtils::GetString(root, "id"));
				}
			} else {
				msg.type = MCPMessageType::NOTIFICATION;
			}

			// Extract params
			yyjson_val *params_val = JSONUtils::GetObject(root, "params");
			if (params_val) {
				// For now, store params as raw JSON string
				size_t len;
				char *params_json = yyjson_val_write(params_val, 0, &len);
				if (params_json) {
					msg.params = Value(string(params_json, len));
					free(params_json);
				} else {
					msg.params = Value("{}");
				}
			}

		} else if (error_val) {
			// Error response
			msg.type = MCPMessageType::ERROR;
			msg.has_error = true;
			msg.error.code = JSONUtils::GetInt(error_val, "code", -1);
			msg.error.message = JSONUtils::GetString(error_val, "message");

			yyjson_val *data_val = yyjson_obj_get(error_val, "data");
			if (data_val) {
				size_t len;
				char *data_json = yyjson_val_write(data_val, 0, &len);
				if (data_json) {
					msg.error.data = Value(string(data_json, len));
					free(data_json);
				}
			}

			// Extract ID
			if (id_val) {
				if (yyjson_is_str(id_val)) {
					msg.id = Value(JSONUtils::GetString(root, "id"));
				} else if (yyjson_is_int(id_val)) {
					msg.id = Value(JSONUtils::GetInt(root, "id"));
				} else {
					msg.id = Value(JSONUtils::GetString(root, "id"));
				}
			}

		} else if (result_val) {
			// Success response
			msg.type = MCPMessageType::RESPONSE;

			// Store result as raw JSON string
			size_t len;
			char *result_json = yyjson_val_write(result_val, 0, &len);
			if (result_json) {
				msg.result = Value(string(result_json, len));
				free(result_json);
			} else {
				msg.result = Value("null");
			}

			// Extract ID
			if (id_val) {
				if (yyjson_is_str(id_val)) {
					msg.id = Value(JSONUtils::GetString(root, "id"));
				} else if (yyjson_is_int(id_val)) {
					msg.id = Value(JSONUtils::GetInt(root, "id"));
				} else {
					msg.id = Value(JSONUtils::GetString(root, "id"));
				}
			}
		}

		JSONUtils::FreeDocument(doc);
		return msg;

	} catch (...) {
		if (doc) {
			JSONUtils::FreeDocument(doc);
		}
		throw;
	}
}

bool MCPMessage::IsValid() const {
	if (jsonrpc != "2.0") {
		return false;
	}

	switch (type) {
	case MCPMessageType::REQUEST:
		return !method.empty() && !id.IsNull();
	case MCPMessageType::NOTIFICATION:
		return !method.empty();
	case MCPMessageType::RESPONSE:
		return !id.IsNull() && (has_error || !result.IsNull());
	case MCPMessageType::ERROR:
		return has_error && !id.IsNull();
	default:
		return false;
	}
}

} // namespace duckdb
