-- Configured DuckDB MCP Server Initialization
-- This file runs before the server starts

-- Load the MCP extension
LOAD 'duckdb_mcp';

-- Configure DuckDB settings
-- Note: These use environment variables set by launch-mcp.sh
-- In practice, you may need to hardcode these or use a different approach

-- Set memory limit (default 2GB)
SET memory_limit = '2GB';

-- Set number of threads (default 4)
SET threads = 4;

-- Enable progress bar for long queries (useful for debugging)
SET enable_progress_bar = false;

-- Set a reasonable timeout for queries
SET statement_timeout = 300;  -- 5 minutes

-- Log configuration
.print 'DuckDB MCP Server initialized with custom configuration'
