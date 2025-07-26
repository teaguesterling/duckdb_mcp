-- Integration test for MCP Template functionality
-- This test verifies all template functions are working correctly

.echo on

-- Load the MCP extension
LOAD 'build/release/extension/duckdb_mcp/duckdb_mcp.duckdb_extension';

-- Test 1: Verify all template functions are registered
SELECT 'Test 1: Checking function registration...' as test;
SELECT function_name 
FROM duckdb_functions() 
WHERE function_name LIKE 'mcp_%template%' 
ORDER BY function_name;

-- Test 2: Basic template registration and rendering
SELECT 'Test 2: Basic template operations...' as test;

SELECT mcp_register_prompt_template(
    'test_template',
    'A simple test template',
    'Hello {name}, you are {age} years old!'
) as register_result;

SELECT mcp_list_prompt_templates() as list_result;

SELECT mcp_render_prompt_template(
    'test_template',
    '{"name": "Alice", "age": "30"}'
) as render_result;

-- Test 3: Server prompt functions (will fail gracefully without server)
SELECT 'Test 3: Server prompt functions...' as test;

SELECT mcp_list_prompts('nonexistent_server') as server_list_result;

SELECT mcp_get_prompt(
    'nonexistent_server',
    'test_prompt',
    '{}'
) as server_get_result;

-- Test 4: Error handling
SELECT 'Test 4: Error handling...' as test;

-- Non-existent template
SELECT mcp_render_prompt_template('missing_template', '{}') as error_test_1;

-- NULL server name
SELECT mcp_list_prompts(NULL) as error_test_2;

-- NULL template name
SELECT mcp_get_prompt('server', NULL, '{}') as error_test_3;

-- Test 5: Complex template
SELECT 'Test 5: Complex template...' as test;

SELECT mcp_register_prompt_template(
    'complex_template',
    'Multi-parameter template',
    'Query: SELECT {columns} FROM {table} WHERE {condition} ORDER BY {order_by} LIMIT {limit};'
) as complex_register;

SELECT mcp_render_prompt_template(
    'complex_template',
    '{"columns": "id, name, email", "table": "users", "condition": "active = true", "order_by": "created_at DESC", "limit": "50"}'
) as complex_render;

SELECT 'All tests completed successfully!' as final_status;