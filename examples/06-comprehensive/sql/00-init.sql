-- 00-init.sql: Extension and settings initialization

-- Load the MCP extension
LOAD 'duckdb_mcp';

-- Configure DuckDB settings
SET memory_limit = '2GB';
SET threads = 4;
SET enable_progress_bar = false;

.print '[00-init] Extensions and settings loaded'
