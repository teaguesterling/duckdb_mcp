-- Test MCP Server Template Functions
-- This script tests the new mcp_list_server_templates and mcp_get_server_template functions

.load build/release/extension/duckdb_mcp/duckdb_mcp.duckdb_extension

-- Test 1: Basic template functionality (should work without server)
SELECT 'Starting template tests...' as status;

-- Register a test template
SELECT mcp_register_template('test_prompt', 'A test prompt for demonstration', 'Hello {name}, welcome to {location}!') as result;

-- List local templates
SELECT mcp_list_templates() as local_templates;

-- Render the template
SELECT mcp_render_template('test_prompt', '{"name": "Alice", "location": "DuckDB"}') as rendered;

-- Test 2: Server template functions (will fail gracefully if no server attached)
SELECT 'Testing server template functions...' as status;

-- Test list server templates function (should fail gracefully)
SELECT mcp_list_server_templates('nonexistent_server') as server_list_result;

-- Test get server template function (should fail gracefully)  
SELECT mcp_get_server_template('nonexistent_server', 'some_template', '{}') as server_get_result;

-- Test with null parameters
SELECT mcp_list_server_templates(NULL) as null_server_test;
SELECT mcp_get_server_template(NULL, 'template', '{}') as null_server_test2;
SELECT mcp_get_server_template('server', NULL, '{}') as null_template_test;

SELECT 'Template function tests completed.' as status;