-- Comprehensive MCP Server Initialization
-- Orchestrates loading of all SQL files in order

.read sql/00-init.sql
.read sql/01-schema.sql
.read sql/02-seed-data.sql
.read sql/03-views.sql
.read sql/04-macros.sql
.read sql/05-server.sql
