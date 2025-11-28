#!/bin/bash
# Launch script for Configured DuckDB MCP Server
# Supports environment variable overrides

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Load optional config file
if [[ -f "$SCRIPT_DIR/config.env" ]]; then
    source "$SCRIPT_DIR/config.env"
fi

# Configuration with defaults
DUCKDB="${DUCKDB:-duckdb}"
DB_PATH="${DB_PATH:-$SCRIPT_DIR/data.duckdb}"
MEMORY_LIMIT="${MEMORY_LIMIT:-2GB}"
THREADS="${THREADS:-4}"

# Export for use in SQL scripts
export DB_PATH
export MEMORY_LIMIT
export THREADS

# Launch DuckDB with the persistent database
exec "$DUCKDB" "$DB_PATH" \
    -init "$SCRIPT_DIR/init-mcp-db.sql" \
    -f "$SCRIPT_DIR/start-mcp-server.sql"
