#!/bin/bash
# Launch script for DuckDB MCP Server
# Can be called directly or via MCP client configuration

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Allow overriding the DuckDB binary path
# Usage: DUCKDB=/path/to/duckdb ./launch-mcp.sh
DUCKDB="${DUCKDB:-duckdb}"

# Launch DuckDB with init script and server startup
exec "$DUCKDB" \
    -init "$SCRIPT_DIR/init-mcp-db.sql" \
    -f "$SCRIPT_DIR/start-mcp-server.sql"
