#include "protocol/mcp_connection.hpp"
#include "duckdb/common/exception.hpp"

namespace duckdb {

MCPConnection::MCPConnection(const string &server_name, unique_ptr<MCPTransport> transport)
    : server_name(server_name), transport(std::move(transport)), 
      state(MCPConnectionState::DISCONNECTED), next_request_id(1) {
}

MCPConnection::~MCPConnection() {
    if (IsConnected()) {
        Disconnect();
    }
}

bool MCPConnection::Connect() {
    lock_guard<mutex> lock(connection_mutex);
    
    if (state == MCPConnectionState::CONNECTED || state == MCPConnectionState::INITIALIZED) {
        return true;
    }
    
    state = MCPConnectionState::CONNECTING;
    
    try {
        if (!transport->Connect()) {
            SetError("Failed to establish transport connection");
            state = MCPConnectionState::ERROR;
            return false;
        }
        
        state = MCPConnectionState::CONNECTED;
        return true;
        
    } catch (const std::exception &e) {
        SetError("Connection error: " + string(e.what()));
        state = MCPConnectionState::ERROR;
        return false;
    }
}

void MCPConnection::Disconnect() {
    lock_guard<mutex> lock(connection_mutex);
    
    if (transport) {
        transport->Disconnect();
    }
    
    state = MCPConnectionState::DISCONNECTED;
}

bool MCPConnection::IsConnected() const {
    auto current_state = state.load();
    return current_state == MCPConnectionState::CONNECTED || 
           current_state == MCPConnectionState::INITIALIZED;
}

bool MCPConnection::IsInitialized() const {
    return state.load() == MCPConnectionState::INITIALIZED;
}

MCPConnectionState MCPConnection::GetState() const {
    return state.load();
}

bool MCPConnection::Initialize() {
    if (!IsConnected()) {
        if (!Connect()) {
            return false;
        }
    }
    
    if (IsInitialized()) {
        return true;
    }
    
    try {
        if (!SendInitialize()) {
            SetError("Failed to send initialization request");
            return false;
        }
        
        if (!WaitForInitialized()) {
            SetError("Initialization failed or timed out");
            return false;
        }
        
        state = MCPConnectionState::INITIALIZED;
        return true;
        
    } catch (const std::exception &e) {
        SetError("Initialization error: " + string(e.what()));
        state = MCPConnectionState::ERROR;
        return false;
    }
}

vector<MCPResource> MCPConnection::ListResources(const string &cursor) {
    if (!IsInitialized()) {
        throw InvalidInputException("Connection not initialized");
    }
    
    Value params = Value::STRUCT({});
    if (!cursor.empty()) {
        // Would add cursor to params
    }
    
    auto response = SendRequest(MCPMethods::RESOURCES_LIST, params);
    
    if (response.IsError()) {
        throw IOException("Failed to list resources: " + response.error.message);
    }
    
    vector<MCPResource> resources;
    
    // Parse response.result into resources
    // Placeholder implementation - would parse JSON properly
    
    return resources;
}

MCPResource MCPConnection::ReadResource(const string &uri) {
    if (!IsInitialized()) {
        throw InvalidInputException("Connection not initialized");
    }
    
    // Create params with URI
    Value params = Value::STRUCT({{"uri", Value(uri)}});
    
    auto response = SendRequest(MCPMethods::RESOURCES_READ, params);
    
    if (response.IsError()) {
        if (response.error.code == MCPErrorCodes::RESOURCE_NOT_FOUND) {
            throw IOException("Resource not found: " + uri);
        }
        throw IOException("Failed to read resource: " + response.error.message);
    }
    
    MCPResource resource;
    resource.uri = uri;
    
    // Parse response.result into resource
    // Placeholder - would extract contents, metadata, etc.
    if (!response.result.IsNull()) {
        // For now, just store the result as content
        resource.content = response.result.ToString();
        resource.content_loaded = true;
        resource.size = resource.content.length();
    }
    
    return resource;
}

bool MCPConnection::ResourceExists(const string &uri) {
    try {
        ReadResource(uri);
        return true;
    } catch (...) {
        return false;
    }
}

vector<string> MCPConnection::ListTools() {
    if (!IsInitialized()) {
        throw InvalidInputException("Connection not initialized");
    }
    
    auto response = SendRequest(MCPMethods::TOOLS_LIST, Value::STRUCT({}));
    
    if (response.IsError()) {
        throw IOException("Failed to list tools: " + response.error.message);
    }
    
    vector<string> tools;
    // Parse response.result into tool names
    
    return tools;
}

Value MCPConnection::CallTool(const string &name, const Value &arguments) {
    if (!IsInitialized()) {
        throw InvalidInputException("Connection not initialized");
    }
    
    Value params = Value::STRUCT({
        {"name", Value(name)},
        {"arguments", arguments}
    });
    
    auto response = SendRequest(MCPMethods::TOOLS_CALL, params);
    
    if (response.IsError()) {
        throw IOException("Tool call failed: " + response.error.message);
    }
    
    return response.result;
}

string MCPConnection::GetConnectionInfo() const {
    return server_name + " (" + (transport ? transport->GetConnectionInfo() : "no transport") + ")";
}

bool MCPConnection::Ping() {
    if (!IsConnected()) {
        return false;
    }
    
    return transport->Ping();
}

MCPMessage MCPConnection::SendRequest(const string &method, const Value &params) {
    if (!IsConnected()) {
        throw InvalidInputException("Connection not established");
    }
    
    auto request_id = GenerateRequestId();
    auto request = MCPMessage::CreateRequest(method, params, request_id);
    
    try {
        return transport->SendAndReceive(request);
    } catch (const std::exception &e) {
        throw IOException("Request failed: " + string(e.what()));
    }
}

bool MCPConnection::SendNotification(const string &method, const Value &params) {
    if (!IsConnected()) {
        return false;
    }
    
    auto notification = MCPMessage::CreateNotification(method, params);
    
    try {
        transport->Send(notification);
        return true;
    } catch (...) {
        return false;
    }
}

Value MCPConnection::GenerateRequestId() {
    return Value::BIGINT(next_request_id.fetch_add(1));
}

bool MCPConnection::SendInitialize() {
    Value client_info = Value::STRUCT({
        {"name", Value("DuckDB MCP Extension")},
        {"version", Value("0.1.0")}
    });
    
    Value capabilities = Value::STRUCT({
        {"roots", Value::LIST(LogicalType::VARCHAR, {})},
        {"sampling", Value::STRUCT({})}
    });
    
    Value params = Value::STRUCT({
        {"protocolVersion", Value("2024-11-05")},
        {"clientInfo", client_info},
        {"capabilities", capabilities}
    });
    
    auto response = SendRequest(MCPMethods::INITIALIZE, params);
    
    if (response.IsError()) {
        return false;
    }
    
    // Parse server capabilities
    ParseCapabilities(response.result);
    
    // Send initialized notification
    return SendNotification(MCPMethods::INITIALIZED, Value::STRUCT({}));
}

bool MCPConnection::WaitForInitialized() {
    // In a real implementation, we might wait for server responses
    // For now, assume initialization is immediate
    return true;
}

void MCPConnection::ParseCapabilities(const Value &server_info) {
    // Parse server capabilities from initialization response
    // Placeholder implementation
    capabilities.supports_resources = true;
    capabilities.supports_tools = false;
    capabilities.supports_prompts = false;
    capabilities.supports_sampling = false;
    
    // Would parse actual capabilities from server_info
}

void MCPConnection::SetError(const string &error) {
    last_error = error;
}

void MCPConnection::HandleTransportError(const string &operation) {
    SetError("Transport error during " + operation);
    state = MCPConnectionState::ERROR;
}

} // namespace duckdb