# Simple MCP Server

Minimal DuckDB MCP server with default configuration.

## What it Demonstrates

- Basic MCP server setup
- Default tool configuration (all tools enabled)
- Direct duckdb invocation from mcp.json

## Files

- `init-mcp-server.sql` - Loads extension and starts server
- `mcp.json` - MCP client configuration
- `test-calls.ldjson` - Test requests (line-delimited JSON)

## Usage

Add to your MCP client configuration:

```json
{
  "mcpServers": {
    "duckdb-simple": {
      "command": "duckdb",
      "args": ["-init", "init-mcp-server.sql"]
    }
  }
}
```

## Testing

```bash
cat test-calls.ldjson | duckdb -init init-mcp-server.sql 2>/dev/null | jq .
```
