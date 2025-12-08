# Comprehensive MCP Server Example

A full-featured DuckDB MCP server demonstrating all capabilities together.

## What This Example Demonstrates

- Complete production-like setup
- Modular SQL file organization
- Security with selective permissions
- Custom domain-specific tools
- Multi-tenant SaaS application schema

## Architecture

```
06-comprehensive/
├── init-mcp-server.sql     # Entry point (orchestrates SQL loading)
├── mcp.json                # Client configuration
├── sql/
│   ├── 00-init.sql         # Extension loading
│   ├── 01-schema.sql       # Database schema
│   ├── 02-seed-data.sql    # Sample data
│   ├── 03-views.sql        # Views and materialized queries
│   ├── 04-macros.sql       # Custom macros (tools)
│   └── 05-server.sql       # Server configuration and start
└── test-calls.ldjson       # Test suite (line-delimited JSON)
```

## Features

### Data Model
- Multi-tenant SaaS application schema
- Organizations, users, projects, tasks
- Activity logging and analytics

### Security
- Execute tool disabled
- Blocked dangerous operations (DROP, TRUNCATE, ALTER, COPY TO)

### Custom Tools
- `org_dashboard` - Organization metrics
- `user_activity` - User activity report
- `project_status` - Project health check
- `task_search` - Search tasks with filters

## Usage

Add to your MCP client configuration:

```json
{
  "mcpServers": {
    "duckdb-app": {
      "command": "duckdb",
      "args": ["-init", "init-mcp-server.sql"]
    }
  }
}
```

## Test Suite

The `test-calls.ldjson` includes comprehensive tests:

1. **Discovery** - List tools, tables, database info
2. **Queries** - Various SELECT patterns
3. **Custom Tools** - All domain-specific tools
4. **Security** - Verify blocked operations

Run tests:

```bash
cat test-calls.ldjson | duckdb -init init-mcp-server.sql 2>/dev/null | jq .
```
