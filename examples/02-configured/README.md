# Configured MCP Server Example

A DuckDB MCP server with custom configuration options - persistent database, specific tools enabled, and custom settings.

## What This Example Demonstrates

- Persistent database file
- Selective tool enablement
- Custom server configuration
- Environment variable overrides
- DuckDB settings configuration

## Files

- `launch-mcp.sh` - Entry point with environment variable support
- `mcp.json` - MCP server configuration
- `init-mcp-db.sql` - Database and extension configuration
- `start-mcp-server.sql` - Server startup with custom options
- `config.env` - Optional environment configuration

## Configuration Options

This example shows how to:

1. **Use a persistent database** - Data survives server restarts
2. **Enable/disable specific tools** - Only expose what you need
3. **Set DuckDB options** - Memory limits, threads, etc.
4. **Configure via environment** - Override settings without editing files

## Usage

### Basic Usage

```bash
./launch-mcp.sh
```

### With Custom Database Path

```bash
DB_PATH=/path/to/my.duckdb ./launch-mcp.sh
```

### With Development Build

```bash
DUCKDB=../../build/release/duckdb ./launch-mcp.sh
```

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `DUCKDB` | `duckdb` | Path to DuckDB binary |
| `DB_PATH` | `./data.duckdb` | Database file path |
| `MEMORY_LIMIT` | `2GB` | DuckDB memory limit |
| `THREADS` | `4` | Number of worker threads |

## Available Tools

This example enables a read-heavy configuration:

- `query` - Execute SQL queries
- `describe` - Describe tables or queries
- `list_tables` - List all tables
- `database_info` - Get database overview
- `export` - Export query results

The `execute` tool is **disabled** in this example for safety.

## Test Calls

```json
// Query with specific format
{"method": "tools/call", "params": {"name": "query", "arguments": {"sql": "SELECT 1 + 1 AS result", "format": "json"}}}

// Export to CSV format
{"method": "tools/call", "params": {"name": "export", "arguments": {"query": "SELECT 1, 2, 3", "format": "csv"}}}
```
