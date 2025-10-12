-- Simple pagination test with attached MCP server
.echo on

-- Load the MCP extension
LOAD 'duckdb_mcp';

-- Set allowed commands for security
SET allowed_mcp_commands='/mnt/aux-data/teague/Projects/duckdb_mcp/venv/bin/python';

-- Test 1: Basic pagination functions without server (should return NULL)
SELECT '=== Test 1: Functions without server ===' as test_name;
SELECT mcp_list_resources_paginated('nonexistent', '') as result;
SELECT mcp_list_prompts_paginated('nonexistent', '') as result;
SELECT mcp_list_tools_paginated('nonexistent', '') as result;

-- Test 2: Try to attach our pagination server
SELECT '=== Test 2: Attach pagination server ===' as test_name;
ATTACH 'stdio:///mnt/aux-data/teague/Projects/duckdb_mcp/venv/bin/python /mnt/aux-data/teague/Projects/duckdb_mcp/test/fastmcp/pagination_test_server.py' AS paginationtest (TYPE mcp);

-- Show attached databases
SHOW DATABASES;

-- Test 3: Test pagination functions with attached server
SELECT '=== Test 3: Test pagination with real server ===' as test_name;
SELECT mcp_list_resources_paginated('paginationtest', '') as first_page;

-- Test 4: Test all three pagination functions
SELECT '=== Test 4: All pagination functions ===' as test_name;
SELECT 
    'resources' as type,
    mcp_list_resources_paginated('paginationtest', '') as result
UNION ALL
SELECT 
    'prompts' as type,
    mcp_list_prompts_paginated('paginationtest', '') as result
UNION ALL
SELECT 
    'tools' as type,
    mcp_list_tools_paginated('paginationtest', '') as result;

-- Test 5: Check structure of pagination result
SELECT '=== Test 5: Pagination result structure ===' as test_name;
WITH pagination_result AS (
    SELECT mcp_list_resources_paginated('paginationtest', '') as result
)
SELECT 
    result.items as items,
    result.next_cursor as next_cursor,
    result.has_more_pages as has_more_pages,
    result.total_items as total_items
FROM pagination_result
WHERE result IS NOT NULL;

SELECT '=== Pagination Test Completed ===' as status;