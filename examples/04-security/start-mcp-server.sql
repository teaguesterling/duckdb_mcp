-- Start Secure MCP Server
-- Read-only configuration with restricted operations

SELECT mcp_server_start('stdio', '{
    "enable_query_tool": true,
    "enable_describe_tool": true,
    "enable_list_tables_tool": true,
    "enable_database_info_tool": true,
    "enable_export_tool": false,
    "enable_execute_tool": false,
    "denied_queries": ["DROP", "TRUNCATE", "ALTER", "COPY"]
}');
