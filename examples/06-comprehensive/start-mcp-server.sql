-- start-mcp-server.sql: Start the MCP server with appropriate configuration

-- Default configuration: read-only analytics server
-- Execute tool disabled for safety
SELECT mcp_server_start('stdio', '{
    "enable_query_tool": true,
    "enable_describe_tool": true,
    "enable_list_tables_tool": true,
    "enable_database_info_tool": true,
    "enable_export_tool": true,
    "enable_execute_tool": false,
    "denied_queries": ["DROP", "TRUNCATE", "ALTER", "COPY TO"]
}');
