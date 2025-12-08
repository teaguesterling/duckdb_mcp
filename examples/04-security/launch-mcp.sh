#!/bin/bash
# Launch script for Secure DuckDB MCP Server
# Read-only with restricted operations (export/execute disabled, denied query patterns)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DUCKDB="${DUCKDB:-duckdb}"

# Load schema/data, then start with security-focused config
exec "$DUCKDB" -init "$SCRIPT_DIR/init-mcp-db.sql" -c "
SELECT mcp_server_start('stdio', '{
  \"enable_query_tool\": true,
  \"enable_describe_tool\": true,
  \"enable_list_tables_tool\": true,
  \"enable_database_info_tool\": true,
  \"enable_export_tool\": false,
  \"enable_execute_tool\": false,
  \"denied_queries\": [\"DROP\", \"TRUNCATE\", \"ALTER\", \"COPY\"]
}');
"
