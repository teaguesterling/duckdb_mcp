# MCP Server Testing Strategy

## Current Architecture Challenges

The current MCP server architecture has several testing challenges:

### 1. Foreground Mode Blocks Forever
When `mcp_server_start('stdio')` is called in foreground mode (the default), it:
- Calls `MCPServer::RunMainLoop()` which blocks forever
- Reads from stdin and writes to stdout
- Never returns control to the caller

This makes it impossible to test from within DuckDB SQL tests since the test would hang.

### 2. Background Mode Uses Real stdin/stdout
When background mode is used (`DUCKDB_MCP_BACKGROUND=1`), it:
- Spawns a thread that reads from stdin/stdout
- Still uses the process's actual stdin/stdout file descriptors
- Can't be tested without external process coordination

### 3. Transport is Tightly Coupled
The `FdServerTransport` directly reads from file descriptors, making it hard to:
- Inject test messages
- Capture responses
- Test without actual I/O

## Proposed Testing Architecture

### Option A: In-Memory Transport for Testing

Create a `MemoryTransport` class that:
- Implements the `MCPTransport` interface
- Uses in-memory queues instead of file descriptors
- Allows direct message injection and response capture

```cpp
class MemoryTransport : public MCPTransport {
public:
    // Queue a message to be "received" by the server
    void QueueIncomingMessage(const MCPMessage &message);

    // Get the last message "sent" by the server
    MCPMessage GetLastSentMessage();

    // MCPTransport interface
    void Send(const MCPMessage &message) override;
    MCPMessage Receive() override;
    // ...
};
```

### Option B: Direct Request Handler Testing

Expose `MCPServer::HandleRequest()` for direct testing:
- Skip the transport layer entirely
- Test request → response logic directly
- Fast, no I/O overhead

```cpp
// In test code:
MCPServer server(config);
server.StartForeground();  // Initialize but don't run loop

MCPMessage request = MCPMessage::CreateRequest("tools/list", {});
MCPMessage response = server.HandleRequest(request);
// Assert on response
```

**Problem**: `HandleRequest` is currently private.

### Option C: External Process Testing (Current Approach)

Test via subprocess:
- Launch DuckDB with the MCP extension
- Send JSON-RPC messages via stdin
- Read responses from stdout
- Use timeouts to detect hangs

```python
process = subprocess.Popen(['duckdb', '-init', 'init.sql', '-c',
                           "SELECT mcp_server_start('stdio')"],
                          stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE)
# Send/receive JSON-RPC messages
```

**Problem**: Slow, requires external process coordination, harder to debug.

### Option D: Loopback Transport

Create a transport that connects a server to itself or to a test harness:
- Server writes to one end of a pipe
- Test harness reads from the other end
- Bidirectional communication without actual stdin/stdout

## Recommended Approach

A combination of approaches:

### Unit Tests (Option B - Direct Handler Testing)
- Make `HandleRequest()` public or add a `TestableHandleRequest()` method
- Test individual protocol handlers directly
- Fast, isolated, easy to debug

```cpp
// mcp_server.hpp
class MCPServer {
public:
    // For testing - handle a single request without transport
    MCPMessage ProcessRequest(const MCPMessage &request);
};
```

### Integration Tests (Option A - Memory Transport)
- Test full request/response flow with transport abstraction
- Verify message serialization/deserialization
- Test connection lifecycle

```cpp
// Create server with memory transport
auto transport = make_shared<MemoryTransport>();
MCPServer server(config);
server.SetTransport(transport);
server.StartForeground();

// Inject test message
transport->QueueIncomingMessage(init_request);
server.ProcessOneMessage();  // New method: process exactly one message

// Check response
auto response = transport->GetLastSentMessage();
ASSERT_EQ(response.id, init_request.id);
```

### End-to-End Tests (Option C - External Process)
- Test the actual stdio transport
- Verify real-world usage scenarios
- Run less frequently (slower)

## Implementation Steps

1. **Add `ProcessRequest()` public method** to MCPServer
   - Wrapper around `HandleRequest()` for testing

2. **Add `ProcessOneMessage()` method** to MCPServer
   - Receive one message, handle it, send response
   - Returns after one message (doesn't loop)

3. **Create `MemoryTransport` class**
   - In-memory message queues
   - Thread-safe for concurrent testing

4. **Add transport injection to MCPServer**
   - Allow setting transport after construction
   - Or pass transport to `RunMainLoop()`

5. **Create SQL-based tests** using background mode + pipes
   - Use named pipes or Unix domain sockets
   - DuckDB test harness can write to input pipe

## Test Categories

### Protocol Tests
- Initialize handshake
- tools/list response format
- tools/call argument validation
- resources/list and resources/read
- Error responses (invalid method, missing params)

### Tool Tests
- query tool execution
- describe tool output
- list_tables tool
- database_info tool
- execute tool (DDL/DML)

### Security Tests
- Authentication validation
- Query allowlist/denylist
- mcp_disable_serving flag

### Transport Tests
- Connection lifecycle
- Message framing (newline-delimited JSON)
- Large message handling
- Timeout behavior

## Example Test File Structure

```
test/
├── sql/
│   └── mcp_server_functionality.test  # Existing basic tests
├── cpp/
│   ├── test_mcp_protocol.cpp          # Direct handler tests
│   ├── test_mcp_transport.cpp         # Transport layer tests
│   └── test_mcp_tools.cpp             # Tool handler tests
└── integration/
    ├── test_mcp_e2e.py                # End-to-end subprocess tests
    └── test_mcp_client.py             # Client library tests
```
