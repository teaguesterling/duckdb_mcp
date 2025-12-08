#!/bin/bash
# Launch DuckDB MCP server for Developer workflows
# Requires: sitting_duck, duck_tails, duck_hunt, prql, hashfuncs, crypto, fakeit extensions

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DUCKDB="${DUCKDB:-duckdb}"

# Allow community extensions
export DUCKDB_ALLOW_COMMUNITY_EXTENSIONS=1

exec "$DUCKDB" \
    -init "$SCRIPT_DIR/init-mcp-db.sql" \
    -f "$SCRIPT_DIR/start-mcp-server.sql"
