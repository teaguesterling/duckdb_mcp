-- start-mcp-server.sql: Start MCP server for Web API workflows

SELECT mcp_server_start('stdio', 'localhost', 0, '{
    "enable_query_tool": true,
    "enable_describe_tool": true,
    "enable_list_tables_tool": true,
    "enable_database_info_tool": true,
    "enable_export_tool": true,
    "enable_execute_tool": false
}');
