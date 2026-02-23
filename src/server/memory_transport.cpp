#include "server/memory_transport.hpp"
#include "duckdb/common/exception.hpp"

namespace duckdb {

bool MemoryTransport::Connect() {
	lock_guard<mutex> lock(queue_mutex);
	connected = true;
	return true;
}

void MemoryTransport::Disconnect() {
	lock_guard<mutex> lock(queue_mutex);
	connected = false;
}

bool MemoryTransport::IsConnected() const {
	lock_guard<mutex> lock(queue_mutex);
	return connected;
}

void MemoryTransport::Send(const MCPMessage &message) {
	lock_guard<mutex> lock(queue_mutex);
	if (!connected) {
		throw IOException("MemoryTransport not connected");
	}
	outgoing_queue.push(message);
}

MCPMessage MemoryTransport::Receive() {
	lock_guard<mutex> lock(queue_mutex);
	if (!connected) {
		throw IOException("MemoryTransport not connected");
	}
	if (incoming_queue.empty()) {
		throw IOException("No messages available in MemoryTransport");
	}
	MCPMessage msg = incoming_queue.front();
	incoming_queue.pop();
	return msg;
}

MCPMessage MemoryTransport::SendAndReceive(const MCPMessage &message) {
	// Not typically used in server mode
	throw NotImplementedException("SendAndReceive not supported in MemoryTransport");
}

bool MemoryTransport::Ping() {
	return IsConnected();
}

string MemoryTransport::GetConnectionInfo() const {
	return "memory transport (testing)";
}

void MemoryTransport::QueueIncomingMessage(const MCPMessage &message) {
	lock_guard<mutex> lock(queue_mutex);
	incoming_queue.push(message);
}

bool MemoryTransport::HasOutgoingMessage() const {
	lock_guard<mutex> lock(queue_mutex);
	return !outgoing_queue.empty();
}

MCPMessage MemoryTransport::PopOutgoingMessage() {
	lock_guard<mutex> lock(queue_mutex);
	if (outgoing_queue.empty()) {
		throw IOException("No outgoing messages in MemoryTransport");
	}
	MCPMessage msg = outgoing_queue.front();
	outgoing_queue.pop();
	return msg;
}

vector<MCPMessage> MemoryTransport::GetAllOutgoingMessages() {
	lock_guard<mutex> lock(queue_mutex);
	vector<MCPMessage> messages;
	while (!outgoing_queue.empty()) {
		messages.push_back(outgoing_queue.front());
		outgoing_queue.pop();
	}
	return messages;
}

void MemoryTransport::Clear() {
	lock_guard<mutex> lock(queue_mutex);
	while (!incoming_queue.empty()) {
		incoming_queue.pop();
	}
	while (!outgoing_queue.empty()) {
		outgoing_queue.pop();
	}
}

} // namespace duckdb
