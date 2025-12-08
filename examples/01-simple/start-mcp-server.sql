-- Start the MCP Server
-- This file is executed after init-mcp-db.sql

-- Start the server in stdio mode (for MCP communication)
-- The server will block and handle requests until shutdown
SELECT mcp_server_start('stdio', 'localhost', 0, '{}');
