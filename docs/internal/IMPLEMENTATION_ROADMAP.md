# MCP + DuckDB Implementation Roadmap

## Project Overview

This roadmap outlines the implementation plan for MCP (Model Context Protocol) integration with DuckDB through one or more extensions. The project can be implemented as separate client/server extensions or as a unified extension with both capabilities.

## Architecture Decision: Unified vs. Separate Extensions

### Option A: Unified Extension (`duckdb_mcp`)
**Pros:**
- Single extension to install and manage
- Shared infrastructure and dependencies
- Consistent configuration and API
- Easier dependency management

**Cons:**
- Larger extension size
- More complex codebase
- Potential for feature coupling

### Option B: Separate Extensions (`mcp_client` + `mcp_server`)
**Pros:**
- Smaller, focused extensions
- Independent deployment
- Clear separation of concerns
- Optional server functionality

**Cons:**
- Duplicate infrastructure code
- Complex inter-extension dependencies
- More maintenance overhead

### Recommendation: Unified Extension with Feature Flags

Implement as a single `duckdb_mcp` extension with configurable client/server capabilities:

```sql
LOAD mcp;
-- Both client and server capabilities available

-- Enable only client features
SET mcp.enable_server = false;

-- Enable only server features  
SET mcp.enable_client = false;
```

## Implementation Phases

### Phase 1: Foundation and MCPFS (Weeks 1-4)

#### Goals
- Implement basic MCPFS file system
- Basic ATTACH support for stdio transport
- Integration with DuckDB file functions
- Core MCP protocol handling

#### Deliverables

**1.1 Project Structure**
```
duckdb_mcp/
├── CMakeLists.txt
├── Makefile
├── extension_config.cmake
├── vcpkg.json
├── README.md
├── src/
│   ├── mcp_extension.cpp
│   ├── mcpfs/
│   │   ├── mcp_file_system.cpp
│   │   ├── mcp_file_handle.cpp
│   │   └── path_parser.cpp
│   ├── protocol/
│   │   ├── mcp_protocol.cpp
│   │   ├── mcp_message.cpp
│   │   └── mcp_transport.cpp
│   ├── client/
│   │   ├── mcp_client.cpp
│   │   └── mcp_attachment.cpp
│   └── include/
│       ├── mcp_extension.hpp
│       ├── mcpfs/
│       ├── protocol/
│       └── client/
├── test/
│   ├── sql/
│   │   ├── mcp_basic.test
│   │   ├── mcpfs.test
│   │   └── attach.test
│   └── mcp_servers/
│       ├── test_stdio_server.py
│       └── test_resources/
└── docs/
    ├── EXAMPLES.md
    └── API_REFERENCE.md
```

**1.2 Core Components**

```cpp
// mcp_extension.cpp - Main extension entry point
class MCPExtension : public Extension {
public:
    void Load(DuckDB &db) override;
    std::string Name() override { return "mcp"; }
    std::string Version() const override;
};

// Register MCPFS and attachment handlers
void MCPExtension::Load(DuckDB &db) {
    // Register MCP file system
    auto &fs = db.GetFileSystem();
    fs.RegisterSubSystem("mcp", make_unique<MCPFileSystem>());
    
    // Register attachment handler for MCP servers
    auto &config = DBConfig::GetConfig(*db.instance);
    config.attachment_handlers["mcp"] = make_unique<MCPAttachmentHandler>();
    
    // Register core functions
    RegisterMCPClientFunctions(*db.instance);
}
```

**1.3 MCPFS Implementation**

```cpp
// mcpfs/mcp_file_system.cpp
class MCPFileSystem : public FileSystem {
private:
    unordered_map<string, unique_ptr<MCPConnection>> connections;
    
public:
    unique_ptr<FileHandle> OpenFile(const string &path, uint8_t flags) override;
    bool FileExists(const string &filename) override;
    vector<string> Glob(const string &path) override;
    
private:
    MCPPath ParsePath(const string &path);
    MCPConnection* GetConnection(const string &server_name);
};
```

**1.4 Basic Protocol Support**

