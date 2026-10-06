-- ZIM Article Lookup MCP Server - Initialization
-- Searches and reads articles from ZIM files (e.g., Wikipedia)
--
-- Usage:
--   ZIM_FILES=/path/to/file.zim duckdb -init init-mcp-server.sql
--
-- Note: Custom tools via mcp_publish_tool don't work with table functions
-- (see github.com/teaguesterling/duckdb_mcp/issues/71). Use the built-in query tool
-- with hardcoded SQL patterns referencing getenv('ZIM_FILES').

-- Install (idempotent) + load. All five are community extensions.
INSTALL duckdb_mcp FROM community;
INSTALL zim FROM community;
INSTALL webbed FROM community;
INSTALL duck_block_utils FROM community;
INSTALL markdown FROM community;

LOAD duckdb_mcp;
LOAD zim;
LOAD webbed;
LOAD duck_block_utils;
LOAD markdown;

-- Start MCP server (stdio transport, blocks until stdin closes)
PRAGMA mcp_server_start('stdio');
