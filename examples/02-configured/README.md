# Configured MCP Server Example

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

- `launch-mcp.sh` - Server entry point with inline configuration
- `mcp.json` - MCP client configuration
- `test-calls.ldjson` - Test requests including execute (should fail)

## Usage

```bash
./launch-mcp.sh

# Or specify DuckDB binary
DUCKDB=/path/to/duckdb ./launch-mcp.sh
```

## Testing

The test file includes an execute call that should return an error since execute is disabled:

```bash
cat test-calls.ldjson | DUCKDB=/path/to/duckdb ./launch-mcp.sh 2>/dev/null
```
