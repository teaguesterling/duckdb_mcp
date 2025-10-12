#pragma once

#include "protocol/mcp_transport.hpp"
#include "duckdb/common/common.hpp"
#include <iostream>
#include <sstream>
#ifndef _WIN32
#include <unistd.h>
#else
// Windows doesn't have these constants, so define them
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#endif

namespace duckdb {

//! Server-side file descriptor transport for MCP communication
//! Handles JSON-RPC messages via configurable file descriptors (stdin/stdout, sockets, pipes, etc.)
class FdServerTransport : public MCPTransport {
public:
    // Default to stdin/stdout
    FdServerTransport(int input_fd = STDIN_FILENO, int output_fd = STDOUT_FILENO);
    
    // Use file paths (e.g., "/dev/stdin", "/dev/stdout", socket paths, named pipes)
    FdServerTransport(const string &input_path, const string &output_path);
    
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
    int input_fd;
    int output_fd;
    bool owns_fds;  // Whether we should close the FDs on destruction
    bool connected;
    mutable mutex io_mutex;
    
    // I/O helpers
    string ReadLine();
    void WriteLine(const string &line);
    bool HasInputAvailable();
    
    // FD management
    bool OpenFileDescriptors(const string &input_path, const string &output_path);
};

} // namespace duckdb