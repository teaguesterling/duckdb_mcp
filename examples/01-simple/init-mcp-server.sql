-- Simple DuckDB MCP Server
-- Minimal setup - just loads extension and starts server

LOAD 'duckdb_mcp';

SELECT mcp_server_start('stdio');
