#!/bin/bash
# Launch script for DuckDB MCP Server with sample e-commerce data
# Uses init file for schema/data setup

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DUCKDB="${DUCKDB:-duckdb}"

# Load schema/data from init file, then start server
exec "$DUCKDB" -init "$SCRIPT_DIR/init-mcp-db.sql" -c "SELECT mcp_server_start('stdio');"
