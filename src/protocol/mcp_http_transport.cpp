#include "protocol/mcp_http_transport.hpp"
#include "protocol/mcp_message.hpp"
#include "duckdb/common/http_util.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/database.hpp"

namespace duckdb {

HTTPTransport::HTTPTransport(const HTTPConfig &config) : config(config), is_connected(false) {
}

HTTPTransport::~HTTPTransport() {
	Disconnect();
}

bool HTTPTransport::Connect() {
	if (is_connected) {
		return true;
	}

	try {
		// Test connection with a basic ping
		auto ping_response = SendHTTPRequest("{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":1}");
		is_connected = ping_response != nullptr;
		return is_connected;
	} catch (const std::exception &e) {
		last_error = "Connection failed: " + string(e.what());
		return false;
	}
}

void HTTPTransport::Disconnect() {
	is_connected = false;
	last_error.clear();
}

void HTTPTransport::Send(const MCPMessage &message) {
	if (!is_connected) {
		throw IOException("Not connected to HTTP MCP server");
	}

	try {
		string json_data = message.ToJSON();
		auto response = SendHTTPRequest(json_data);
		if (!response || !response->Success()) {
			string error_msg = response ? response->GetError() : "No response";
			throw IOException("HTTP send failed: " + error_msg);
		}
	} catch (const std::exception &e) {
		last_error = "Send failed: " + string(e.what());
		throw IOException(last_error);
	}
}

MCPMessage HTTPTransport::Receive() {
	// For HTTP transport, we don't have a separate receive operation
	// Responses are handled synchronously in SendAndReceive
	throw NotImplementedException("HTTP transport uses synchronous request/response - use SendAndReceive instead");
}

MCPMessage HTTPTransport::SendAndReceive(const MCPMessage &request) {
	if (!is_connected) {
		throw IOException("Not connected to HTTP MCP server");
	}

	try {
		string json_data = request.ToJSON();
		auto http_response = SendHTTPRequest(json_data);

		if (!http_response || !http_response->Success()) {
			string error_msg = http_response ? http_response->GetError() : "No response";
			throw IOException("HTTP request failed: " + error_msg);
		}

		// Parse the JSON response into an MCP message
		return MCPMessage::FromJSON(http_response->body);
	} catch (const std::exception &e) {
		last_error = "Request failed: " + string(e.what());
		throw IOException(last_error);
	}
}

bool HTTPTransport::Ping() {
	if (!is_connected) {
		return false;
	}

	try {
		// Send a simple ping request
		MCPMessage ping_msg;
		ping_msg.method = "ping";
		ping_msg.id = 1;

		auto response = SendAndReceive(ping_msg);
		return !response.IsError();
	} catch (const std::exception &e) {
		last_error = "Ping failed: " + string(e.what());
		return false;
	}
}

string HTTPTransport::GetConnectionInfo() const {
	return "HTTP MCP Server at " + config.endpoint_url + (is_connected ? " (connected)" : " (disconnected)");
}

bool HTTPTransport::IsConnected() const {
	return is_connected;
}

unique_ptr<HTTPResponse> HTTPTransport::SendHTTPRequest(const string &json_data) {
	// Get HTTP utility from database
	auto &http_util = HTTPUtil::Get(*config.db_instance);

	// Initialize HTTP parameters
	auto http_params = http_util.InitializeParameters(*config.db_instance, config.endpoint_url);
	http_params->timeout = config.timeout_seconds;
	http_params->retries = config.max_retries;

	// Set up headers for JSON-RPC
	HTTPHeaders headers(*config.db_instance);
	headers.Insert("Content-Type", "application/json");
	headers.Insert("Accept", "application/json");

	// Add any custom headers
	for (const auto &header : config.custom_headers) {
		headers.Insert(header.first, header.second);
	}

	// Create POST request
	PostRequestInfo request(config.endpoint_url, headers, *http_params,
	                        reinterpret_cast<const_data_ptr_t>(json_data.c_str()), json_data.length());

	// Send request
	auto client = http_util.InitializeClient(*http_params, config.endpoint_url);
	return http_util.SendRequest(request, client);
}

} // namespace duckdb