```cpp
// protocol/mcp_transport.cpp
class MCPTransport {
public:
    virtual ~MCPTransport() = default;
    virtual void Send(const MCPMessage &message) = 0;
    virtual MCPMessage Receive() = 0;
    virtual bool IsConnected() = 0;
};

class StdioTransport : public MCPTransport {
private:
    unique_ptr<Process> process;
    
public:
    StdioTransport(const string &command, const vector<string> &args);
    void Send(const MCPMessage &message) override;
    MCPMessage Receive() override;
};
```

**1.5 Test Infrastructure**

```python
# test/mcp_servers/test_stdio_server.py
import json
import sys
from typing import Dict, List

class TestMCPServer:
    def __init__(self):
        self.resources = {
            "file:///test.csv": "id,name,value\n1,test,100\n2,demo,200",
            "resource://data/users": '{"users": [{"id": 1, "name": "John"}]}',
            "system://status": '{"status": "ok", "version": "1.0"}'
        }
    
    def handle_request(self, request: Dict) -> Dict:
        method = request.get("method")
        if method == "resources/list":
            return self.list_resources()
        elif method == "resources/read":
            return self.read_resource(request["params"]["uri"])
        # ... other methods
```

#### Success Criteria - Phase 1
- [ ] ATTACH stdio MCP servers successfully
- [ ] Basic file reading through MCPFS (`read_csv('mcp://server/file:///test.csv')`)
- [ ] Path parsing and validation working
- [ ] Error handling for connection failures
- [ ] Basic test suite passing

### Phase 2: Full Client Implementation (Weeks 5-8)

#### Goals
- Complete MCP client protocol implementation
- Tool execution capabilities  
- Advanced transport support (TCP, WebSocket)
- Resource management and caching

#### Deliverables

**2.1 Complete Protocol Implementation**

```cpp
// protocol/mcp_protocol.cpp
class MCPProtocol {
public:
    // Initialize protocol
    InitializeResult Initialize(const MCPClientCapabilities &capabilities);
    
    // Resource operations
    ListResourcesResult ListResources(const optional<string> &cursor = {});
    ReadResourceResult ReadResource(const string &uri);
    SubscribeResult Subscribe(const string &uri);
    
    // Tool operations
    ListToolsResult ListTools(const optional<string> &cursor = {});
    CallToolResult CallTool(const string &name, const Value &arguments);
    
    // Prompt operations
    ListPromptsResult ListPrompts(const optional<string> &cursor = {});
    GetPromptResult GetPrompt(const string &name, const Value &arguments);
};
```

**2.2 Advanced Transport Support**

```cpp
// protocol/tcp_transport.cpp
class TCPTransport : public MCPTransport {
private:
    unique_ptr<TCPSocket> socket;
    string host;
    int port;
    
public:
    TCPTransport(const string &host, int port, const TCPOptions &options);
    bool Connect();
    void Send(const MCPMessage &message) override;
    MCPMessage Receive() override;
};

// protocol/websocket_transport.cpp  
class WebSocketTransport : public MCPTransport {
private:
    unique_ptr<WebSocketClient> client;
    string url;
    
public:
    WebSocketTransport(const string &url, const WSOptions &options);
    bool Connect();
    void Send(const MCPMessage &message) override;
    MCPMessage Receive() override;
};
```

**2.3 Client Function Registration**

```cpp
// client/mcp_client_functions.cpp
void RegisterMCPClientFunctions(DatabaseInstance &db) {
    // Connection management
    CreateScalarFunction("mcp_list_servers", MCPListServersFunction);
    CreateScalarFunction("mcp_server_info", MCPServerInfoFunction);
    CreateScalarFunction("mcp_ping", MCPPingFunction);
    
    // Tool execution
    CreateScalarFunction("mcp_call_tool", MCPCallToolFunction);
    CreateTableFunction("mcp_call_tool_streaming", MCPCallToolStreamingFunction);
    CreateScalarFunction("mcp_list_tools", MCPListToolsFunction);
    
    // Resource management
    CreateTableFunction("mcp_list_resources", MCPListResourcesFunction);
    CreateScalarFunction("mcp_resource_exists", MCPResourceExistsFunction);
    CreateScalarFunction("mcp_read_resource", MCPReadResourceFunction);
    
    // Prompt management
    CreateTableFunction("mcp_list_prompts", MCPListPromptsFunction);
    CreateScalarFunction("mcp_get_prompt", MCPGetPromptFunction);
}
```

**2.4 Resource Caching System**

