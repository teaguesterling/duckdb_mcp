-- init-mcp-db.sql: Orchestrates loading of all SQL files
-- This is the entry point for database initialization

.print '============================================'
.print 'Initializing Comprehensive MCP Server'
.print '============================================'

-- Load each SQL file in order
.read sql/00-init.sql
.read sql/01-schema.sql
.read sql/02-seed-data.sql
.read sql/03-views.sql
.read sql/04-macros.sql
.read sql/05-server.sql

.print '============================================'
.print 'Database initialization complete'
.print '============================================'
