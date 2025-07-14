#!/bin/bash

# Test script for DuckDB MCP integration
set -e

echo "Testing DuckDB MCP integration..."

# Get the absolute path to the Python MCP server
SERVER_PATH=$(realpath sample_data_server.py)
VENV_PATH=$(realpath ../../venv)

echo "Starting DuckDB with MCP integration test..."
cd ../..

# Test basic extension loading
echo "1. Testing extension loading..."
echo "LOAD 'duckdb_mcp'; SELECT hello_mcp();" | build/release/duckdb

# Test ATTACH with our MCP server
echo "2. Testing ATTACH with FastMCP server..."
SERVER_CMD="$VENV_PATH/bin/python $SERVER_PATH"
echo "LOAD 'duckdb_mcp'; SET allowed_mcp_commands='$VENV_PATH/bin/python'; ATTACH 'stdio://$SERVER_CMD' AS testdata (TYPE mcp); SHOW DATABASES;" | build/release/duckdb

# Test file access through MCPFS
echo "3. Testing file access through MCPFS..."
echo "LOAD 'duckdb_mcp'; SET allowed_mcp_commands='$VENV_PATH/bin/python'; ATTACH 'stdio://$SERVER_CMD' AS testdata (TYPE mcp); SELECT * FROM read_csv('mcp://testdata/file:///customers.csv');" | build/release/duckdb

echo "All tests completed!"