-- Final comprehensive pagination test
.echo on

-- Load the MCP extension
LOAD 'duckdb_mcp';

-- Test 1: Verify functions exist without server
SELECT '=== Test 1: Function registration check ===' as test_name;
SELECT mcp_list_resources_paginated('nonexistent', '') IS NULL as functions_exist;

-- Test 2: Attach our pagination test server
SELECT '=== Test 2: Attach server ===' as test_name;
ATTACH '/mnt/aux-data/teague/Projects/duckdb_mcp/venv/bin/python' AS paginationtest (
    TYPE mcp, 
    ARGS '["/mnt/aux-data/teague/Projects/duckdb_mcp/test/fastmcp/pagination_test_server.py"]'
);

-- Verify attachment
SHOW DATABASES;

-- Test 3: Test standard (non-paginated) MCP functions
SELECT '=== Test 3: Standard MCP functions ===' as test_name;
SELECT json_array_length(mcp_list_resources('paginationtest')) as resource_count;
SELECT json_array_length(mcp_list_tools('paginationtest')) as tool_count;

-- Test 4: Test our paginated functions - first page
SELECT '=== Test 4: Paginated functions - first page ===' as test_name;
WITH first_page AS (
    SELECT mcp_list_resources_paginated('paginationtest', '') as result
)
SELECT 
    typeof(result) as result_type,
    result.total_items as page_size,
    result.has_more_pages as has_more,
    result.next_cursor IS NOT NULL as has_cursor,
    length(result.next_cursor) as cursor_length
FROM first_page
WHERE result IS NOT NULL;

-- Test 5: Test cursor navigation - get second page  
SELECT '=== Test 5: Cursor navigation - second page ===' as test_name;
WITH first_page AS (
    SELECT mcp_list_resources_paginated('paginationtest', '') as result
),
second_page AS (
    SELECT mcp_list_resources_paginated('paginationtest', first_page.result.next_cursor) as result
    FROM first_page
    WHERE first_page.result.next_cursor IS NOT NULL
)
SELECT 
    'Page 2' as page,
    result.total_items as page_size,
    result.has_more_pages as has_more,
    result.next_cursor IS NOT NULL as has_next_cursor
FROM second_page
WHERE result IS NOT NULL;

-- Test 6: Test all paginated function types
SELECT '=== Test 6: All paginated functions ===' as test_name;
SELECT 
    mcp_list_resources_paginated('paginationtest', '').total_items as resources_page_size,
    mcp_list_tools_paginated('paginationtest', '').total_items as tools_page_size,
    mcp_list_prompts_paginated('paginationtest', '').total_items as prompts_page_size;

-- Test 7: Performance test - multiple rapid calls
SELECT '=== Test 7: Performance test ===' as test_name;
WITH RECURSIVE perf_test(n, start_time) AS (
    SELECT 1, now()
    UNION ALL
    SELECT 
        n + 1, 
        start_time
    FROM perf_test
    WHERE n < 10
),
results AS (
    SELECT 
        n,
        mcp_list_resources_paginated('paginationtest', '') IS NOT NULL as success,
        start_time
    FROM perf_test
)
SELECT 
    count(*) as total_calls,
    sum(case when success then 1 else 0 end) as successful_calls,
    sum(case when success then 1 else 0 end) * 100.0 / count(*) as success_rate,
    (max(now()) - min(start_time)) as total_time
FROM results;

SELECT '=== Comprehensive Pagination Test Completed Successfully ===' as final_status;