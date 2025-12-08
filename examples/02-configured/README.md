# Configured MCP Server

MCP server with custom tool configuration.

## What it Demonstrates

- Selective tool enabling/disabling
- Execute tool disabled for read-only access
- Query, describe, list_tables, database_info, export enabled

## Configuration

The server is configured with:
- `enable_query_tool: true`
- `enable_describe_tool: true`
- `enable_list_tables_tool: true`
- `enable_database_info_tool: true`
- `enable_export_tool: true`
- `enable_execute_tool: false` (disabled for safety)

## Files

- `init-mcp-server.sql` - Loads extension, configures and starts server
- `mcp.json` - MCP client configuration
- `test-calls.ldjson` - Test requests including execute (should fail)

## Usage

Add to your MCP client configuration:

```json
{
  "mcpServers": {
    "duckdb-configured": {
      "command": "duckdb",
      "args": ["-init", "init-mcp-server.sql"]
    }
  }
}
```

## Testing

The test file includes an execute call that should return an error since execute is disabled:

```bash
cat test-calls.ldjson | duckdb -init init-mcp-server.sql 2>/dev/null | jq .
```
