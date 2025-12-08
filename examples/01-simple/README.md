# Simple MCP Server Example

The simplest possible DuckDB MCP server configuration.

## What it Demonstrates

- Minimal MCP server setup with empty in-memory database
- All default tools enabled (query, describe, list_tables, database_info, export)
- Basic MCP protocol flow (initialize, tools/list, tools/call, shutdown)

## Files

- `launch-mcp.sh` - Server entry point
- `mcp.json` - MCP client configuration
- `test-calls.ldjson` - Test requests (line-delimited JSON)

## Usage

```bash
# Start the server
./launch-mcp.sh

# Or specify DuckDB binary
DUCKDB=/path/to/duckdb ./launch-mcp.sh
```

## Testing

```bash
cat test-calls.ldjson | DUCKDB=/path/to/duckdb ./launch-mcp.sh 2>/dev/null
```
