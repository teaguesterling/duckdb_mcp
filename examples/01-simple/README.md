# Simple MCP Server Example

The simplest possible DuckDB MCP server - an empty in-memory database with default tools enabled.

## What This Example Demonstrates

- Basic MCP server setup
- Default tools: `query`, `describe`, `export`, `list_tables`, `database_info`
- In-memory database (no persistence)

## Files

- `launch-mcp.sh` - Entry point called by MCP clients
- `mcp.json` - MCP server configuration for clients
- `init-mcp-db.sql` - Database initialization (minimal in this example)
- `start-mcp-server.sql` - Starts the MCP server

## Usage

### With Claude Desktop or other MCP clients

Add to your MCP client configuration (e.g., `~/.config/claude/mcp.json`):

```json
{
  "mcpServers": {
    "duckdb-simple": {
      "command": "/path/to/examples/01-simple/launch-mcp.sh"
    }
  }
}
```

### Manual Testing

```bash
# Using system DuckDB
./launch-mcp.sh

# Using a specific DuckDB binary
DUCKDB=/path/to/duckdb ./launch-mcp.sh

# Using the development build
DUCKDB=../../build/release/duckdb ./launch-mcp.sh
```

## Available Tools

Once connected, you can use:

- `list_tables` - List all tables (will be empty initially)
- `database_info` - Get database overview
- `query` - Execute SQL queries
- `describe` - Describe tables or queries
- `execute` - Run DDL/DML statements (if enabled)

## Test Calls

```json
// List tables (empty database)
{"method": "tools/call", "params": {"name": "list_tables", "arguments": {}}}

// Get database info
{"method": "tools/call", "params": {"name": "database_info", "arguments": {}}}

// Create a table
{"method": "tools/call", "params": {"name": "execute", "arguments": {"statement": "CREATE TABLE test (id INTEGER, name VARCHAR)"}}}

// Query the table
{"method": "tools/call", "params": {"name": "query", "arguments": {"sql": "SELECT * FROM test"}}}
```
