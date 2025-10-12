-- Test script for MCP logging functionality
-- This tests the logging system without requiring a full MCP server

-- Load the extension
LOAD 'duckdb_mcp';

-- Test 1: Check diagnostics function (should work immediately)
SELECT 'Testing diagnostics function...' as test;
SELECT mcp_get_diagnostics() as diagnostics;

-- Test 2: Configure logging settings
SELECT 'Configuring logging settings...' as test;
SET mcp_log_level = 'debug';
SET mcp_console_logging = true;
SET mcp_log_file = '/tmp/mcp_test.log';

-- Test 3: Check diagnostics again to see changes
SELECT 'Checking updated diagnostics...' as test;
SELECT mcp_get_diagnostics() as updated_diagnostics;

-- Test 4: Try different log levels
SELECT 'Testing different log levels...' as test;
SET mcp_log_level = 'info';
SELECT mcp_get_diagnostics() as info_level;

SET mcp_log_level = 'error';
SELECT mcp_get_diagnostics() as error_level;

SET mcp_log_level = 'off';
SELECT mcp_get_diagnostics() as off_level;

-- Test 5: Test invalid log level (should not crash)
SELECT 'Testing invalid log level...' as test;
SET mcp_log_level = 'invalid';
SELECT mcp_get_diagnostics() as invalid_test;

-- Reset to warn level
SET mcp_log_level = 'warn';
SELECT 'Test completed - logging system functional!' as result;