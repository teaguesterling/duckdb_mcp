-- DuckDB MCP Server Initialization
-- This file runs before the server starts
INSTALL duckdb_mcp FROM community;

-- Load the MCP extension
LOAD duckdb_mcp;

-- For simple example, we just use an empty in-memory database
-- No additional setup needed
