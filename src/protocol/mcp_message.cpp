#include "protocol/mcp_message.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/exception.hpp"

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
    // Basic JSON serialization - in a real implementation, we'd use a proper JSON library
    // For now, this is a simplified version for basic functionality
    
    string json = "{";
    json += "\"jsonrpc\":\"2.0\"";
    
    if (type == MCPMessageType::REQUEST || type == MCPMessageType::NOTIFICATION) {
        json += ",\"method\":\"" + method + "\"";
        
        // Simple params serialization (would need proper JSON encoder)
        if (!params.IsNull()) {
            json += ",\"params\":{";
            // Placeholder - would serialize params properly
            json += "}";
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
    // Placeholder implementation - in reality would use proper JSON parser
    MCPMessage msg;
    
    // Basic parsing for testing - would need proper JSON parsing
    if (json.find("\"method\"") != string::npos) {
        if (json.find("\"id\"") != string::npos) {
            msg.type = MCPMessageType::REQUEST;
        } else {
            msg.type = MCPMessageType::NOTIFICATION;
        }
    } else {
        msg.type = MCPMessageType::RESPONSE;
    }
    
    // Extract method name (basic regex-like parsing)
    auto method_pos = json.find("\"method\":\"");
    if (method_pos != string::npos) {
        method_pos += 10; // len("\"method\":\"")
        auto method_end = json.find("\"", method_pos);
        if (method_end != string::npos) {
            msg.method = json.substr(method_pos, method_end - method_pos);
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