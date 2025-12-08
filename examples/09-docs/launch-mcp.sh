#!/bin/bash
# Launch DuckDB MCP server for Documentation workflows
# Requires: markdown, yaml, fts, rapidfuzz, inflector, textplot, minijinja extensions

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DUCKDB="${DUCKDB:-duckdb}"

# Allow community extensions
export DUCKDB_ALLOW_COMMUNITY_EXTENSIONS=1

exec "$DUCKDB" \
    -init "$SCRIPT_DIR/init-mcp-db.sql" \
    -f "$SCRIPT_DIR/start-mcp-server.sql"
