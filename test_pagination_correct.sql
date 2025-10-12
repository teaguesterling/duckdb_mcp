-- Correct pagination test using proper ATTACH syntax from README
.echo on

-- Load the MCP extension
LOAD 'duckdb_mcp';

-- Test 1: Basic pagination functions without server (should return NULL)
SELECT '=== Test 1: Functions without server ===' as test_name;
SELECT mcp_list_resources_paginated('nonexistent', '') as result;

-- Test 2: Attach our pagination test server using correct syntax
SELECT '=== Test 2: Attach pagination server (correct syntax) ===' as test_name;
ATTACH '/mnt/aux-data/teague/Projects/duckdb_mcp/venv/bin/python' AS paginationtest (
    TYPE mcp, 
    ARGS '["/mnt/aux-data/teague/Projects/duckdb_mcp/test/fastmcp/pagination_test_server.py"]'
);

-- Show databases to verify attachment
SHOW DATABASES;

-- Test 3: Test basic list functions (non-paginated) first
SELECT '=== Test 3: Test basic list functions ===' as test_name;
SELECT mcp_list_resources('paginationtest') as resources;
SELECT mcp_list_tools('paginationtest') as tools;

-- Test 4: Now test our pagination functions
SELECT '=== Test 4: Test pagination functions ===' as test_name;
SELECT mcp_list_resources_paginated('paginationtest', '') as resources_page1;
SELECT mcp_list_prompts_paginated('paginationtest', '') as prompts_page1;
SELECT mcp_list_tools_paginated('paginationtest', '') as tools_page1;

-- Test 5: Extract and examine pagination structure
SELECT '=== Test 5: Examine pagination structure ===' as test_name;
WITH pagination_test AS (
    SELECT mcp_list_resources_paginated('paginationtest', '') as result
)
SELECT 
    typeof(result) as result_type,
    result.items as items,
    result.next_cursor as next_cursor,
    result.has_more_pages as has_more_pages,
    result.total_items as total_items
FROM pagination_test
WHERE result IS NOT NULL;

-- Test 6: Test cursor-based pagination (get second page)
SELECT '=== Test 6: Test cursor navigation ===' as test_name;
WITH first_page AS (
    SELECT mcp_list_resources_paginated('paginationtest', '') as result
),
second_page AS (
    SELECT mcp_list_resources_paginated('paginationtest', first_page.result.next_cursor) as result
    FROM first_page
    WHERE first_page.result.next_cursor IS NOT NULL AND first_page.result.next_cursor != ''
)
SELECT 
    'Second page test' as test_name,
    result.total_items as page2_item_count,
    result.has_more_pages as page2_has_more,
    result.next_cursor IS NOT NULL as page2_has_cursor
FROM second_page;

SELECT '=== Pagination Test Completed Successfully ===' as status;