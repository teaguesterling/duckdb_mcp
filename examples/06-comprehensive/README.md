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
├── launch-mcp.sh           # Entry point
├── mcp.json                # Client configuration
├── config.env              # Environment configuration
├── sql/
│   ├── 00-init.sql         # Extension loading
│   ├── 01-schema.sql       # Database schema
│   ├── 02-seed-data.sql    # Sample data
│   ├── 03-views.sql        # Views and materialized queries
│   ├── 04-macros.sql       # Custom macros (tools)
│   └── 05-server.sql       # Server configuration
├── init-mcp-db.sql         # Orchestrates SQL loading
├── start-mcp-server.sql    # Starts the server
└── test-calls.json         # Test suite
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
# Database
DB_PATH=./app.duckdb

# Security
READ_ONLY=false
ENABLE_EXECUTE=false

# Performance
MEMORY_LIMIT=4GB
THREADS=8
```

## Test Suite

The `test-calls.json` includes comprehensive tests:

1. **Discovery** - List tools, tables, database info
2. **Queries** - Various SELECT patterns
3. **Custom Tools** - All domain-specific tools
4. **Security** - Verify blocked operations
5. **Resources** - Access published resources

Run tests with an MCP test client or manually send JSON-RPC requests.
