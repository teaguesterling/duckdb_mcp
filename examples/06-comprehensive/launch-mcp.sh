#!/bin/bash
# Comprehensive DuckDB MCP Server Launch Script
# Production-ready with multi-file SQL organization

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Load optional configuration
if [[ -f "$SCRIPT_DIR/config.env" ]]; then
    source "$SCRIPT_DIR/config.env"
fi

DUCKDB="${DUCKDB:-duckdb}"
DB_PATH="${DB_PATH:-}"  # Empty = in-memory
READ_ONLY="${READ_ONLY:-false}"

# Build DuckDB command
DUCKDB_CMD="$DUCKDB"

# Add database path if specified
if [[ -n "$DB_PATH" ]]; then
    DUCKDB_CMD="$DUCKDB_CMD $DB_PATH"
fi

# Add read-only flag if enabled
if [[ "$READ_ONLY" == "true" && -n "$DB_PATH" ]]; then
    DUCKDB_CMD="$DUCKDB_CMD -readonly"
fi

# Load schema/data from init file, then start server with analytics config
exec $DUCKDB_CMD -init "$SCRIPT_DIR/init-mcp-db.sql" -c "
SELECT mcp_server_start('stdio', '{
  \"enable_query_tool\": true,
  \"enable_describe_tool\": true,
  \"enable_list_tables_tool\": true,
  \"enable_database_info_tool\": true,
  \"enable_export_tool\": true,
  \"enable_execute_tool\": false,
  \"denied_queries\": [\"DROP\", \"TRUNCATE\", \"ALTER\", \"COPY TO\"]
}');
"
