# DuckDB MCP Extension

[![Documentation](https://img.shields.io/badge/docs-readthedocs-blue)](https://duckdb-mcp.readthedocs.io/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) extension for DuckDB that enables seamless integration between SQL databases and AI assistants like Claude.

## Features

- **Run as MCP Server**: Expose your DuckDB database to AI assistants with built-in tools for querying, describing, and exploring data
- **HTTP Transport**: Run as an HTTP server with authentication for web-based clients
- **Act as MCP Client**: Connect to external MCP servers and query their resources using SQL
- **Custom Tools**: Publish parameterized SQL queries as tools that AI assistants can discover and call
- **Multiple Formats**: Output results as JSON, Markdown (token-efficient), or CSV

## Quick Start

### As an MCP Server (for AI Assistants)

Create `init-server.sql`:

```sql
LOAD 'duckdb_mcp';

CREATE TABLE products (id INT, name VARCHAR, price DECIMAL(10,2));
INSERT INTO products VALUES (1, 'Widget', 9.99), (2, 'Gadget', 24.99);

PRAGMA mcp_server_start('stdio');
```

Add to Claude Desktop configuration:

```json
{
  "mcpServers": {
    "my-database": {
      "command": "duckdb",
      "args": ["-init", "/path/to/init-server.sql"]
    }
  }
}
```

Now Claude can query your database directly!

### As an HTTP Server

Run DuckDB as an HTTP server that any HTTP client can connect to:

```sql
LOAD 'duckdb_mcp';

-- Start HTTP server with authentication
PRAGMA mcp_server_start('http', 'localhost', 8080, '{"auth_token": "secret"}');
```

Then connect via HTTP:

```bash
# Health check
curl http://localhost:8080/health

# Query with authentication
curl -X POST http://localhost:8080/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer secret" \
    -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"query","arguments":{"sql":"SELECT 1"}}}'
```

### As an MCP Client

```sql
LOAD 'duckdb_mcp';

-- Connect to an MCP server
ATTACH 'python3' AS server (TYPE mcp, ARGS '["path/to/server.py"]');

-- Read data through MCP
SELECT * FROM read_csv('mcp://server/file:///data.csv');

-- Call tools
SELECT mcp_call_tool('server', 'analyze', '{"dataset": "sales"}');
```

## Built-in Server Tools

| Tool | Description |
|------|-------------|
| `query` | Execute SQL SELECT queries (supports json/markdown/csv format) |
| `describe` | Get table or query schema information |
| `list_tables` | List all tables and views |
| `database_info` | Get database overview |
| `export` | Export query results to files |
| `execute` | Run DDL/DML statements (disabled by default) |

## Publishing Custom Tools

```sql
PRAGMA mcp_publish_tool(
    'search_products',
    'Search products by name',
    'SELECT * FROM products WHERE name ILIKE ''%'' || $query || ''%''',
    '{"query": {"type": "string", "description": "Search term"}}',
    '["query"]',
    'markdown'
);
```

## PRAGMA vs SELECT

All server management and publishing functions are available in two forms:

| Form | When to use | Example |
|------|-------------|---------|
| `PRAGMA` | Init scripts, fire-and-forget | `PRAGMA mcp_server_start('stdio');` |
| `SELECT` | When you need the return value | `SELECT mcp_server_status();` |

`PRAGMA` versions produce no output, making init scripts clean. `SELECT` versions return status messages or structs.

## Documentation

Full documentation is available at **[duckdb-mcp.readthedocs.io](https://duckdb-mcp.readthedocs.io/)**

- [Getting Started](https://duckdb-mcp.readthedocs.io/getting-started/)
- [Server Functions Reference](https://duckdb-mcp.readthedocs.io/reference/server/)
- [Client Functions Reference](https://duckdb-mcp.readthedocs.io/reference/client/)
- [Configuration](https://duckdb-mcp.readthedocs.io/reference/configuration/)
- [Security Guide](https://duckdb-mcp.readthedocs.io/guides/security/)
- [Examples](https://duckdb-mcp.readthedocs.io/examples/)

## Examples

The `examples/` directory contains ready-to-use configurations:

| Example | Description |
|---------|-------------|
| [01-simple](examples/01-simple/) | Minimal setup |
| [02-configured](examples/02-configured/) | Custom configuration |
| [03-with-data](examples/03-with-data/) | E-commerce schema with data |
| [04-security](examples/04-security/) | Security hardening |
| [05-custom-tools](examples/05-custom-tools/) | Custom SQL tools |
| [06-comprehensive](examples/06-comprehensive/) | Full SaaS application |
| [11-http-server](examples/11-http-server/) | HTTP server with authentication |

## Installation

```bash
# Clone and build
git clone https://github.com/teague/duckdb_mcp.git
cd duckdb_mcp
make

# Run
./build/release/duckdb
```

```sql
LOAD 'duckdb_mcp';
```

## Testing

```bash
make test
```

## License

MIT License - see [LICENSE](LICENSE) for details.
