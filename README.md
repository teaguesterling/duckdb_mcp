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

### Pagination Support

The extension supports MCP-compliant cursor-based pagination for handling large datasets efficiently:

- **Server-side pagination**: Reduces memory usage and network traffic by retrieving data in chunks
- **Cursor-based navigation**: Uses opaque cursor tokens for reliable page navigation
- **MCP specification compliance**: Implements pagination through standard MCP tool calls
- **Automatic chunking**: Server determines optimal page sizes based on data characteristics

#### Pagination Example

```sql
-- Navigate through all resources using recursive pagination
WITH RECURSIVE pagination_flow AS (
    SELECT 1 as page_num, mcp_list_resources('server', '') as result
    UNION ALL
    SELECT 
        page_num + 1,
        mcp_list_resources('server', json_extract(result, '$.nextCursor')) as result
    FROM pagination_flow
    WHERE json_extract(result, '$.nextCursor') IS NOT NULL 
      AND page_num < 10
)
SELECT page_num, json_array_length(json_extract(result, '$.resources')) as items_count
FROM pagination_flow ORDER BY page_num;
```

### Client Functions

| Function | Purpose | Limitations |
|----------|---------|-------------|
| `mcp_list_resources(server [, cursor])` | List available resources with optional pagination | Returns JSON; cursor enables server-side pagination |
| `mcp_list_tools(server [, cursor])` | List available tools with optional pagination | Returns JSON; cursor enables server-side pagination |
| `mcp_list_prompts(server [, cursor])` | List available prompts with optional pagination | Returns JSON; cursor enables server-side pagination |
| `mcp_get_resource(server, uri)` | Retrieve specific resource content | Content size limited by available memory |
| `mcp_call_tool(server, tool, args)` | Execute tool on MCP server | Tool availability depends on server implementation |
| `read_csv('mcp://server/uri')` | Read CSV via MCP | Standard CSV limitations apply; requires file access |
| `read_parquet('mcp://server/uri')` | Read Parquet via MCP | Parquet format limitations; network latency impact |
| `read_json('mcp://server/uri')` | Read JSON via MCP | JSON parsing limitations; memory constraints |

### Prompt Template Functions

| Function | Purpose | Limitations |
|----------|---------|-------------|
| `mcp_register_prompt_template(name, desc, content)` | Register reusable prompt template locally | Template stored in memory only |
| `mcp_list_prompt_templates()` | List all registered prompt templates | Returns JSON; local templates only |
| `mcp_render_prompt_template(name, args_json)` | Render prompt template with parameters | Requires valid JSON arguments |
| `mcp_list_prompts(server)` | List prompts from MCP server | Depends on server capabilities |
| `mcp_get_prompt(server, name, args)` | Retrieve and render server prompt | Network latency; server availability |

### Server Functions

| Function | Purpose | Limitations |
|----------|---------|-------------|
| `mcp_server_start(transport, host, port, config)` | Start MCP server | One server instance per session |
| `mcp_server_stop()` | Stop MCP server | Graceful shutdown may take time |
| `mcp_server_status()` | Check server status | Basic status information only |
| `mcp_publish_table(table, uri, format)` | Publish table as resource | Static snapshot; no real-time updates |
| `mcp_publish_query(sql, uri, format, interval)` | Publish query result | Refresh interval minimum 60 seconds |

### Built-in Server Tools

When running as an MCP server, DuckDB exposes these tools to clients:

| Tool | Purpose | Default |
|------|---------|---------|
| `query` | Execute SQL SELECT queries (supports `format` parameter) | Enabled |
| `describe` | Get table or query schema information | Enabled |
| `list_tables` | List tables and views with metadata | Enabled |
| `database_info` | Get database overview (schemas, tables, extensions) | Enabled |
| `export` | Export query results to CSV/JSON/Parquet | Enabled |
| `execute` | Run DDL/DML statements (CREATE, INSERT, etc.) | **Disabled** |

#### Query Tool Format Options

The `query` tool supports different output formats via the `format` parameter:

| Format | Description | Best For |
|--------|-------------|----------|
| `json` | JSON array of objects (default) | Programmatic processing |
| `markdown` | GitHub-flavored markdown table | LLM context (token-efficient) |
| `csv` | Comma-separated values | Data export |

Example with format parameter:
```json
{"name": "query", "arguments": {"sql": "SELECT * FROM users", "format": "markdown"}}
```

Markdown format is more token-efficient for LLMs - column names appear once in the header rather than repeated for each row as in JSON.

