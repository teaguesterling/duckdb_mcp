-- init-mcp-db.sql: DevOps MCP server initialization
-- Loads extensions for git, test results, YAML, and observability

-- ============================================
-- Load Community Extensions
-- ============================================

-- Git repository access
INSTALL duck_tails FROM community;
LOAD duck_tails;

-- Test/build result parsing
INSTALL duck_hunt FROM community;
LOAD duck_hunt;

-- YAML configuration parsing
INSTALL yaml FROM community;
LOAD yaml;

-- OpenTelemetry observability
INSTALL otlp FROM community;
LOAD otlp;

-- File system extensions
INSTALL hostfs FROM community;
LOAD hostfs;

INSTALL shellfs FROM community;
LOAD shellfs;

INSTALL zipfs FROM community;
LOAD zipfs;

-- ============================================
-- Load Core Extensions
-- ============================================

-- IP address functions
INSTALL inet;
LOAD inet;

-- JSON processing
INSTALL json;
LOAD json;

-- HTTP/S3 access
INSTALL httpfs;
LOAD httpfs;

.print '[init-mcp-db] DevOps extensions loaded'

-- ============================================
-- Helper Views (optional)
-- ============================================

-- Recent git activity view
CREATE OR REPLACE VIEW recent_commits AS
SELECT
    hash[:8] as short_hash,
    author_name,
    author_date,
    message[:60] as message_preview
FROM git_log()
ORDER BY author_date DESC
LIMIT 50;

-- Branch summary
CREATE OR REPLACE VIEW branch_summary AS
SELECT * FROM git_branches();

.print '[init-mcp-db] Helper views created'
