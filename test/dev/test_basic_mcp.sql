-- Basic MCP test to establish connectivity
.echo on

-- Load the MCP extension  
LOAD 'duckdb_mcp';

-- Set allowed commands (use existing working server)
SET allowed_mcp_commands='/mnt/aux-data/teague/Projects/duckdb_mcp/venv/bin/python /mnt/aux-data/teague/Projects/duckdb_mcp/test/fastmcp/sample_data_server.py';

-- Attach the sample data server
ATTACH 'stdio:///mnt/aux-data/teague/Projects/duckdb_mcp/venv/bin/python /mnt/aux-data/teague/Projects/duckdb_mcp/test/fastmcp/sample_data_server.py' AS testserver (TYPE mcp);

-- Show databases
SHOW DATABASES;

-- Test basic MCP functions
SELECT mcp_list_resources('testserver') as resources;

-- Now test our pagination functions with this server
SELECT mcp_list_resources_paginated('testserver', '') as paginated_resources;

SELECT '=== Basic MCP Test Completed ===' as status;