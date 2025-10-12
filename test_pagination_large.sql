-- MCP Pagination Large Dataset Tests
-- Tests the pagination functions and data structures with various scenarios

.echo on

-- Test 1: Basic pagination function availability and signatures
SELECT '=== Test 1: Function Registration ===' as test_group;

SELECT function_name, parameter_types, return_type 
FROM duckdb_functions() 
WHERE function_name LIKE 'mcp%paginated%' 
ORDER BY function_name;

-- Test 2: Pagination function error handling with various inputs
SELECT '=== Test 2: Error Handling ===' as test_group;

-- Test with NULL server
SELECT 'NULL server' as test_case, mcp_list_resources_paginated(NULL, '') IS NULL as passes;

-- Test with empty server
SELECT 'Empty server' as test_case, mcp_list_resources_paginated('', '') IS NULL as passes;

-- Test with various cursor inputs
SELECT 'NULL cursor' as test_case, mcp_list_resources_paginated('test', NULL) IS NULL as passes;

-- Test 3: Pagination struct result type validation
SELECT '=== Test 3: Result Structure ===' as test_group;

-- Create a query that would return pagination structure if server existed
WITH pagination_test AS (
    SELECT mcp_list_resources_paginated('nonexistent', '') as result
)
SELECT 
    'Result type' as test_case,
    typeof(result) as actual_type,
    typeof(result) = 'STRUCT(items JSON[], next_cursor VARCHAR, has_more_pages BOOLEAN, total_items BIGINT)' as correct_type
FROM pagination_test;

-- Test 4: All three pagination functions consistency
SELECT '=== Test 4: Function Consistency ===' as test_group;

WITH consistency_test AS (
    SELECT 
        mcp_list_resources_paginated('test', '') as resources_result,
        mcp_list_prompts_paginated('test', '') as prompts_result,
        mcp_list_tools_paginated('test', '') as tools_result
)
SELECT 
    'Type consistency' as test_case,
    typeof(resources_result) = typeof(prompts_result) and 
    typeof(prompts_result) = typeof(tools_result) as all_same_type,
    typeof(resources_result) as common_type
FROM consistency_test;

-- Test 5: Performance test with multiple calls
SELECT '=== Test 5: Performance Test ===' as test_group;

-- Simulate multiple pagination calls (simulating large dataset scenario)
WITH RECURSIVE call_sequence(call_num) AS (
    SELECT 1
    UNION ALL 
    SELECT call_num + 1 FROM call_sequence WHERE call_num < 100
),
pagination_calls AS (
    SELECT 
        call_num,
        mcp_list_resources_paginated('server_' || call_num, 'cursor_' || call_num) as result,
        current_timestamp as call_time
    FROM call_sequence
)
SELECT 
    'Multiple calls test' as test_case,
    COUNT(*) as total_calls,
    COUNT(result) as non_null_results,
    COUNT(CASE WHEN result IS NULL THEN 1 END) as null_results,
    MAX(call_time) - MIN(call_time) as execution_time_diff
FROM pagination_calls;

-- Test 6: Cursor handling with various formats
SELECT '=== Test 6: Cursor Format Handling ===' as test_group;

WITH cursor_tests AS (
    SELECT unnest([
        '', 
        'simple_cursor', 
        'cursor_with_numbers_123', 
        'cursor-with-dashes',
        'cursor_with_underscores',
        'very_long_cursor_' || repeat('a', 100),
        '{"complex": "json_cursor"}',
        'base64_like_ABCdef123=='
    ]) as cursor_value
)
SELECT 
    'Cursor: ' || left(cursor_value, 20) || (CASE WHEN length(cursor_value) > 20 THEN '...' ELSE '' END) as test_case,
    mcp_list_resources_paginated('test_server', cursor_value) IS NULL as handles_gracefully
FROM cursor_tests;

-- Test 7: Memory and structure validation
SELECT '=== Test 7: Memory Usage Validation ===' as test_group;

-- Test with very long server names and cursors to validate memory handling
WITH memory_test AS (
    SELECT 
        mcp_list_resources_paginated(
            repeat('long_server_name_', 100),
            repeat('long_cursor_value_', 100)
        ) as result
)
SELECT 
    'Long input handling' as test_case,
    result IS NULL as handles_long_inputs,
    length(repeat('long_server_name_', 100)) as server_name_length,
    length(repeat('long_cursor_value_', 100)) as cursor_length
FROM memory_test;

-- Test 8: Concurrent simulation (batch operations)
SELECT '=== Test 8: Batch Operations Test ===' as test_group;

-- Simulate what would happen with concurrent pagination requests
WITH batch_test AS (
    SELECT 
        generate_series as batch_id,
        mcp_list_resources_paginated('batch_server_' || generate_series, 'page_' || generate_series) as resources,
        mcp_list_prompts_paginated('batch_server_' || generate_series, 'page_' || generate_series) as prompts,
        mcp_list_tools_paginated('batch_server_' || generate_series, 'page_' || generate_series) as tools
    FROM generate_series(1, 50)
)
SELECT 
    'Batch processing' as test_case,
    COUNT(*) as total_batches,
    COUNT(resources) as resource_calls,
    COUNT(prompts) as prompt_calls,
    COUNT(tools) as tool_calls,
    COUNT(CASE WHEN resources IS NULL AND prompts IS NULL AND tools IS NULL THEN 1 END) as all_null_batches
FROM batch_test;

SELECT '=== Pagination Large Dataset Tests Completed ===' as final_message;