#pragma once

#include "protocol/mcp_transport.hpp"
#include "duckdb/common/common.hpp"
#include <mutex>

namespace duckdb {

//! Server-side stdio transport for MCP communication
//! Uses std::cin/std::cout for proper blocking I/O without the poll/iostream mixing issues
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
};

} // namespace duckdb
