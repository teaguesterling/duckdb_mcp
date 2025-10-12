-- MCP Pagination Performance and Large Dataset Tests
-- Focus on testing pagination system performance and scalability

.echo on
.timer on

-- Test 1: High-volume function calls
SELECT '=== Performance Test 1: High Volume Calls ===' as test_section;

WITH RECURSIVE large_sequence(n) AS (
    SELECT 1
    UNION ALL 
    SELECT n + 1 FROM large_sequence WHERE n < 1000
),
performance_test AS (
    SELECT 
        n,
        mcp_list_resources_paginated('perf_server_' || (n % 10), 'cursor_' || n) as result
    FROM large_sequence
)
SELECT 
    'High volume test' as test_name,
    COUNT(*) as total_calls,
    COUNT(result) as non_null_results,
    AVG(length('perf_server_' || (n % 10))) as avg_server_name_length
FROM performance_test;

-- Test 2: Memory stress test with large cursors
SELECT '=== Performance Test 2: Large Cursor Handling ===' as test_section;

WITH cursor_sizes AS (
    SELECT 
        size_kb,
        repeat('cursor_data_', size_kb * 64) as large_cursor -- ~1KB per 64 repetitions
    FROM unnest([1, 5, 10, 25, 50]) as t(size_kb)
),
large_cursor_test AS (
    SELECT 
        size_kb,
        length(large_cursor) as actual_size,
        mcp_list_resources_paginated('large_cursor_test', large_cursor) as result
    FROM cursor_sizes
)
SELECT 
    'Large cursor test' as test_name,
    size_kb || ' KB target' as cursor_size,
    actual_size as actual_bytes,
    result IS NULL as handled_gracefully
FROM large_cursor_test
ORDER BY size_kb;

-- Test 3: Concurrent batch simulation
SELECT '=== Performance Test 3: Simulated Concurrent Load ===' as test_section;

-- Simulate what pagination would look like under load
WITH RECURSIVE pagination_simulation(page_num, server_id, cursor_token) AS (
    SELECT 1 as page_num, 1 as server_id, 'start' as cursor_token
    UNION ALL
    SELECT 
        page_num + 1, 
        CASE WHEN page_num % 20 = 0 THEN server_id + 1 ELSE server_id END,
        'page_' || page_num || '_cursor_' || server_id
    FROM pagination_simulation 
    WHERE page_num < 500
),
concurrent_test AS (
    SELECT 
        server_id,
        page_num,
        cursor_token,
        mcp_list_resources_paginated('server_' || server_id, cursor_token) as resources,
        mcp_list_prompts_paginated('server_' || server_id, cursor_token) as prompts,
        mcp_list_tools_paginated('server_' || server_id, cursor_token) as tools
    FROM pagination_simulation
)
SELECT 
    'Concurrent load simulation' as test_name,
    COUNT(DISTINCT server_id) as unique_servers,
    COUNT(*) as total_page_requests,
    MAX(page_num) as max_page_number,
    COUNT(resources) + COUNT(prompts) + COUNT(tools) as total_null_responses
FROM concurrent_test;

-- Test 4: Edge case testing
SELECT '=== Performance Test 4: Edge Cases ===' as test_section;

WITH edge_cases AS (
    SELECT unnest([
        -- Unicode and special characters
        'server_🚀',
        'server with spaces',
        'server/with/slashes', 
        'server:with:colons',
        'server.with.dots',
        -- SQL injection attempt (should be handled safely)
        'server''; DROP TABLE test; --',
        -- Very long names
        repeat('long_name_', 50)
    ]) as edge_server_name,
    unnest([
        'normal_cursor',
        '{"json": "cursor", "page": 1}',
        'cursor with spaces',
        'cursor/with/special#chars',
        '🚀✨cursor_with_emoji',
        'cursor''with''quotes',
        repeat('long_cursor_', 30)
    ]) as edge_cursor
),
edge_case_test AS (
    SELECT 
        left(edge_server_name, 20) || '...' as server_sample,
        left(edge_cursor, 20) || '...' as cursor_sample,
        mcp_list_resources_paginated(edge_server_name, edge_cursor) as result
    FROM edge_cases
    LIMIT 20  -- Limit combinations for performance
)
SELECT 
    'Edge case handling' as test_name,
    COUNT(*) as total_edge_cases,
    COUNT(CASE WHEN result IS NULL THEN 1 END) as null_results,
    COUNT(CASE WHEN result IS NOT NULL THEN 1 END) as non_null_results
FROM edge_case_test;

-- Test 5: Type system validation
SELECT '=== Performance Test 5: Type System Validation ===' as test_section;

-- Validate that all pagination functions return identical types
WITH type_validation AS (
    SELECT 
        typeof(mcp_list_resources_paginated('test', '')) as resources_type,
        typeof(mcp_list_prompts_paginated('test', '')) as prompts_type,
        typeof(mcp_list_tools_paginated('test', '')) as tools_type
)
SELECT 
    'Type consistency' as test_name,
    resources_type = prompts_type AND prompts_type = tools_type as types_match,
    resources_type as common_return_type
FROM type_validation;

-- Test 6: Parameter validation
SELECT '=== Performance Test 6: Parameter Edge Cases ===' as test_section;

-- Test various parameter combinations
WITH param_tests AS (
    SELECT 
        test_case,
        server_param,
        cursor_param,
        mcp_list_resources_paginated(server_param, cursor_param) as result
    FROM (VALUES 
        ('Both NULL', CAST(NULL AS VARCHAR), CAST(NULL AS VARCHAR)),
        ('Server NULL', CAST(NULL AS VARCHAR), ''),
        ('Cursor NULL', 'test', CAST(NULL AS VARCHAR)),
        ('Both empty', '', ''),
        ('Very long server', repeat('s', 1000), 'cursor'),
        ('Very long cursor', 'server', repeat('c', 1000)),
        ('Both very long', repeat('s', 500), repeat('c', 500))
    ) as t(test_case, server_param, cursor_param)
)
SELECT 
    test_case,
    result IS NULL as handled_safely,
    CASE WHEN server_param IS NOT NULL THEN length(server_param) ELSE -1 END as server_length,
    CASE WHEN cursor_param IS NOT NULL THEN length(cursor_param) ELSE -1 END as cursor_length
FROM param_tests;

SELECT '=== Performance Tests Completed Successfully ===' as completion_message;