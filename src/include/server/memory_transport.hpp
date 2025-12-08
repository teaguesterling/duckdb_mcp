#pragma once

#include "protocol/mcp_transport.hpp"
#include "protocol/mcp_message.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>

namespace duckdb {

//! In-memory transport for testing MCP server without actual I/O
//! Messages are exchanged via thread-safe queues
class MemoryTransport : public MCPTransport {
public:
    MemoryTransport() : connected(false) {}
    ~MemoryTransport() override = default;

    // MCPTransport interface
    bool Connect() override;
    void Disconnect() override;
    bool IsConnected() const override;
    void Send(const MCPMessage &message) override;
    MCPMessage Receive() override;
    MCPMessage SendAndReceive(const MCPMessage &message) override;
    bool Ping() override;
    string GetConnectionInfo() const override;

    // Testing interface - inject messages for the server to receive
    void QueueIncomingMessage(const MCPMessage &message);

    // Testing interface - get messages sent by the server
    bool HasOutgoingMessage() const;
    MCPMessage PopOutgoingMessage();

    // Testing interface - get all outgoing messages
    vector<MCPMessage> GetAllOutgoingMessages();

    // Testing interface - clear all queues
    void Clear();

private:
    bool connected;
    mutable mutex queue_mutex;
    std::queue<MCPMessage> incoming_queue;  // Messages for server to receive
    std::queue<MCPMessage> outgoing_queue;  // Messages sent by server
};

} // namespace duckdb
