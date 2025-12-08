#include "server/stdio_server_transport.hpp"
#include "protocol/mcp_message.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb_mcp_logging.hpp"
#include <iostream>

namespace duckdb {

FdServerTransport::FdServerTransport() : connected(false) {
}

FdServerTransport::~FdServerTransport() {
    Disconnect();
}

bool FdServerTransport::Connect() {
    lock_guard<mutex> lock(io_mutex);

    if (connected) {
        return true;
    }

    connected = true;
    return true;
}

void FdServerTransport::Disconnect() {
    lock_guard<mutex> lock(io_mutex);
    connected = false;
}

bool FdServerTransport::IsConnected() const {
    return connected;
}

void FdServerTransport::Send(const MCPMessage &message) {
    if (!IsConnected()) {
        MCP_LOG_ERROR("stdio", "Send called but not connected");
        throw IOException("Not connected");
    }

    lock_guard<mutex> lock(io_mutex);

    try {
        string json = message.ToJSON();
        MCP_LOG_DEBUG("stdio", "Sending response: %s", json.substr(0, 100).c_str());
        // Write directly to stdout and flush immediately
        std::cout << json << std::endl;
        std::cout.flush();
        MCP_LOG_DEBUG("stdio", "Response sent and flushed");

    } catch (const std::exception &e) {
        MCP_LOG_ERROR("stdio", "Failed to send message: %s", e.what());
        throw IOException("Failed to send message: " + string(e.what()));
    }
}

MCPMessage FdServerTransport::Receive() {
    if (!IsConnected()) {
        MCP_LOG_ERROR("stdio", "Receive called but not connected");
        throw IOException("Not connected");
    }

    lock_guard<mutex> lock(io_mutex);

    try {
        MCP_LOG_DEBUG("stdio", "Waiting for input on stdin...");
        // Use std::getline which blocks until a line is available
        // This properly handles iostream buffering without mixing with poll()
        string line;
        if (!std::getline(std::cin, line)) {
            MCP_LOG_DEBUG("stdio", "EOF or error on stdin");
            throw IOException("End of input stream");
        }

        MCP_LOG_DEBUG("stdio", "Received line (%zu chars): %s", line.length(), line.substr(0, 100).c_str());

        if (line.empty()) {
            MCP_LOG_WARN("stdio", "Received empty line");
            throw IOException("Received empty message");
        }

        return MCPMessage::FromJSON(line);

    } catch (const IOException &) {
        throw;
    } catch (const std::exception &e) {
        MCP_LOG_ERROR("stdio", "Failed to receive/parse message: %s", e.what());
        throw IOException("Failed to receive message: " + string(e.what()));
    }
}

MCPMessage FdServerTransport::SendAndReceive(const MCPMessage &message) {
    // For server mode, this doesn't make sense - we send responses, not requests
    throw NotImplementedException("SendAndReceive not supported in server mode");
}

bool FdServerTransport::Ping() {
    return IsConnected();
}

string FdServerTransport::GetConnectionInfo() const {
    return "stdio server transport (stdin/stdout)";
}

} // namespace duckdb
