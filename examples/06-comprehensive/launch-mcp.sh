#!/bin/bash
# Comprehensive DuckDB MCP Server Launch Script
# Production-ready with full configuration support

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Load configuration
if [[ -f "$SCRIPT_DIR/config.env" ]]; then
    source "$SCRIPT_DIR/config.env"
fi

# Defaults
DUCKDB="${DUCKDB:-duckdb}"
DB_PATH="${DB_PATH:-}"  # Empty = in-memory
MEMORY_LIMIT="${MEMORY_LIMIT:-2GB}"
THREADS="${THREADS:-4}"
READ_ONLY="${READ_ONLY:-false}"
ENABLE_EXECUTE="${ENABLE_EXECUTE:-false}"

# Export for SQL scripts
export MEMORY_LIMIT
export THREADS
export READ_ONLY
export ENABLE_EXECUTE

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

# Launch
exec $DUCKDB_CMD \
    -init "$SCRIPT_DIR/init-mcp-db.sql" \
    -f "$SCRIPT_DIR/start-mcp-server.sql"
