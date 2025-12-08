#pragma once

#include "duckdb.hpp"

namespace duckdb {

// MCP JSON-RPC 2.0 message types
enum class MCPMessageType {
    REQUEST,
    RESPONSE,
    NOTIFICATION,
    ERROR
};

// MCP message structure
struct MCPMessage {
    MCPMessageType type = MCPMessageType::REQUEST;  // Default to REQUEST
    string jsonrpc = "2.0";  // Always "2.0" for JSON-RPC 2.0
    
    // Request/Notification fields
    string method;
    Value params;  // DuckDB Value can hold JSON-like structures
    Value id;      // Can be string, number, or null
    
    // Response fields
    Value result;
    
    // Error fields
    struct MCPError {
        int code;
        string message;
        Value data;
    } error;
    
    bool has_error = false;
    
    // Constructors
    MCPMessage() = default;
    
    // Create request
    static MCPMessage CreateRequest(const string &method, const Value &params, const Value &id);
    
    // Create notification
    static MCPMessage CreateNotification(const string &method, const Value &params);
    
    // Create response
    static MCPMessage CreateResponse(const Value &result, const Value &id);
    
    // Create error response
    static MCPMessage CreateError(int code, const string &message, const Value &id, const Value &data = Value());
    
    // Serialization
    string ToJSON() const;
    static MCPMessage FromJSON(const string &json);
    
    // Validation
    bool IsValid() const;
    bool IsRequest() const { return type == MCPMessageType::REQUEST; }
    bool IsResponse() const { return type == MCPMessageType::RESPONSE; }
    bool IsNotification() const { return type == MCPMessageType::NOTIFICATION; }
    bool IsError() const { return type == MCPMessageType::ERROR || has_error; }
};

// MCP protocol method names
namespace MCPMethods {
    // Initialization
    constexpr const char* INITIALIZE = "initialize";
    constexpr const char* INITIALIZED = "notifications/initialized";
    
    // Resources
    constexpr const char* RESOURCES_LIST = "resources/list";
    constexpr const char* RESOURCES_READ = "resources/read";
    constexpr const char* RESOURCES_SUBSCRIBE = "resources/subscribe";
    constexpr const char* RESOURCES_UNSUBSCRIBE = "resources/unsubscribe";
    
    // Tools
    constexpr const char* TOOLS_LIST = "tools/list";
    constexpr const char* TOOLS_CALL = "tools/call";
    
    // Prompts
    constexpr const char* PROMPTS_LIST = "prompts/list";
    constexpr const char* PROMPTS_GET = "prompts/get";
    
    // Sampling
    constexpr const char* SAMPLING_CREATE = "sampling/create";
    
    // Notifications
    constexpr const char* NOTIFICATIONS_CANCELLED = "notifications/cancelled";
    constexpr const char* NOTIFICATIONS_PROGRESS = "notifications/progress";
    constexpr const char* NOTIFICATIONS_MESSAGE = "notifications/message";
    
    // Ping
    constexpr const char* PING = "ping";
    
    // Server control
    constexpr const char* SHUTDOWN = "shutdown";
}

// Common MCP error codes
namespace MCPErrorCodes {
    constexpr int PARSE_ERROR = -32700;
    constexpr int INVALID_REQUEST = -32600;
    constexpr int METHOD_NOT_FOUND = -32601;
    constexpr int INVALID_PARAMS = -32602;
    constexpr int INTERNAL_ERROR = -32603;
    
    // MCP-specific errors
    constexpr int RESOURCE_NOT_FOUND = -32001;
    constexpr int TOOL_NOT_FOUND = -32002;
    constexpr int INVALID_TOOL_INPUT = -32003;
    constexpr int RESOURCE_ACCESS_DENIED = -32004;
}

} // namespace duckdb