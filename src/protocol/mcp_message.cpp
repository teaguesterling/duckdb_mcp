#include "protocol/mcp_message.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/connection.hpp"

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
    msg.type = MCPMessageType::ERROR;
    msg.has_error = true;
    msg.error.code = code;
    msg.error.message = message;
    msg.error.data = data;
    msg.id = id;
    return msg;
}

string MCPMessage::ToJSON() const {
    // Basic JSON serialization using DuckDB's Value serialization
    string json = "{";
    json += "\"jsonrpc\":\"2.0\"";
    
    if (type == MCPMessageType::REQUEST || type == MCPMessageType::NOTIFICATION) {
        json += ",\"method\":\"" + method + "\"";
        
        // Simple params serialization - avoid ToString() to prevent cast issues
        if (!params.IsNull()) {
            // For initialize method, use proper MCP parameters
            if (method == "initialize") {
                json += ",\"params\":{\"protocolVersion\":\"2024-11-05\",\"clientInfo\":{\"name\":\"DuckDB MCP Extension\",\"version\":\"0.1.0\"},\"capabilities\":{\"roots\":{},\"sampling\":{}}}";
            } else if (method == "resources/read") {
                // For resource reading, extract URI from params struct
                if (params.type().id() == LogicalTypeId::STRUCT) {
                    auto &struct_values = StructValue::GetChildren(params);
                    for (size_t i = 0; i < struct_values.size(); i++) {
                        auto &key = StructType::GetChildName(params.type(), i);
                        if (key == "uri") {
                            string uri = struct_values[i].ToString();
                            json += ",\"params\":{\"uri\":\"" + uri + "\"}";
                            break;
                        }
                    }
                } else {
                    json += ",\"params\":{}";
                }
            } else if (method == "tools/call") {
                // For tool calling, extract name and arguments from params struct
                if (params.type().id() == LogicalTypeId::STRUCT) {
                    string tool_name, arguments = "{}";
                    auto &struct_values = StructValue::GetChildren(params);
                    for (size_t i = 0; i < struct_values.size(); i++) {
                        auto &key = StructType::GetChildName(params.type(), i);
                        if (key == "name") {
                            tool_name = struct_values[i].ToString();
                        } else if (key == "arguments") {
                            string arg_str = struct_values[i].ToString();
                            // If it looks like JSON, use it directly, otherwise wrap in quotes
                            if (arg_str.empty() || arg_str == "{}") {
                                arguments = "{}";
                            } else if (arg_str[0] == '{' || arg_str[0] == '[') {
                                arguments = arg_str;  // Already JSON
                            } else {
                                arguments = "\"" + arg_str + "\"";  // Wrap as string
                            }
                        }
                    }
                    if (!tool_name.empty()) {
                        json += ",\"params\":{\"name\":\"" + tool_name + "\",\"arguments\":" + arguments + "}";
                    } else {
                        json += ",\"params\":{}";
                    }
                } else {
                    json += ",\"params\":{}";
                }
            } else {
                json += ",\"params\":{}";
            }
        }
        
        if (type == MCPMessageType::REQUEST && !id.IsNull()) {
            json += ",\"id\":" + id.ToString();
        }
    } else if (type == MCPMessageType::RESPONSE) {
        if (has_error) {
            json += ",\"error\":{";
            json += "\"code\":" + std::to_string(error.code);
            json += ",\"message\":\"" + error.message + "\"";
            if (!error.data.IsNull()) {
                json += ",\"data\":" + error.data.ToString();
            }
            json += "}";
        } else {
            json += ",\"result\":" + result.ToString();
        }
        
        if (!id.IsNull()) {
            json += ",\"id\":" + id.ToString();
        }
    }
    
    json += "}";
    return json;
}

MCPMessage MCPMessage::FromJSON(const string &json) {
    // Basic JSON parsing for MCP protocol - in reality would use proper JSON parser
    MCPMessage msg;
    
    // Determine message type
    if (json.find("\"method\"") != string::npos) {
        if (json.find("\"id\"") != string::npos) {
            msg.type = MCPMessageType::REQUEST;
        } else {
            msg.type = MCPMessageType::NOTIFICATION;
        }
    } else {
        msg.type = MCPMessageType::RESPONSE;
    }
    
    // Extract method name for requests/notifications
    auto method_pos = json.find("\"method\":\"");
    if (method_pos != string::npos) {
        method_pos += 10; // len("\"method\":\"")
        auto method_end = json.find("\"", method_pos);
        if (method_end != string::npos) {
            msg.method = json.substr(method_pos, method_end - method_pos);
        }
    }
    
    // Extract error for error responses
    auto error_pos = json.find("\"error\":{");
    if (error_pos != string::npos) {
        msg.has_error = true;
        msg.type = MCPMessageType::ERROR;
        
        // Extract error code
        auto code_pos = json.find("\"code\":", error_pos);
        if (code_pos != string::npos) {
            code_pos += 7; // len("\"code\":")
            auto code_end = json.find_first_of(",}", code_pos);
            if (code_end != string::npos) {
                string code_str = json.substr(code_pos, code_end - code_pos);
                msg.error.code = std::stoi(code_str);
            }
        }
        
        // Extract error message
        auto message_pos = json.find("\"message\":\"", error_pos);
        if (message_pos != string::npos) {
            message_pos += 11; // len("\"message\":\"")
            auto message_end = json.find("\"", message_pos);
            if (message_end != string::npos) {
                msg.error.message = json.substr(message_pos, message_end - message_pos);
            }
        }
    }
    
    // Extract result for successful responses
    auto result_pos = json.find("\"result\":");
    if (result_pos != string::npos && !msg.has_error) {
        result_pos += 9; // len("\"result\":")
        
        // Find the start of the result value
        while (result_pos < json.length() && isspace(json[result_pos])) {
            result_pos++;
        }
        
        // Extract the result JSON (basic bracket counting)
        if (result_pos < json.length()) {
            size_t result_end = result_pos;
            int bracket_count = 0;
            bool in_string = false;
            bool escaped = false;
            
            for (size_t i = result_pos; i < json.length(); i++) {
                char c = json[i];
                
                if (escaped) {
                    escaped = false;
                    continue;
                }
                
                if (c == '\\') {
                    escaped = true;
                    continue;
                }
                
                if (c == '"') {
                    in_string = !in_string;
                    continue;
                }
                
                if (!in_string) {
                    if (c == '{' || c == '[') {
                        bracket_count++;
                    } else if (c == '}' || c == ']') {
                        bracket_count--;
                        if (bracket_count == 0) {
                            result_end = i + 1;
                            break;
                        }
                    } else if (bracket_count == 0 && (c == ',' || c == '}')) {
                        result_end = i;
                        break;
                    }
                }
            }
            
            string result_json = json.substr(result_pos, result_end - result_pos);
            
            // For now, store the raw JSON as a VARCHAR Value
            // TODO: Parse into proper DuckDB Value structure
            msg.result = Value(result_json);
        }
    }
    
    return msg;
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