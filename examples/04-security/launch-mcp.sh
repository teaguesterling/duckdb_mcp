#!/bin/bash
# Launch script for Secure DuckDB MCP Server

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DUCKDB="${DUCKDB:-duckdb}"

# Use read-only mode if a database file is specified
# For this example, we use in-memory with read-only simulated via config

exec "$DUCKDB" \
    -init "$SCRIPT_DIR/init-mcp-db.sql" \
    -f "$SCRIPT_DIR/start-mcp-server.sql"
