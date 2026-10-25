#include "server/http_server_transport.hpp"

// Include httplib from DuckDB's third_party
// Note: This must be included before other headers that might conflict
#include "httplib.hpp"

#include "protocol/mcp_message.hpp"
#include "duckdb/common/string_util.hpp"

#include <iostream>
#include <sstream>

namespace duckdb {

// Constant-time string comparison to prevent timing attacks on auth tokens.
//
// Uses a fixed-size comparison buffer so the iteration count is independent
// of both input lengths.  Earlier versions iterated max(a,b) times which
// still let an attacker probe for the expected token length by sending
// increasingly long inputs and observing the timing transition.
//
// 256 bytes is sufficient for "Bearer " (7 bytes) + any reasonable token.
static bool ConstantTimeEquals(const string &a, const string &b) {
	static constexpr size_t kCompareSize = 256;

	// Strings longer than the buffer cannot be valid tokens;
	// return false without constant-time guarantees since the
	// attacker already knows their own input length.
	if (a.size() > kCompareSize || b.size() > kCompareSize) {
		return false;
	}

	// Copy into fixed-size volatile buffers so the compiler cannot
	// reason about their contents or eliminate the comparison loop.
	// Byte-by-byte copy is required because memcpy cannot target volatile.
	volatile unsigned char buf_a[kCompareSize] = {};
	volatile unsigned char buf_b[kCompareSize] = {};
	for (size_t i = 0; i < a.size(); i++) {
		buf_a[i] = static_cast<unsigned char>(a[i]);
	}
	for (size_t i = 0; i < b.size(); i++) {
		buf_b[i] = static_cast<unsigned char>(b[i]);
	}

	// Length mismatch feeds into the result but does not short-circuit.
	volatile unsigned char result = (a.size() != b.size()) ? 1 : 0;

	// Fixed iteration count -- no timing correlation with either string.
	for (size_t i = 0; i < kCompareSize; i++) {
		result |= buf_a[i] ^ buf_b[i];
	}
	return result == 0;
}

// Helper: determine the CORS origin header value for a given request.
// Returns empty string if CORS is disabled or the origin is not allowed.
static string GetCorsOriginHeader(const HTTPServerConfig &config, const string &request_origin) {
	if (config.cors_origins.empty()) {
		return ""; // CORS disabled
	}
	if (config.cors_origins == "*") {
		return "*"; // Wildcard
	}
	// Check if request origin matches any configured origin
	// Parse comma-separated origin list
	std::istringstream stream(config.cors_origins);
	string origin;
	while (std::getline(stream, origin, ',')) {
		// Trim whitespace
		size_t start = origin.find_first_not_of(" \t");
		size_t end = origin.find_last_not_of(" \t");
		if (start != string::npos && end != string::npos) {
			origin = origin.substr(start, end - start + 1);
		}
		if (origin == request_origin) {
			return request_origin; // Return specific origin (not wildcard)
		}
	}
	return ""; // Origin not allowed
}

// True for an Origin whose host is localhost, 127.0.0.1 or [::1], on any port and scheme.
// Parses scheme://host[:port] exactly, so "http://localhost.evil.com" is NOT loopback.
static bool IsLoopbackOrigin(const string &origin) {
	// Only web schemes: a browser page's Origin is http(s); anything else is not
	// the loopback page this policy is meant to admit
	string rest;
	if (StringUtil::StartsWith(origin, "http://")) {
		rest = origin.substr(7);
	} else if (StringUtil::StartsWith(origin, "https://")) {
		rest = origin.substr(8);
	} else {
		return false;
	}
	string host;
	if (!rest.empty() && rest[0] == '[') {
		auto close = rest.find(']');
		if (close == string::npos) {
			return false;
		}
		host = rest.substr(0, close + 1);
		rest = rest.substr(close + 1);
	} else {
		auto colon = rest.find(':');
		host = rest.substr(0, colon);
		rest = colon == string::npos ? "" : rest.substr(colon);
	}
	// Anything after the host may only be ":<digits>"
	if (!rest.empty()) {
		if (rest[0] != ':' || rest.size() == 1 || rest.find_first_not_of("0123456789", 1) != string::npos) {
			return false;
		}
	}
	return host == "localhost" || host == "127.0.0.1" || host == "[::1]";
}

// Origin validation (MCP 2025-11-25 Streamable HTTP): an Origin header that is present
// and not allowed MUST get 403, on every incoming connection (DNS rebinding defence).
// No Origin header (non-browser clients) always passes.
static bool IsOriginAllowed(const HTTPServerConfig &config, const string &origin) {
	if (origin.empty()) {
		return true;
	}
	if (!GetCorsOriginHeader(config, origin).empty()) {
		return true; // listed in cors_origins, or cors_origins = "*"
	}
	if (config.origin_policy == "allow") {
		return true;
	}
	if (config.origin_policy == "loopback") {
		return IsLoopbackOrigin(origin);
	}
	return false; // "reject"
}

// Helper to set up common routes on a server (works with both Server and SSLServer)
template <typename ServerType>
void SetupRoutes(ServerType &server, const HTTPServerConfig &config,
                 HTTPServerTransport::RequestHandler &request_handler) {
	// Origin check runs before routing and before auth, for every method and path
	server.set_pre_routing_handler(
	    [&config](const CPPHTTPLIB_NAMESPACE::Request &req, CPPHTTPLIB_NAMESPACE::Response &res) {
		    if (IsOriginAllowed(config, req.get_header_value("Origin"))) {
			    return CPPHTTPLIB_NAMESPACE::Server::HandlerResponse::Unhandled;
		    }
		    res.status = 403;
		    res.set_content(R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Forbidden: Origin not allowed"}})",
		                    "application/json");
		    return CPPHTTPLIB_NAMESPACE::Server::HandlerResponse::Handled;
	    });

	// Configure CORS preflight if enabled
	if (!config.cors_origins.empty()) {
		server.Options(".*", [&config](const CPPHTTPLIB_NAMESPACE::Request &req, CPPHTTPLIB_NAMESPACE::Response &res) {
			string origin = req.get_header_value("Origin");
			string cors_value = GetCorsOriginHeader(config, origin);
			if (!cors_value.empty()) {
				res.set_header("Access-Control-Allow-Origin", cors_value);
				res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
				// MCP-Protocol-Version: 2025-06-18+ browser clients MUST send it after
				// initialize, and a browser drops headers the preflight does not list
				res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, MCP-Protocol-Version");
				res.set_header("Access-Control-Max-Age", "86400");
				if (cors_value != "*") {
					res.set_header("Vary", "Origin");
				}
			}
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
			if (!ConstantTimeEquals(auth_header, expected)) {
				// Invalid credentials provided
				res.status = 403;
				res.set_content(
				    R"({"jsonrpc":"2.0","error":{"code":-32003,"message":"Forbidden: invalid credentials"},"id":null})",
				    "application/json");
				return;
			}
		}

		// MCP-Protocol-Version (2025-06-18+): unsupported or invalid MUST get 400.
		// Absent is accepted: 2024-11-05 clients never send it.
		if (req.has_header("MCP-Protocol-Version") &&
		    !MCPProtocolVersions::IsSupported(req.get_header_value("MCP-Protocol-Version"))) {
			res.status = 400;
			res.set_content(
			    R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Unsupported MCP-Protocol-Version"},"id":null})",
			    "application/json");
			return;
		}

		// Set CORS headers on response
		if (!config.cors_origins.empty()) {
			string origin = req.get_header_value("Origin");
			string cors_value = GetCorsOriginHeader(config, origin);
			if (!cors_value.empty()) {
				res.set_header("Access-Control-Allow-Origin", cors_value);
				if (cors_value != "*") {
					res.set_header("Vary", "Origin");
				}
			}
		}

		// Process the MCP request
		try {
			string response = request_handler(req.body);
			if (response.empty()) {
				res.status = 202;
			} else {
				res.set_content(response, "application/json");
			}
		} catch (const std::exception &e) {
			// Log full error internally but return a generic message to the client
			// to avoid leaking internal details (stack traces, file paths, etc.)
			(void)e; // Suppress unused variable warning; in production, log e.what() here
			res.status = 500;
			res.set_content(R"({"jsonrpc":"2.0","error":{"code":-32603,"message":"Internal server error"},"id":null})",
			                "application/json");
		}
	};

	// Main MCP endpoint
	server.Post("/", mcp_handler);

	// Alternative MCP endpoint
	server.Post("/mcp", mcp_handler);

	// Streamable HTTP: GET MUST open an SSE stream or return 405, and 405 is how a
	// server declines DELETE (session termination). We offer neither, so say so;
	// a 404 reads to clients as a broken connection.
	// See: https://modelcontextprotocol.io/specification/2025-03-26/basic/transports
	auto method_not_allowed = [](const CPPHTTPLIB_NAMESPACE::Request &, CPPHTTPLIB_NAMESPACE::Response &res) {
		res.status = 405;
		res.set_header("Allow", "POST");
	};
	server.Get("/", method_not_allowed);
	server.Get("/mcp", method_not_allowed);
	server.Delete("/", method_not_allowed);
	server.Delete("/mcp", method_not_allowed);

	// Health check endpoint (conditionally enabled, optionally auth-protected)
	if (config.enable_health_endpoint) {
		server.Get("/health", [&config](const CPPHTTPLIB_NAMESPACE::Request &req, CPPHTTPLIB_NAMESPACE::Response &res) {
			// Check authentication if required for health endpoint
			if (config.auth_health_endpoint && !config.auth_token.empty()) {
				auto auth_header = req.get_header_value("Authorization");
				if (auth_header.empty()) {
					res.status = 401;
					res.set_header("WWW-Authenticate", "Bearer");
					res.set_content(R"({"error":"Unauthorized"})", "application/json");
					return;
				}
				string expected = "Bearer " + config.auth_token;
				if (!ConstantTimeEquals(auth_header, expected)) {
					res.status = 403;
					res.set_content(R"({"error":"Forbidden"})", "application/json");
					return;
				}
			}
			res.set_content(R"({"status":"ok"})", "application/json");
		});
	}
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

	{
		std::lock_guard<std::mutex> lock(startup_mutex);
		startup_complete = false;
	}

	// Start server in background thread
	server_thread = make_uniq<std::thread>(&HTTPServerTransport::ServerLoop, this);

	// Wait for ServerLoop to signal bind success/failure
	{
		std::unique_lock<std::mutex> lock(startup_mutex);
		startup_cv.wait(lock, [this] { return startup_complete; });
	}

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
	// Helper to signal startup complete (success or failure)
	auto signal_startup = [this]() {
		std::lock_guard<std::mutex> lock(startup_mutex);
		startup_complete = true;
		startup_cv.notify_one();
	};

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
	if (config.use_ssl) {
		// HTTPS server with SSL
		if (config.cert_path.empty() || config.key_path.empty()) {
			running = false;
			signal_startup();
			return;
		}

		CPPHTTPLIB_NAMESPACE::SSLServer server(config.cert_path.c_str(), config.key_path.c_str());

		if (!server.is_valid()) {
			running = false;
			signal_startup();
			return;
		}

		SetupRoutes(server, config, request_handler);

		// Bind and listen
		int port = config.port;
		if (!server.bind_to_port(config.host.c_str(), port)) {
			running = false;
			signal_startup();
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
			signal_startup();
			return;
		}

		signal_startup();
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
		signal_startup();
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
		signal_startup();
		return;
	}

	// Signal that bind succeeded and server is ready
	signal_startup();

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
