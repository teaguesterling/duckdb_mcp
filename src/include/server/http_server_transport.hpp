#pragma once

#include "duckdb.hpp"
#include <atomic>
#include <thread>
#include <functional>
#include <mutex>

namespace duckdb {

// Forward declaration
class MCPServer;

//! Configuration for the HTTP server transport
struct HTTPServerConfig {
	string host = "localhost";
	int port = 8080;
	string auth_token; // Optional: Bearer token for authentication
	bool enable_cors = true;
	int request_timeout_ms = 30000;

	// HTTPS/SSL configuration
	bool use_ssl = false;
	string cert_path; // Path to SSL certificate file
	string key_path;  // Path to SSL private key file
};

//! HTTP Server Transport for MCP
//! Provides an HTTP endpoint for MCP JSON-RPC requests
class HTTPServerTransport {
public:
	using RequestHandler = std::function<string(const string &)>;

	HTTPServerTransport(const HTTPServerConfig &config);
	~HTTPServerTransport();

	//! Start the HTTP server (non-blocking, runs in background thread)
	bool Start(RequestHandler handler);

	//! Run the HTTP server (blocking, runs in calling thread until Stop() is called)
	bool Run(RequestHandler handler);

	//! Stop the HTTP server
	void Stop();

	//! Check if the server is running
	bool IsRunning() const;

	//! Get the actual port (useful when port 0 was specified for auto-assign)
	int GetPort() const;

	//! Get connection info string
	string GetConnectionInfo() const;

private:
	HTTPServerConfig config;
	std::atomic<bool> running;
	std::atomic<bool> stop_requested;
	std::atomic<int> actual_port;
	unique_ptr<std::thread> server_thread;
	RequestHandler request_handler;

	//! Opaque pointer to httplib server (can't include httplib.hpp in header)
	void *server_ptr;
	std::mutex server_mutex;

	//! Internal server loop
	void ServerLoop();
};

} // namespace duckdb
