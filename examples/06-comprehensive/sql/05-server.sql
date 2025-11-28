-- 05-server.sql: MCP Server configuration
-- This file is sourced by start-mcp-server.sql

-- Server configuration is handled in start-mcp-server.sql
-- This file can be used for additional server-time setup

-- Example: Register resources (if supported)
-- SELECT mcp_publish_table('organizations', 'db://tables/organizations', 'json');
-- SELECT mcp_publish_query('SELECT * FROM org_summary', 'db://views/org_summary', 'json', 60);

.print '[05-server] Server configuration ready'