```cpp
// client/resource_cache.cpp
class ResourceCache {
private:
    struct CacheEntry {
        vector<char> data;
        time_t expires_at;
        string etag;
        size_t size;
    };
    
    unordered_map<string, CacheEntry> cache;
    size_t max_size;
    mutex cache_mutex;
    
public:
    optional<vector<char>> Get(const string &key);
    void Put(const string &key, const vector<char> &data, time_t ttl);
    void Evict(const string &key);
    void Clear();
    size_t GetCurrentSize();
};
```

#### Success Criteria - Phase 2
- [ ] All transport types working (stdio, TCP, WebSocket)
- [ ] Tool execution with complex arguments
- [ ] Resource listing and pattern matching
- [ ] Caching system reducing redundant requests
- [ ] Authentication working for secure servers
- [ ] Comprehensive error handling and recovery

### Phase 3: Server Implementation (Weeks 9-12)

#### Goals
- Implement MCP server capabilities
- Resource publishing from DuckDB tables/queries
- Built-in tool registration and execution
- Server management functions

#### Deliverables

**3.1 Server Core Infrastructure**

```cpp
// server/mcp_server.cpp
class MCPServer {
private:
    unique_ptr<MCPTransport> transport;
    ResourceRegistry resource_registry;
    ToolRegistry tool_registry;
    PromptRegistry prompt_registry;
    bool running;
    
public:
    bool Start(const ServerConfig &config);
    void Stop();
    bool IsRunning() const { return running; }
    
    // Resource management
    bool PublishResource(const string &uri, unique_ptr<ResourceProvider> provider);
    bool UnpublishResource(const string &uri);
    
    // Tool management
    bool RegisterTool(const string &name, unique_ptr<ToolHandler> handler);
    bool UnregisterTool(const string &name);
    
private:
    void HandleRequest(const MCPMessage &request);
    MCPMessage HandleResourcesList(const Value &params);
    MCPMessage HandleResourcesRead(const Value &params);
    MCPMessage HandleToolsList(const Value &params);
    MCPMessage HandleToolsCall(const Value &params);
};
```

**3.2 Resource Publishing**

```cpp
// server/resource_providers.cpp
class TableResourceProvider : public ResourceProvider {
private:
    string table_name;
    vector<string> formats;
    DatabaseInstance &db;
    
public:
    TableResourceProvider(const string &table, const vector<string> &formats, DatabaseInstance &db);
    ReadResourceResult Read(const string &format = "arrow") override;
    optional<size_t> GetSize() override;
    string GetMimeType(const string &format) override;
};

class QueryResourceProvider : public ResourceProvider {
private:
    string query;
    vector<string> formats;
    time_t refresh_interval;
    time_t last_refresh;
    DatabaseInstance &db;
    
public:
    QueryResourceProvider(const string &query, const vector<string> &formats, 
                         time_t refresh_interval, DatabaseInstance &db);
    ReadResourceResult Read(const string &format = "arrow") override;
    bool ShouldRefresh() const;
};
```

**3.3 Built-in Tool Handlers**

```cpp
// server/builtin_tools.cpp
class QueryToolHandler : public ToolHandler {
private:
    DatabaseInstance &db;
    SecurityConfig security;
    
public:
    CallToolResult Execute(const Value &arguments) override {
        auto sql = arguments["sql"].GetString();
        
        // Validate query against security policy
        if (!security.IsQueryAllowed(sql)) {
            return CallToolResult::Error("Query not allowed");
        }
        
        // Execute query
        auto result = db.Query(sql);
        auto format = arguments.GetMember("format").GetStringOrDefault("json");
        
        return CallToolResult::Success(FormatResult(result, format));
    }
};

class DescribeToolHandler : public ToolHandler {
public:
    CallToolResult Execute(const Value &arguments) override {
        auto table_name = arguments["table"].GetString();
        auto query = "DESCRIBE " + table_name;
        auto result = db.Query(query);
        return CallToolResult::Success(FormatResult(result, "json"));
    }
};
```

**3.4 Server Management Functions**

