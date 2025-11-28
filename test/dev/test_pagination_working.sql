-- Working pagination test with proper command setup
.echo on

-- Load the MCP extension
LOAD 'duckdb_mcp';

-- Set allowed commands for security (include full python command path with arguments)
SET allowed_mcp_commands='/mnt/aux-data/teague/Projects/duckdb_mcp/venv/bin/python /mnt/aux-data/teague/Projects/duckdb_mcp/test/fastmcp/pagination_test_server.py';

-- Test 1: Basic pagination functions without server (should return NULL)
SELECT '=== Test 1: Functions without server ===' as test_name;
SELECT mcp_list_resources_paginated('nonexistent', '') as result;

-- Test 2: Attach pagination server with correct allowed command
SELECT '=== Test 2: Attach pagination server ===' as test_name;
ATTACH 'stdio:///mnt/aux-data/teague/Projects/duckdb_mcp/venv/bin/python /mnt/aux-data/teague/Projects/duckdb_mcp/test/fastmcp/pagination_test_server.py' AS paginationtest (TYPE mcp);

-- Show attached databases
SHOW DATABASES;

-- Test 3: Test basic list functions (non-paginated) first
SELECT '=== Test 3: Test basic list functions ===' as test_name;
SELECT mcp_list_resources('paginationtest') as resources;

-- Test 4: Now test pagination functions
SELECT '=== Test 4: Test pagination functions ===' as test_name;
SELECT mcp_list_resources_paginated('paginationtest', '') as first_page;

SELECT '=== Pagination Test Completed ===' as status;