Tools can be enabled/disabled via server configuration:

```sql
SELECT mcp_server_start('stdio', 'localhost', 0, '{
    "enable_query_tool": true,
    "enable_describe_tool": true,
    "enable_list_tables_tool": true,
    "enable_database_info_tool": true,
    "enable_export_tool": true,
    "enable_execute_tool": false,
    "default_result_format": "json"
}');
```

#### Server Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enable_query_tool` | bool | `true` | Enable SQL query execution |
| `enable_describe_tool` | bool | `true` | Enable schema description |
| `enable_list_tables_tool` | bool | `true` | Enable table listing |
| `enable_database_info_tool` | bool | `true` | Enable database info |
| `enable_export_tool` | bool | `true` | Enable data export |
| `enable_execute_tool` | bool | `false` | Enable DDL/DML execution |
| `default_result_format` | string | `"json"` | Default format for query results (`json`, `markdown`, `csv`) |
| `max_requests` | int | `0` | Max requests before shutdown (0 = unlimited) |
| `background` | bool | `false` | Run server in background thread |

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

-- Use prompt templates
SELECT mcp_register_prompt_template('query_tmpl', 'Reusable query', 'SELECT {cols} FROM {table} WHERE {filter}');
SELECT mcp_render_prompt_template('query_tmpl', '{"cols": "*", "table": "sales", "filter": "date > ''2024-01-01''"}');

-- Use cursor-based pagination for large datasets
SELECT mcp_list_resources('data_server', '') as first_page;
WITH first_page AS (SELECT mcp_list_resources('data_server', '') as result)
SELECT mcp_list_resources('data_server', json_extract(result, '$.nextCursor')) as second_page
FROM first_page WHERE json_extract(result, '$.nextCursor') IS NOT NULL;
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

## Examples

The `examples/` directory contains ready-to-use MCP server configurations:

| Example | Description |
|---------|-------------|
| [01-simple](examples/01-simple/) | Minimal setup with empty in-memory database |
| [02-configured](examples/02-configured/) | Custom configuration with persistent DB and env vars |
| [03-with-data](examples/03-with-data/) | E-commerce schema with sample data and views |
| [04-security](examples/04-security/) | Security features: query restrictions, read-only mode |
| [05-custom-tools](examples/05-custom-tools/) | Domain-specific tools using DuckDB macros |
| [06-comprehensive](examples/06-comprehensive/) | Full-featured SaaS app with modular SQL |
| [07-devops](examples/07-devops/) | DevOps: git history, test results, YAML configs, OpenTelemetry |
| [08-web-api](examples/08-web-api/) | Web API: HTTP client, HTML parsing, JSON schema, cloud storage |
| [09-docs](examples/09-docs/) | Documentation: markdown, FTS, fuzzy matching, templating |
| [10-developer](examples/10-developer/) | Developer: AST parsing, test data generation, cryptography |

Each example includes:
- `init-mcp-server.sql` - SQL initialization that loads extension, sets up schema/data, and starts server
- `mcp.json` - Client configuration file (directly invokes DuckDB)
- `test-calls.ldjson` - Line-delimited JSON test requests for testing

### Quick Start with Examples

```bash
# Run the simple example directly
cd examples/01-simple
duckdb -init init-mcp-server.sql

# Or test with piped JSON-RPC requests
cat test-calls.ldjson | duckdb -init init-mcp-server.sql 2>/dev/null
```

### Testing Examples

Each example includes a `test-calls.ldjson` file containing line-delimited JSON requests:

```bash
# Pipe test calls through the server
cd examples/03-with-data
cat test-calls.ldjson | duckdb -init init-mcp-server.sql 2>/dev/null

# View each response formatted
cat test-calls.ldjson | duckdb -init init-mcp-server.sql 2>/dev/null | jq .
```

### Using with Claude Desktop

Add to your MCP client configuration (uses direct DuckDB invocation):

```json
{
  "mcpServers": {
    "duckdb": {
      "command": "duckdb",
      "args": ["-init", "/path/to/examples/01-simple/init-mcp-server.sql"]
    }
  }
}
```

## Support

- **Documentation**: See `docs/` directory for technical details
- **Examples**: See `examples/` directory for ready-to-use configurations
- **Issues**: Report bugs and feature requests via GitHub issues
- **Testing**: Comprehensive test suite in `test/` directory

## License

MIT License - see [LICENSE](LICENSE) file for details.

---

**Enable seamless MCP integration with SQL using the DuckDB MCP Extension**