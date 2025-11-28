#!/bin/bash
# Launch script for DuckDB MCP Server with sample data

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DUCKDB="${DUCKDB:-duckdb}"

# Use in-memory database with pre-loaded schema
exec "$DUCKDB" \
    -init "$SCRIPT_DIR/init-mcp-db.sql" \
    -f "$SCRIPT_DIR/start-mcp-server.sql"
