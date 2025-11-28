# DuckDB MCP Extension v1.0.0 - Initial Release 🦆🔗

## 🎉 Introducing the DuckDB MCP Extension

We're thrilled to announce the **first stable release** of the DuckDB MCP Extension! This groundbreaking extension brings **Model Context Protocol (MCP)** integration directly into DuckDB, enabling seamless communication between SQL databases and MCP servers via **stdio transport**.

## 🌟 **What is MCP Integration?**

The Model Context Protocol allows applications to expose resources and tools that can be dynamically discovered and utilized. Our extension makes DuckDB both an **MCP client** (consuming external resources) and an **MCP server** (exposing database capabilities) through process-based communication.

## 🚀 **Core Capabilities**

### **📡 MCP Client Features**
- **Resource Access**: Query external data sources through `mcp://` URIs
- **Tool Execution**: Execute MCP tools directly from SQL with `mcp_call_tool()`
- **Seamless Integration**: Use MCP resources with standard DuckDB functions (`read_csv`, `read_parquet`, etc.)
- **File System Protocol**: Full MCPFS support for transparent file access

```sql
-- Connect to an MCP server via stdio
ATTACH 'python3' AS data_server (
    TYPE mcp, 
    TRANSPORT 'stdio', 
    ARGS '["path/to/mcp_server.py"]'
);

-- Query remote resources as if they were local
SELECT * FROM read_csv('mcp://data_server/file:///customers.csv');

-- Execute MCP tools from SQL
SELECT mcp_call_tool('data_server', 'process_data', '{"input": "value"}');
```

### **🖥️ MCP Server Features** *(stdio only)*
- **Resource Publishing**: Expose database tables and views as MCP resources
- **Tool Registration**: Custom SQL-based tools for data processing
- **JSON-RPC 2.0**: Core MCP protocol support
- **Dual Operation Modes**: Background and foreground server modes

```sql
-- Start MCP server exposing database resources (stdio only)
SELECT mcp_server_start('{"transport": "stdio"}');

-- Publish tables as MCP resources
SELECT mcp_publish_table('customers', 'customer_data');
```

### **🔒 Flexible Security Model**
- **Development Mode**: Automatic permissive mode for rapid prototyping
- **Production Security**: Strict allowlists for commands and URLs
- **Native Configuration**: DuckDB-style security settings

```sql
-- Production security configuration
SET mcp_allowed_commands = 'python3,node,uvx';
SET mcp_allowed_urls = 'https://api.example.com/*';
```

## 📋 **Current Implementation Status**

### ✅ **Fully Implemented - v1.0.0**
- **Stdio Transport**: Complete process-based communication via stdin/stdout
- **MCPFS File System**: Full read-only file system protocol support
- **Core MCP Methods**: `initialize`, `resources/list`, `resources/read`, `tools/list`, `tools/call`, `prompts/list`, `prompts/get`
- **Resource Management**: List and read MCP resources with content extraction
- **Tool Execution**: Full tool calling with parameter validation
- **Security Framework**: Allowlist-based security validation with permissive/strict modes
- **Server Architecture**: Resource and tool publishing via stdio transport
- **Configuration System**: JSON config file support with environment variable parsing

### 🚧 **Planned for Future Releases**
- **HTTP Transport**: RESTful MCP communication *(v1.1.0)*
- **WebSocket Transport**: Real-time bidirectional communication *(v1.1.0)*
- **TCP Transport**: Direct socket-based connections *(v1.2.0)*
- **Extended MCP Protocol**: Subscriptions, sampling, notifications *(v1.1.0)*
- **Enhanced Logging**: Comprehensive MCP communication logging *(v1.1.0)*
- **Write Operations**: MCPFS write support *(v1.2.0)*
- **Windows Native Transports**: Windows-specific process management *(v1.3.0)*

### 📋 **Transport Roadmap**

| Transport | Status | Use Case | Target Release |
|-----------|--------|----------|----------------|
| **Stdio** | ✅ **Stable** | Local processes, development | **v1.0.0** |
| **HTTP** | 🚧 Planned | Web services, cloud APIs | v1.1.0 |
| **WebSocket** | 🚧 Planned | Real-time applications | v1.1.0 |
| **TCP** | 🚧 Planned | Direct network connections | v1.2.0 |

## 🎯 **Key Use Cases** *(stdio-based)*

### **Data Integration**
```sql
-- Access remote file systems via MCP processes
SELECT * FROM read_csv('mcp://api_server/data/export.csv');

-- Combine local and remote data
SELECT l.*, r.metadata 
FROM local_table l 
JOIN read_json('mcp://api_server/enrichment/' || l.id) r;
```

### **Tool Orchestration**
```sql
-- Process data through external MCP tools
SELECT mcp_call_tool('ml_server', 'predict', 
    json_object('features', array_agg(feature_col))
) FROM training_data;
```

### **Database as a Service** *(development)*
```sql
-- Expose database capabilities via stdio MCP server
SELECT mcp_server_start('{"transport": "stdio", "expose_all_tables": true}');
```

## 🌍 **Platform Support**

**Fully Supported:**
- Linux (x86_64, musl)
- macOS (Intel, Apple Silicon) 
- WebAssembly (all variants)
- Windows (MinGW - runtime compatibility layer)

## 📚 **Getting Started**

### **Installation**
```bash
# Build from source
git clone https://github.com/teaguesterling/duckdb_mcp
cd duckdb_mcp
make

# Load in DuckDB
./build/release/duckdb
```

### **Quick Example**
```sql
-- Load the extension
LOAD 'duckdb_mcp';

-- Connect to an MCP server (development mode)
ATTACH 'python3' AS data_server (
    TYPE mcp, 
    TRANSPORT 'stdio', 
    ARGS '["examples/sample_server.py"]'
);

-- Start querying!
SELECT * FROM read_csv('mcp://data_server/file:///data.csv');
```

## 🔮 **Future Vision**

This initial release establishes DuckDB as a **first-class citizen** in the MCP ecosystem through robust stdio transport. Future releases will expand to **network transports** (HTTP, WebSocket, TCP), enhance protocol compliance, and add advanced server capabilities, making DuckDB a powerful hub for **AI-driven data workflows**.

## 🏗️ **What Makes This Special**

- **First-of-its-kind**: The only SQL database with native MCP protocol support
- **Bidirectional**: Both client and server capabilities in one extension
- **Production-ready stdio**: Robust process management and security
- **Seamless integration**: MCP resources work with all DuckDB file functions
- **Developer-friendly**: Permissive mode for rapid prototyping, strict mode for production

---

**🚀 Ready to bridge SQL and MCP?**

This v1.0.0 release provides a solid foundation for stdio-based MCP integration. Network transports and advanced features coming soon!

**Links:** [GitHub Repository](https://github.com/teaguesterling/duckdb_mcp) | [Documentation](https://github.com/teaguesterling/duckdb_mcp/blob/main/README.md) | [Examples](https://github.com/teaguesterling/duckdb_mcp/tree/main/test)