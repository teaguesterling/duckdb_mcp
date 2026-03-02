#include "protocol/mcp_connection.hpp"
#include "duckdb/common/exception.hpp"
#include <ctime>
#include <thread>
#include <chrono>

namespace duckdb {

MCPConnection::MCPConnection(const string &server_name, unique_ptr<MCPTransport> transport)
    : server_name(server_name), transport(std::move(transport)), state(MCPConnectionState::DISCONNECTED),
      next_request_id(1), is_recoverable_error(false), consecutive_failures(0), last_activity_time(time(nullptr)) {
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
			SetError("Failed to establish transport connection", true);
			state = MCPConnectionState::ERROR;
			RecordFailure();
			return false;
		}

		state = MCPConnectionState::CONNECTED;
		RecordSuccess();
		return true;

	} catch (const std::exception &e) {
		SetError("Connection error: " + string(e.what()), true);
		state = MCPConnectionState::ERROR;
		RecordFailure();
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
	return current_state == MCPConnectionState::CONNECTED || current_state == MCPConnectionState::INITIALIZED;
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
	// Store the raw JSON response for MCP functions,
	// MCPFS will extract content as needed
	if (!response.result.IsNull()) {
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

	Value params = Value::STRUCT({{"name", Value(name)}, {"arguments", arguments}});

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

	MCPMessage response;
	if (!SendRequestWithRetry(method, params, response)) {
		throw IOException("Request failed after retries: " + GetLastError());
	}

	return response;
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
	// Use empty struct since we'll manually construct JSON in ToJSON()
	Value params = Value::STRUCT({});

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

void MCPConnection::SetError(const string &error, bool recoverable) {
	lock_guard<mutex> lock(error_mutex);
	last_error = error;
	is_recoverable_error = recoverable;
}

string MCPConnection::GetLastError() const {
	lock_guard<mutex> lock(error_mutex);
	return last_error;
}

void MCPConnection::HandleTransportError(const string &operation) {
	SetError("Transport error during " + operation, true);
	state = MCPConnectionState::ERROR;
}

bool MCPConnection::HasRecoverableError() const {
	return is_recoverable_error.load();
}

void MCPConnection::ClearError() {
	lock_guard<mutex> lock(error_mutex);
	last_error.clear();
	is_recoverable_error = false;
}

bool MCPConnection::IsHealthy() const {
	auto current_state = GetState();
	return (current_state == MCPConnectionState::CONNECTED || current_state == MCPConnectionState::INITIALIZED) &&
	       consecutive_failures.load() < 3;
}

void MCPConnection::RecordSuccess() {
	consecutive_failures = 0;
	last_activity_time = time(nullptr);
	ClearError();
}

void MCPConnection::RecordFailure() {
	consecutive_failures.fetch_add(1);
	last_activity_time = time(nullptr);
}

bool MCPConnection::SendRequestWithRetry(const string &method, const Value &params, MCPMessage &response,
                                         int max_retries) {
	auto request_id = GenerateRequestId();
	auto request = MCPMessage::CreateRequest(method, params, request_id);

	for (int attempt = 0; attempt <= max_retries; attempt++) {
		try {
			if (!IsConnected()) {
				// Try to reconnect for recoverable errors
				if (attempt > 0 && HasRecoverableError()) {
					if (!Connect()) {
						continue; // Try next attempt
					}
					if (!Initialize()) {
						continue; // Try next attempt
					}
				} else {
					SetError("Connection not established", false);
					return false;
				}
			}

			response = transport->SendAndReceive(request);
			RecordSuccess();
			return true;

		} catch (const std::exception &e) {
			RecordFailure();
			SetError("Request attempt " + std::to_string(attempt + 1) + " failed: " + string(e.what()), true);

			// Don't retry for the last attempt
			if (attempt == max_retries) {
				break;
			}

			// Brief delay before retry (exponential backoff)
			std::this_thread::sleep_for(std::chrono::milliseconds(100 * (1 << attempt)));
		}
	}

	return false;
}

} // namespace duckdb
