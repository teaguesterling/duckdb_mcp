#!/bin/bash
# Launch script for DuckDB MCP Server - Configured Example
# Read-only configuration with execute tool disabled

set -e

DUCKDB="${DUCKDB:-duckdb}"

# Launch with custom configuration
# - All read tools enabled
# - Execute tool disabled for safety
exec "$DUCKDB" -c "
LOAD duckdb_mcp;
SELECT mcp_server_start('stdio', '{
  \"enable_query_tool\": true,
  \"enable_describe_tool\": true,
  \"enable_list_tables_tool\": true,
  \"enable_database_info_tool\": true,
  \"enable_export_tool\": true,
  \"enable_execute_tool\": false
}');
"
