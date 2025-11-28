-- Test MCP-compliant cursor-based pagination
.echo on

-- Load the MCP extension
LOAD 'duckdb_mcp';

-- Test 1: Test basic functions without cursor (existing functionality preserved)
SELECT '=== Test 1: Basic functions without cursor ===' as test_name;
ATTACH '/mnt/aux-data/teague/Projects/duckdb_mcp/venv/bin/python' AS mcptest (
    TYPE mcp, 
    ARGS '["/mnt/aux-data/teague/Projects/duckdb_mcp/test/fastmcp/pagination_test_server.py"]'
);

-- Show databases to verify attachment
SHOW DATABASES;

-- Test 2: Test standard MCP functions (single parameter)
SELECT '=== Test 2: Standard MCP functions (single parameter) ===' as test_name;
SELECT json_array_length(json_extract(mcp_list_resources('mcptest'), '$.resources')) as resource_count_page1;
SELECT json_array_length(json_extract(mcp_list_tools('mcptest'), '$.tools')) as tool_count_page1;
SELECT json_array_length(json_extract(mcp_list_prompts('mcptest'), '$.prompts')) as prompt_count_page1;

-- Test 3: Test MCP-compliant cursor-based pagination
SELECT '=== Test 3: MCP-compliant pagination (first page) ===' as test_name;
SELECT 
    json_array_length(json_extract(mcp_list_resources('mcptest', ''), '$.resources')) as resource_count,
    json_extract(mcp_list_resources('mcptest', ''), '$.nextCursor') IS NOT NULL as has_next_cursor;

SELECT 
    json_array_length(json_extract(mcp_list_tools('mcptest', ''), '$.tools')) as tool_count,
    json_extract(mcp_list_tools('mcptest', ''), '$.nextCursor') IS NOT NULL as has_next_cursor;

SELECT 
    json_array_length(json_extract(mcp_list_prompts('mcptest', ''), '$.prompts')) as prompt_count,
    json_extract(mcp_list_prompts('mcptest', ''), '$.nextCursor') IS NOT NULL as has_next_cursor;

-- Test 4: Test cursor navigation (get second page)
SELECT '=== Test 4: Cursor navigation (second page) ===' as test_name;
WITH first_page AS (
    SELECT mcp_list_resources('mcptest', '') as result
),
second_page AS (
    SELECT 
        mcp_list_resources('mcptest', json_extract(first_page.result, '$.nextCursor')) as result
    FROM first_page
    WHERE json_extract(first_page.result, '$.nextCursor') IS NOT NULL
)
SELECT 
    json_array_length(json_extract(second_page.result, '$.resources')) as page2_resource_count,
    json_extract(second_page.result, '$.nextCursor') IS NOT NULL as page2_has_next_cursor
FROM second_page;

-- Test 5: Test complete pagination flow for resources
SELECT '=== Test 5: Complete pagination flow ===' as test_name;
WITH RECURSIVE pagination_flow AS (
    -- Start with first page
    SELECT 
        1 as page_num,
        mcp_list_resources('mcptest', '') as result
    
    UNION ALL
    
    -- Get next pages using cursor
    SELECT 
        page_num + 1,
        mcp_list_resources('mcptest', json_extract(result, '$.nextCursor')) as result
    FROM pagination_flow
    WHERE json_extract(result, '$.nextCursor') IS NOT NULL 
      AND page_num < 10  -- Safety limit
)
SELECT 
    page_num,
    json_array_length(json_extract(result, '$.resources')) as resources_on_page,
    json_extract(result, '$.nextCursor') IS NOT NULL as has_next_page
FROM pagination_flow
ORDER BY page_num;

-- Test 6: Verify MCP specification compliance
SELECT '=== Test 6: MCP specification compliance ===' as test_name;
WITH page_test AS (
    SELECT mcp_list_resources('mcptest', '') as result
)
SELECT 
    typeof(json_extract(result, '$.resources')) as resources_type,
    typeof(json_extract(result, '$.nextCursor')) as cursor_type,
    length(json_extract(result, '$.nextCursor')) > 0 as cursor_has_content
FROM page_test
WHERE json_extract(result, '$.nextCursor') IS NOT NULL;

SELECT '=== MCP-Compliant Pagination Test Completed Successfully ===' as final_status;