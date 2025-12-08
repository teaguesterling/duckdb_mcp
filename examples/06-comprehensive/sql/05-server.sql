-- 05-server.sql: MCP Server start
-- Final step - starts the MCP server with configuration

-- Start MCP server with analytics-focused config
-- - Execute disabled for safety
-- - Dangerous query patterns blocked
SELECT mcp_server_start('stdio', '{
  "enable_query_tool": true,
  "enable_describe_tool": true,
  "enable_list_tables_tool": true,
  "enable_database_info_tool": true,
  "enable_export_tool": true,
  "enable_execute_tool": false,
  "denied_queries": ["DROP", "TRUNCATE", "ALTER", "COPY TO"]
}');
