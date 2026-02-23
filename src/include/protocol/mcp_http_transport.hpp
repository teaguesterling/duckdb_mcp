#pragma once

#include "protocol/mcp_transport.hpp"
#include "duckdb/common/common.hpp"
#include "duckdb/common/unordered_map.hpp"

namespace duckdb {

#ifndef __EMSCRIPTEN__

class DatabaseInstance;
class HTTPResponse;

//! Configuration for HTTP MCP transport
struct HTTPConfig {
	string endpoint_url;
	uint32_t timeout_seconds = 30;
	uint32_t max_retries = 3;
	unordered_map<string, string> custom_headers;
	DatabaseInstance *db_instance;

	HTTPConfig(const string &url, DatabaseInstance *db) : endpoint_url(url), db_instance(db) {
	}
};

//! HTTP transport implementation for MCP communication
class HTTPTransport : public MCPTransport {
private:
	HTTPConfig config;
	bool is_connected;
	string last_error;

public:
	HTTPTransport(const HTTPConfig &config);
	~HTTPTransport() override;

	// MCPTransport interface
	bool Connect() override;
	void Disconnect() override;
	bool IsConnected() const override;
	void Send(const MCPMessage &message) override;
	MCPMessage Receive() override;
	MCPMessage SendAndReceive(const MCPMessage &message) override;
	bool Ping() override;
	string GetConnectionInfo() const override;

private:
	//! Send HTTP request and return response
	unique_ptr<HTTPResponse> SendHTTPRequest(const string &json_data);
};

#endif // !__EMSCRIPTEN__

} // namespace duckdb
