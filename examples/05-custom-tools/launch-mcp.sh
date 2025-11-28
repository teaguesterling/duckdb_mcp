#!/bin/bash
# Launch script for DuckDB MCP Server with custom tools

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DUCKDB="${DUCKDB:-duckdb}"

exec "$DUCKDB" \
    -init "$SCRIPT_DIR/init-mcp-db.sql" \
    -f "$SCRIPT_DIR/start-mcp-server.sql"
