-- Start MCP Server with custom tools and published resources

-- Publish tables as resources (if mcp_publish_table is available)
-- SELECT mcp_publish_table('products', 'db://tables/products', 'json');
-- SELECT mcp_publish_table('customers', 'db://tables/customers', 'json');

-- Publish views as resources
-- SELECT mcp_publish_query('SELECT * FROM inventory_status', 'db://views/inventory_status', 'json', 60);
-- SELECT mcp_publish_query('SELECT * FROM daily_summary', 'db://reports/daily_summary', 'json', 300);

-- Start the server with all tools enabled
SELECT mcp_server_start('stdio', '{
    "enable_query_tool": true,
    "enable_describe_tool": true,
    "enable_list_tables_tool": true,
    "enable_database_info_tool": true,
    "enable_export_tool": true,
    "enable_execute_tool": true
}');
