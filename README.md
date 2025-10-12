# DuckDB MCP Extension

A Model Context Protocol (MCP) extension for DuckDB that enables seamless integration between SQL databases and MCP servers. This extension allows DuckDB to both consume MCP resources and serve as an MCP server.

## Overview

The DuckDB MCP Extension bridges SQL databases with MCP servers, enabling:
- Direct SQL access to remote resources via MCP protocol
- Tool execution from within SQL queries 
- Database serving as an MCP resource provider
- Flexible security models for development and production

## Core Capabilities

### Client Features
- **Resource Access**: Query remote data sources using `mcp://` URIs with standard SQL functions
- **Tool Execution**: Execute MCP tools directly from SQL with `mcp_call_tool()`
- **Multiple Transports**: Connect via stdio, TCP, and WebSocket protocols
- **Security Modes**: Permissive mode for development, strict allowlists for production
- **Configuration Support**: JSON-based server configuration files

### Server Features  
- **Resource Publishing**: Expose database tables and views as MCP resources
- **Tool Registration**: Register custom SQL-based tools for external access
- **JSON-RPC 2.0**: Complete MCP protocol implementation
- **Multiple Transports**: Support for stdio, TCP, and WebSocket serving

## Function Reference

### Client Functions

| Function | Purpose | Limitations |
|----------|---------|-------------|
| `mcp_list_resources(server)` | List available resources on MCP server | Returns JSON array; limited by server capabilities |
| `mcp_get_resource(server, uri)` | Retrieve specific resource content | Content size limited by available memory |
| `mcp_call_tool(server, tool, args)` | Execute tool on MCP server | Tool availability depends on server implementation |
| `read_csv('mcp://server/uri')` | Read CSV via MCP | Standard CSV limitations apply; requires file access |
| `read_parquet('mcp://server/uri')` | Read Parquet via MCP | Parquet format limitations; network latency impact |
| `read_json('mcp://server/uri')` | Read JSON via MCP | JSON parsing limitations; memory constraints |

### Server Functions

| Function | Purpose | Limitations |
|----------|---------|-------------|
| `mcp_server_start(transport, host, port, config)` | Start MCP server | One server instance per session |
| `mcp_server_stop()` | Stop MCP server | Graceful shutdown may take time |
| `mcp_server_status()` | Check server status | Basic status information only |
| `mcp_publish_table(table, uri, format)` | Publish table as resource | Static snapshot; no real-time updates |
| `mcp_publish_query(sql, uri, format, interval)` | Publish query result | Refresh interval minimum 60 seconds |

## Quick Start

### Installation
```bash
make
./build/release/duckdb
```

### Basic Client Usage
```sql
-- Load extension
LOAD 'duckdb_mcp';

-- Connect to MCP server (development mode)
ATTACH 'python3' AS data_server (
    TYPE mcp, 
    TRANSPORT 'stdio', 
    ARGS '["path/to/server.py"]'
);

-- Access remote data
SELECT * FROM read_csv('mcp://data_server/file:///data.csv');

-- Execute tools
SELECT mcp_call_tool('data_server', 'process_data', '{"table": "sales"}');
```

### Basic Server Usage
```sql
-- Create sample data
CREATE TABLE products AS SELECT * FROM read_csv('products.csv');

-- Start MCP server
SELECT mcp_server_start('stdio', 'localhost', 0, '{}');

-- Publish table as resource
SELECT mcp_publish_table('products', 'data://tables/products', 'json');
```

## Security Configuration

### Development Mode (Permissive)
No security configuration required - all commands allowed:
```sql
ATTACH 'python3' AS server (TYPE mcp, ARGS '["server.py"]');
```

### Production Mode (Strict)  
Configure security settings to enable strict validation:
```sql
-- Enable strict mode
SET allowed_mcp_commands='python3:/usr/bin/python3';

-- All connections now require allowlist validation
ATTACH 'python3' AS secure_server (TYPE mcp, ARGS '["server.py"]');
```

### Security Settings

| Setting | Purpose | Format |
|---------|---------|--------|
| `allowed_mcp_commands` | Allowlist of executable commands | Colon-separated paths |
| `allowed_mcp_urls` | Allowlist of permitted URLs | Space-separated URLs |
| `mcp_server_file` | Configuration file path | File path string |
| `mcp_lock_servers` | Prevent runtime server changes | Boolean |
| `mcp_disable_serving` | Client-only mode | Boolean |

## Limitations

### General Limitations
- **Single Server Instance**: Only one MCP server per DuckDB session
- **Memory Constraints**: Large resources limited by available system memory
- **Transport Support**: TCP/WebSocket transports in development
- **Error Handling**: Limited error recovery mechanisms
- **Performance**: Network latency affects remote resource access

### Client Limitations
- **Protocol Version**: MCP 1.0 only
- **Authentication**: No built-in authentication support
- **Caching**: No resource caching mechanism
- **Concurrent Connections**: Limited by system resources
- **Reconnection**: Manual reconnection required on connection loss

### Server Limitations
- **Resource Updates**: Published resources are static snapshots
- **Tool Registration**: SQL-based tools only
- **Client Management**: No client session management
- **Streaming**: No streaming response support
- **Metadata**: Limited resource metadata support

## Configuration Examples

### JSON Configuration File
```json
{
  "mcpServers": {
    "data_server": {
      "command": "python3",
      "args": ["services/data_server.py"],
      "cwd": "/opt/project",
      "env": {"PYTHONPATH": "/opt/lib"}
    }
  }
}
```

### Using Configuration File
```sql
ATTACH 'data_server' AS server (
    TYPE mcp,
    FROM_CONFIG_FILE '/path/to/config.json'
);
```

## Testing

```bash
# Run test suite
make test

# Basic functionality test
python3 test/test_documented_features.py
```

## Architecture

```
┌─────────────────┐    MCP Protocol    ┌─────────────────┐
│   DuckDB Client │◄──────────────────►│   MCP Server    │
│   (SQL Queries) │                    │   (Resources)   │
└─────────────────┘                    └─────────────────┘
         │                                       │
         │ ATTACH/SELECT                         │ Publish
         ▼                                       ▼
┌─────────────────┐                    ┌─────────────────┐
│ DuckDB Instance │                    │ Data Sources    │
│ (This Extension)│                    │ (Files/APIs)    │ 
└─────────────────┘                    └─────────────────┘
```

## Support

- **Documentation**: See `docs/` directory for technical details
- **Issues**: Report bugs and feature requests via GitHub issues
- **Testing**: Comprehensive test suite in `test/` directory

## License

MIT License - see [LICENSE](LICENSE) file for details.

---

**Enable seamless MCP integration with SQL using the DuckDB MCP Extension**