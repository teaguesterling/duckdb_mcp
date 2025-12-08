#!/bin/bash
# Launch script for DuckDB MCP Server - Simple Example
# Can be called directly or via MCP client configuration

set -e

# Allow overriding the DuckDB binary path
DUCKDB="${DUCKDB:-duckdb}"

# Launch DuckDB MCP server directly
# The extension auto-loads if installed, otherwise load explicitly
exec "$DUCKDB" -c "LOAD duckdb_mcp; SELECT mcp_server_start('stdio');"
