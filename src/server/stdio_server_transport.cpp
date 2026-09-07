#include "server/stdio_server_transport.hpp"
#include "protocol/mcp_message.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb_mcp_logging.hpp"
#include <cerrno>
#include <cstdio>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#define MCP_DUP       _dup
#define MCP_DUP2      _dup2
#define MCP_CLOSE     _close
#define MCP_STDOUT_FD _fileno(stdout)
#define MCP_STDERR_FD _fileno(stderr)
#else
#include <unistd.h>
#define MCP_DUP       dup
#define MCP_DUP2      dup2
#define MCP_CLOSE     close
#define MCP_STDOUT_FD STDOUT_FILENO
#define MCP_STDERR_FD STDERR_FILENO
#endif

namespace duckdb {

//===--------------------------------------------------------------------===//
// ProtocolStdout
//===--------------------------------------------------------------------===//

static int64_t WriteFd(int fd, const char *data, size_t size) {
#ifdef _WIN32
	return static_cast<int64_t>(_write(fd, data, static_cast<unsigned int>(size)));
#else
	return static_cast<int64_t>(::write(fd, data, size));
#endif
}

ProtocolStdout::ProtocolStdout() : protocol_fd(-1), acquired(false) {
}

ProtocolStdout::~ProtocolStdout() {
	Release();
}

bool ProtocolStdout::Acquire() {
	if (acquired) {
		return true;
	}

	// Anything the host process has already buffered for the real stdout must be drained
	// BEFORE the descriptor moves — otherwise it would be flushed to stderr later and
	// appear to have vanished. The CLI shell in particular buffers its own output.
	std::cout.flush();
	fflush(stdout);

	int dup_fd = MCP_DUP(MCP_STDOUT_FD);
	if (dup_fd < 0) {
		// Could not duplicate stdout; leave the process alone and fall back to writing
		// straight to fd 1 (the pre-fix behaviour).
		return false;
	}
	if (MCP_DUP2(MCP_STDERR_FD, MCP_STDOUT_FD) < 0) {
		MCP_CLOSE(dup_fd);
		return false;
	}

	protocol_fd = dup_fd;
	acquired = true;
	return true;
}

void ProtocolStdout::Release() {
	if (!acquired) {
		return;
	}
	acquired = false;

	// Drain whatever was written to the stderr-backed fd 1 while we held it, so it is
	// delivered to stderr rather than to the restored stdout.
	std::cout.flush();
	fflush(stdout);

	MCP_DUP2(protocol_fd, MCP_STDOUT_FD);
	MCP_CLOSE(protocol_fd);
	protocol_fd = -1;
}

bool ProtocolStdout::Write(const char *data, size_t size) {
	if (!acquired) {
		// Degraded path: we never got the descriptor, so fd 1 is still shared with
		// whatever else the process writes through stdio buffers. Drain those first so
		// this frame is not interleaved into the middle of a buffered line.
		std::cout.flush();
		fflush(stdout);
	}
	const int fd = acquired ? protocol_fd : MCP_STDOUT_FD;
	size_t written = 0;
	while (written < size) {
		auto n = WriteFd(fd, data + written, size - written);
		if (n < 0) {
#ifndef _WIN32
			if (errno == EINTR) {
				// Interrupted before any byte was transferred - retry.
				continue;
			}
#endif
			return false;
		}
		if (n == 0) {
			return false;
		}
		written += static_cast<size_t>(n);
	}
	return true;
}

//===--------------------------------------------------------------------===//
// FdServerTransport
//===--------------------------------------------------------------------===//

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

	// Take exclusive ownership of the real stdout for the duration of the session, so that
	// only framed JSON-RPC reaches the peer (issue #74). A failure here is not fatal: the
	// transport still works, it is just no longer protected from stray writes.
	if (!protocol_stdout.Acquire()) {
		MCP_LOG_WARN("stdio", "Could not take exclusive ownership of stdout; a stray write by "
		                      "DuckDB or another extension may corrupt the JSON-RPC stream");
	}

	connected = true;
	return true;
}

void FdServerTransport::Disconnect() {
	lock_guard<mutex> lock(io_mutex);
	connected = false;
	protocol_stdout.Release();
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
		// Write the framed message to the protocol channel in a single call. Note this
		// deliberately does NOT go through std::cout: while the session is connected,
		// fd 1 points at stderr and std::cout is where the stray writes we are protecting
		// against end up.
		json += "\n";
		if (!protocol_stdout.Write(json.c_str(), json.size())) {
			throw IOException("Failed to write message to stdout");
		}
		MCP_LOG_DEBUG("stdio", "Response sent and flushed");

	} catch (const IOException &) {
		throw;
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
