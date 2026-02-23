#include "server/http_server_transport.hpp"

// Include httplib from DuckDB's third_party
// Note: This must be included before other headers that might conflict
#include "httplib.hpp"

#include <iostream>

namespace duckdb {

// Helper to set up common routes on a server (works with both Server and SSLServer)
template <typename ServerType>
void SetupRoutes(ServerType &server, const HTTPServerConfig &config,
                 HTTPServerTransport::RequestHandler &request_handler) {
	// Configure CORS if enabled
	if (config.enable_cors) {
		server.Options(".*", [](const CPPHTTPLIB_NAMESPACE::Request &req, CPPHTTPLIB_NAMESPACE::Response &res) {
			res.set_header("Access-Control-Allow-Origin", "*");
			res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
			res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
			res.set_header("Access-Control-Max-Age", "86400");
			res.status = 204;
		});
	}

	// Handler for MCP requests
	auto mcp_handler = [&config, &request_handler](const CPPHTTPLIB_NAMESPACE::Request &req,
	                                               CPPHTTPLIB_NAMESPACE::Response &res) {
		// Check authentication if configured
		if (!config.auth_token.empty()) {
			auto auth_header = req.get_header_value("Authorization");
			if (auth_header.empty()) {
				// No credentials provided
				res.status = 401;
				res.set_header("WWW-Authenticate", "Bearer");
				res.set_content(
				    R"({"jsonrpc":"2.0","error":{"code":-32001,"message":"Unauthorized: authentication required"},"id":null})",
				    "application/json");
				return;
			}
			string expected = "Bearer " + config.auth_token;
			if (auth_header != expected) {
				// Invalid credentials provided
				res.status = 403;
				res.set_content(
				    R"({"jsonrpc":"2.0","error":{"code":-32003,"message":"Forbidden: invalid credentials"},"id":null})",
				    "application/json");
				return;
			}
		}

		// Set CORS headers
		if (config.enable_cors) {
			res.set_header("Access-Control-Allow-Origin", "*");
		}

		// Process the MCP request
		try {
			string response = request_handler(req.body);
			res.set_content(response, "application/json");
		} catch (const std::exception &e) {
			// Log full error internally but return a generic message to the client
			// to avoid leaking internal details (stack traces, file paths, etc.)
			(void)e; // Suppress unused variable warning; in production, log e.what() here
			res.status = 500;
			res.set_content(
			    R"({"jsonrpc":"2.0","error":{"code":-32603,"message":"Internal server error"},"id":null})",
			    "application/json");
		}
	};

	// Main MCP endpoint
	server.Post("/", mcp_handler);

	// Alternative MCP endpoint
	server.Post("/mcp", mcp_handler);

	// Health check endpoint
	server.Get("/health", [](const CPPHTTPLIB_NAMESPACE::Request &req, CPPHTTPLIB_NAMESPACE::Response &res) {
		res.set_content(R"({"status":"ok"})", "application/json");
	});
}

HTTPServerTransport::HTTPServerTransport(const HTTPServerConfig &config)
    : config(config), running(false), stop_requested(false), actual_port(0), server_ptr(nullptr) {
}

HTTPServerTransport::~HTTPServerTransport() {
	Stop();
}

bool HTTPServerTransport::Start(RequestHandler handler) {
	if (running.load()) {
		return true; // Already running
	}

	request_handler = std::move(handler);
	stop_requested = false; // Reset stop flag for potential restart
	running = true;

	// Start server in background thread
	server_thread = make_uniq<std::thread>(&HTTPServerTransport::ServerLoop, this);

	// Wait briefly for server to start and get actual port
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	return running.load();
}

bool HTTPServerTransport::Run(RequestHandler handler) {
	if (running.load()) {
		return false; // Already running
	}

	request_handler = std::move(handler);
	stop_requested = false;
	running = true;

	// Run server in calling thread (blocks until Stop() is called)
	ServerLoop();

	return true;
}

void HTTPServerTransport::Stop() {
	stop_requested = true;
	running = false;

	// Stop the httplib server if it's running
	{
		std::lock_guard<std::mutex> lock(server_mutex);
		if (server_ptr) {
			// Cast and stop the server - this will cause listen_after_bind to return
			static_cast<CPPHTTPLIB_NAMESPACE::Server *>(server_ptr)->stop();
		}
	}

	if (server_thread && server_thread->joinable()) {
		server_thread->join();
	}
	server_thread.reset();
}

bool HTTPServerTransport::IsRunning() const {
	return running.load();
}

int HTTPServerTransport::GetPort() const {
	return actual_port.load();
}

string HTTPServerTransport::GetConnectionInfo() const {
	return "HTTP MCP Server at http://" + config.host + ":" + std::to_string(actual_port.load());
}

void HTTPServerTransport::ServerLoop() {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
	if (config.use_ssl) {
		// HTTPS server with SSL
		if (config.cert_path.empty() || config.key_path.empty()) {
			running = false;
			return;
		}

		CPPHTTPLIB_NAMESPACE::SSLServer server(config.cert_path.c_str(), config.key_path.c_str());

		if (!server.is_valid()) {
			running = false;
			return;
		}

		SetupRoutes(server, config, request_handler);

		// Bind and listen
		int port = config.port;
		if (!server.bind_to_port(config.host.c_str(), port)) {
			running = false;
			return;
		}

		actual_port = port;

		// Store server pointer so Stop() can call stop() on it
		// Note: SSLServer inherits from Server, so the base class pointer works for stop()
		{
			std::lock_guard<std::mutex> lock(server_mutex);
			server_ptr = static_cast<CPPHTTPLIB_NAMESPACE::Server *>(&server);
		}

		// Check if stop was requested before we started listening
		if (stop_requested.load()) {
			std::lock_guard<std::mutex> lock(server_mutex);
			server_ptr = nullptr;
			running = false;
			return;
		}

		server.listen_after_bind();

		// Clear pointer after server stops
		{
			std::lock_guard<std::mutex> lock(server_mutex);
			server_ptr = nullptr;
		}
		running = false;
		return;
	}
#endif

	// HTTP server (no SSL)
	CPPHTTPLIB_NAMESPACE::Server server;

	SetupRoutes(server, config, request_handler);

	// Bind and listen
	int port = config.port;
	if (!server.bind_to_port(config.host.c_str(), port)) {
		running = false;
		return;
	}

	actual_port = port;

	// Store server pointer so Stop() can call stop() on it
	{
		std::lock_guard<std::mutex> lock(server_mutex);
		server_ptr = &server;
	}

	// Check if stop was requested before we started listening
	if (stop_requested.load()) {
		std::lock_guard<std::mutex> lock(server_mutex);
		server_ptr = nullptr;
		running = false;
		return;
	}

	// Run the server (this blocks until server.stop() is called)
	server.listen_after_bind();

	// Clear pointer after server stops
	{
		std::lock_guard<std::mutex> lock(server_mutex);
		server_ptr = nullptr;
	}
	running = false;
}

} // namespace duckdb
