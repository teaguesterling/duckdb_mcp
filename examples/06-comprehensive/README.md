# Comprehensive MCP Server Example

A full-featured DuckDB MCP server demonstrating all capabilities together - configuration, data, security, custom tools, and extensibility.

## What This Example Demonstrates

- Complete production-like setup
- Modular SQL file organization
- Environment-based configuration
- Security with selective permissions
- Custom domain-specific tools
- Resource publishing
- Comprehensive test suite

## Architecture

```
06-comprehensive/
├── launch-mcp.sh           # Entry point (handles all config)
├── mcp.json                # Client configuration
├── config.env              # Environment configuration (optional)
├── sql/
│   ├── 00-init.sql         # Extension loading
│   ├── 01-schema.sql       # Database schema
│   ├── 02-seed-data.sql    # Sample data
│   ├── 03-views.sql        # Views and materialized queries
│   ├── 04-macros.sql       # Custom macros (tools)
│   └── 05-server.sql       # Server configuration
├── init-mcp-db.sql         # Orchestrates SQL loading
└── test-calls.ldjson       # Test suite (line-delimited JSON)
```

## Features

### Data Model
- Multi-tenant SaaS application schema
- Organizations, users, projects, tasks
- Activity logging and analytics

### Security
- Read-only mode for analytics queries
- Blocked dangerous operations
- Audit logging

### Custom Tools
- `org_dashboard` - Organization metrics
- `user_activity` - User activity report
- `project_status` - Project health check
- `task_search` - Search tasks with filters

### Published Resources
- Organization list
- Active projects
- Task statistics

## Usage

### Quick Start

```bash
./launch-mcp.sh
```

### Development Mode

```bash
DUCKDB=../../build/release/duckdb ./launch-mcp.sh
```

### Production Mode (read-only)

```bash
READ_ONLY=true ./launch-mcp.sh
```

## Configuration

Edit `config.env` to customize:

```bash
# Database path (empty = in-memory)
DB_PATH=

# Security - read-only mode (requires DB_PATH)
READ_ONLY=false
```

## Test Suite

The `test-calls.ldjson` includes comprehensive tests:

1. **Discovery** - List tools, tables, database info
2. **Queries** - Various SELECT patterns
3. **Custom Tools** - All domain-specific tools
4. **Security** - Verify blocked operations

Run tests:

```bash
cat test-calls.ldjson | DUCKDB=/path/to/duckdb ./launch-mcp.sh 2>/dev/null
```