```cpp
// server/mcp_server_functions.cpp
void RegisterMCPServerFunctions(DatabaseInstance &db) {
    // Server lifecycle
    CreateScalarFunction("mcp_server_start", MCPServerStartFunction);
    CreateScalarFunction("mcp_server_stop", MCPServerStopFunction);
    CreateScalarFunction("mcp_server_status", MCPServerStatusFunction);
    
    // Resource publishing
    CreateScalarFunction("mcp_publish_table", MCPPublishTableFunction);
    CreateScalarFunction("mcp_publish_query", MCPPublishQueryFunction);
    CreateScalarFunction("mcp_publish_directory", MCPPublishDirectoryFunction);
    CreateScalarFunction("mcp_unpublish_resource", MCPUnpublishResourceFunction);
    CreateTableFunction("mcp_list_published_resources", MCPListPublishedResourcesFunction);
    
    // Tool registration
    CreateScalarFunction("mcp_register_sql_tool", MCPRegisterSQLToolFunction);
    CreateScalarFunction("mcp_register_function_tool", MCPRegisterFunctionToolFunction);
    CreateScalarFunction("mcp_unregister_tool", MCPUnregisterToolFunction);
    CreateTableFunction("mcp_list_registered_tools", MCPListRegisteredToolsFunction);
}
```

#### Success Criteria - Phase 3
- [ ] Server starts and accepts connections
- [ ] Tables published as resources in multiple formats
- [ ] Built-in tools (query, describe, export) working
- [ ] Custom SQL tools can be registered
- [ ] Authentication and authorization working
- [ ] Server can handle concurrent connections

### Phase 4: Advanced Features and Production Readiness (Weeks 13-16)

#### Goals
- Performance optimization and streaming
- Advanced security features
- Comprehensive monitoring and logging
- Production deployment features

#### Deliverables

**4.1 Streaming and Performance**

```cpp
// streaming/resource_stream.cpp
class ResourceStream {
private:
    unique_ptr<MCPTransport> transport;
    string resource_uri;
    size_t chunk_size;
    size_t current_position;
    
public:
    ResourceStream(unique_ptr<MCPTransport> transport, const string &uri, size_t chunk_size = 64 * 1024);
    
    size_t Read(void *buffer, size_t size);
    bool Seek(size_t position);
    size_t GetPosition() const { return current_position; }
    bool IsEOF() const;
};

// Connection pooling
class ConnectionPool {
private:
    queue<unique_ptr<MCPConnection>> available_connections;
    set<unique_ptr<MCPConnection>> in_use_connections;
    size_t max_connections;
    mutex pool_mutex;
    
public:
    unique_ptr<MCPConnection> Acquire();
    void Release(unique_ptr<MCPConnection> connection);
    void ResizePool(size_t new_size);
};
```

**4.2 Security Framework**

```cpp
// security/security_manager.cpp
class SecurityManager {
private:
    AccessControlList tool_acl;
    AccessControlList resource_acl;
    RateLimiter rate_limiter;
    AuditLogger audit_logger;
    
public:
    bool IsToolCallAllowed(const string &tool_name, const Value &arguments, 
                          const ConnectionContext &context);
    bool IsResourceAccessAllowed(const string &resource_uri, 
                                const ConnectionContext &context);
    void LogSecurityEvent(const SecurityEvent &event);
    void EnforceRateLimit(const ConnectionContext &context);
};

// Audit logging
class AuditLogger {
public:
    void LogToolCall(const string &tool, const Value &args, const ConnectionContext &ctx);
    void LogResourceAccess(const string &uri, const ConnectionContext &ctx);
    void LogSecurityViolation(const string &violation, const ConnectionContext &ctx);
    void LogError(const string &error, const ConnectionContext &ctx);
};
```

**4.3 Monitoring and Metrics**

```cpp
// monitoring/metrics_collector.cpp
class MetricsCollector {
private:
    atomic<uint64_t> total_requests;
    atomic<uint64_t> successful_requests;
    atomic<uint64_t> failed_requests;
    atomic<uint64_t> bytes_transferred;
    map<string, atomic<uint64_t>> tool_call_counts;
    map<string, atomic<uint64_t>> resource_access_counts;
    
public:
    void RecordRequest(bool success, size_t bytes);
    void RecordToolCall(const string &tool_name);
    void RecordResourceAccess(const string &resource_uri);
    
    MetricsSummary GetSummary() const;
    void ResetMetrics();
};

// Health checks
class HealthChecker {
public:
    HealthStatus CheckOverallHealth();
    HealthStatus CheckConnectionHealth(const string &server_name);
    HealthStatus CheckResourceHealth(const string &resource_uri);
    HealthStatus CheckToolHealth(const string &tool_name);
};
```

