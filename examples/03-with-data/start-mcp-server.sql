-- Start the MCP Server with all tools enabled
SELECT mcp_server_start('stdio', '{
    "enable_query_tool": true,
    "enable_describe_tool": true,
    "enable_list_tables_tool": true,
    "enable_database_info_tool": true,
    "enable_export_tool": true,
    "enable_execute_tool": true
}');
