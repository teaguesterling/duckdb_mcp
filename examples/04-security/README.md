# Secure MCP Server Example

MCP server with security restrictions and query filtering.

## What it Demonstrates

- Disabled execute and export tools
- Query pattern blocking (DROP, TRUNCATE, ALTER, COPY)
- Safe views for data access (masking sensitive fields)
- Read-only analytical access pattern

## Configuration

```json
{
  "enable_query_tool": true,
  "enable_describe_tool": true,
  "enable_list_tables_tool": true,
  "enable_database_info_tool": true,
  "enable_export_tool": false,
  "enable_execute_tool": false,
  "denied_queries": ["DROP", "TRUNCATE", "ALTER", "COPY"]
}
```

## Schema

- **sensitive_data** - Contains account balances (protected)
- **audit_log** - Action audit trail
- **safe_user_summary** - View that masks sensitive balance data

## Files

- `launch-mcp.sh` - Server entry point with security config
- `init-mcp-db.sql` - Schema with sensitive data and safe views
- `mcp.json` - MCP client configuration
- `test-calls.ldjson` - Tests including blocked operations

## Usage

```bash
./launch-mcp.sh
```

## Security Tests

The test file includes operations that should be blocked:
- Execute tool calls (tool not found)
- Export tool calls (tool not found)
- DROP TABLE queries (denied pattern)
- COPY commands (denied pattern)

```bash
cat test-calls.ldjson | DUCKDB=/path/to/duckdb ./launch-mcp.sh 2>/dev/null
```