**4.4 Configuration Management**

```cpp
// config/mcp_config.cpp
class MCPConfig {
public:
    struct ClientConfig {
        map<string, AttachmentConfig> server_configs;
        CacheConfig cache_config;
        SecurityConfig security_config;
        PerformanceConfig performance_config;
    };
    
    struct ServerConfig {
        TransportConfig transport_config;
        AuthenticationConfig auth_config;
        ResourceConfig resource_config;
        ToolConfig tool_config;
        MonitoringConfig monitoring_config;
    };
    
    static MCPConfig LoadFromFile(const string &config_file);
    static MCPConfig LoadFromEnvironment();
    void Validate() const;
};
```

#### Success Criteria - Phase 4
- [ ] Handles large file transfers efficiently
- [ ] Connection pooling reduces latency
- [ ] Comprehensive security controls working
- [ ] Audit logging captures all events
- [ ] Metrics collection and health checks
- [ ] Configuration system supports complex deployments
- [ ] Documentation complete for production use

## Testing Strategy

### Unit Tests
- Protocol message parsing/serialization
- Path parsing and validation  
- Resource caching logic
- Security policy enforcement
- Error handling edge cases

### Integration Tests
- Full client-server communication
- Multi-format resource publishing
- Tool execution with complex arguments
- Authentication flows
- Performance under load

### End-to-End Tests
- Real MCP server integration
- Cross-platform compatibility
- Large file handling
- Concurrent connection handling
- Failure recovery scenarios

## Dependencies and Build System

### Core Dependencies
```json
{
  "name": "duckdb-mcp",
  "version": "0.1.0", 
  "dependencies": [
    {
      "name": "nlohmann-json",
      "version>=": "3.11.0"
    },
    {
      "name": "websocketpp", 
      "version>=": "0.8.2"
    },
    {
      "name": "openssl",
      "version>=": "3.0.0"
    }
  ]
}
```

### Build Configuration
```cmake
# CMakeLists.txt
set(MCP_EXTENSION_FILES
    src/mcp_extension.cpp
    src/mcpfs/mcp_file_system.cpp
    src/mcpfs/mcp_file_handle.cpp
    src/mcpfs/path_parser.cpp
    src/protocol/mcp_protocol.cpp
    src/protocol/mcp_transport.cpp
    src/protocol/stdio_transport.cpp
    src/protocol/tcp_transport.cpp
    src/protocol/websocket_transport.cpp
    src/client/mcp_client.cpp
    src/client/mcp_attachment.cpp
    src/client/mcp_client_functions.cpp
    src/server/mcp_server.cpp
    src/server/resource_providers.cpp
    src/server/tool_handlers.cpp
    src/server/mcp_server_functions.cpp
    src/security/security_manager.cpp
    src/monitoring/metrics_collector.cpp
)

build_static_extension(mcp ${MCP_EXTENSION_FILES})
build_loadable_extension(mcp ${PARAMETERS} ${MCP_EXTENSION_FILES})
```

## Risk Mitigation

### Technical Risks
- **Complex Protocol**: Comprehensive test suite and reference implementations
- **Performance**: Early benchmarking and optimization focus
- **Security**: Security-first design with comprehensive audit logging
- **Cross-platform**: CI testing on all target platforms

### Scope Risks  
- **Feature Creep**: Strict phase gates and MVP definitions
- **Protocol Changes**: Version management and backward compatibility
- **Integration Complexity**: Modular design with clear interfaces

## Success Metrics

### Phase 1 Success
- [ ] Basic MCPFS file reading working
- [ ] stdio transport stable
- [ ] Core test suite passing

### Phase 2 Success  
- [ ] All transport types working
- [ ] Tool execution functional
- [ ] Resource management complete

### Phase 3 Success
- [ ] Server mode operational
- [ ] Resource publishing working
- [ ] Built-in tools functional

### Phase 4 Success
- [ ] Production-ready performance
- [ ] Comprehensive security
- [ ] Full monitoring capabilities

## Conclusion

This roadmap provides a structured approach to implementing comprehensive MCP integration for DuckDB. The phased approach ensures incremental value delivery while managing complexity and risk. The unified extension architecture provides maximum flexibility while maintaining a clean, focused implementation.