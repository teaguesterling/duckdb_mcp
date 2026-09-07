#pragma once

#include "protocol/mcp_transport.hpp"
#include "duckdb/common/common.hpp"
#include <mutex>

namespace duckdb {

#ifndef __EMSCRIPTEN__

//! Exclusive owner of the process's real stdout for the lifetime of a stdio MCP session.
//!
//! JSON-RPC over stdio requires fd 1 to carry framed protocol messages and NOTHING else.
//! A DuckDB process, however, has several writers that reach fd 1 without going through
//! the transport, and any one of them desynchronises the channel for every connected
//! client:
//!
//!   * The DuckDB CLI shell installs its own log storage as the global one
//!     (`RegisterShellLogger` in tools/shell/shell.cpp) at LOG_WARNING level, and
//!     `ShellLogStorage::WriteLogEntry` prints each entry to STDOUT wrapped in ANSI colour
//!     codes. So every `DUCKDB_LOG_WARNING` raised anywhere in DuckDB or in any loaded
//!     extension — including from inside a query the MCP client itself triggered via
//!     `tools/call` — lands mid-stream, and its trailing `\033[00m` reset prefixes the
//!     next response line. See issue #74.
//!   * `SET logging_storage='stdout'` installs DuckDB's own StdOutLogStorage, which
//!     writes log rows via `Printer::RawPrint(OutputStream::STREAM_STDOUT, ...)`.
//!   * The terminal progress bar writes to STREAM_STDOUT.
//!   * Any third-party extension that printf()s.
//!
//! Filtering individual message sources cannot work: the set of writers is open-ended and
//! grows with every extension the user loads. So instead of policing the writers, this
//! class takes the descriptor away from them. `Acquire()` duplicates fd 1 to a private
//! descriptor that only the transport holds, then points fd 1 at fd 2. From that moment
//! every stray write — whoever makes it, through whatever API — goes to stderr, which is
//! the channel a supervisor or container collects anyway, and nothing is silently lost.
//! `Release()` puts fd 1 back the way it was.
class ProtocolStdout {
public:
	ProtocolStdout();
	~ProtocolStdout();

	// Non-copyable: this object owns a descriptor and a process-global side effect.
	ProtocolStdout(const ProtocolStdout &) = delete;
	ProtocolStdout &operator=(const ProtocolStdout &) = delete;

	//! Take the real stdout private and repoint fd 1 at stderr.
	//! Returns false if the descriptor could not be duplicated or redirected, in which case
	//! nothing has changed and writes fall back to the process stdout (the pre-fix behaviour,
	//! which is degraded but still functional).
	bool Acquire();
	//! Restore fd 1 to whatever it referred to before Acquire(). Idempotent.
	void Release();
	bool IsAcquired() const {
		return acquired;
	}
	//! Write raw bytes to the protocol channel. Handles short writes and EINTR.
	//! Returns false if the bytes could not all be written.
	bool Write(const char *data, size_t size);

private:
	//! Private duplicate of the original fd 1, or -1 when not acquired.
	int protocol_fd;
	bool acquired;
};

//! Server-side stdio transport for MCP communication
//! Reads requests from stdin and writes responses to the process's real stdout, which it
//! takes exclusive ownership of via ProtocolStdout for the lifetime of the connection.
class FdServerTransport : public MCPTransport {
public:
	//! Create transport using stdin/stdout
	FdServerTransport();

	~FdServerTransport() override;

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
	bool connected;
	mutable mutex io_mutex;
	//! Guards fd 1 while the session is connected.
	ProtocolStdout protocol_stdout;
};

#endif // !__EMSCRIPTEN__

} // namespace duckdb
