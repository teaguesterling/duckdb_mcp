#!/bin/bash
# Launch script for DuckDB MCP Server with custom tools (macros)
# Demonstrates using DuckDB macros as query-based tools

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DUCKDB="${DUCKDB:-duckdb}"

# Load schema/macros from init file, then start server with all tools enabled
exec "$DUCKDB" -init "$SCRIPT_DIR/init-mcp-db.sql" -c "
SELECT mcp_server_start('stdio', '{
  \"enable_query_tool\": true,
  \"enable_describe_tool\": true,
  \"enable_list_tables_tool\": true,
  \"enable_database_info_tool\": true,
  \"enable_export_tool\": true,
  \"enable_execute_tool\": true
}');
"
