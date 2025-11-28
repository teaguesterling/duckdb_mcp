# Security Features Example

A DuckDB MCP server demonstrating security features including query restrictions, tool limitations, and access controls.

## What This Example Demonstrates

- Query allowlists and denylists
- Read-only mode (no DDL/DML)
- Restricted tool access
- Safe defaults for production use

## Security Features

### 1. Query Restrictions

The server can restrict which SQL operations are allowed:

- **Allowlist**: Only permit specific query patterns
- **Denylist**: Block dangerous operations

### 2. Tool Restrictions

- `execute` tool disabled entirely
- Only read operations permitted

### 3. Statement Blocking

The configuration blocks:
- `DROP` statements
- `DELETE` without WHERE
- `TRUNCATE` operations
- File system access (`COPY TO/FROM` with paths)

## Files

- `launch-mcp.sh` - Entry point
- `mcp.json` - MCP server configuration
- `init-mcp-db.sql` - Security configuration and sample data
- `start-mcp-server.sql` - Secure server startup

## Usage

```bash
./launch-mcp.sh
```

## Security Configuration

This example configures:

```sql
-- Deny dangerous operations
denied_queries: ["DROP", "TRUNCATE", "DELETE FROM.*WHERE 1=1"]

-- Disable write tools
enable_execute_tool: false
execute_allow_ddl: false
execute_allow_dml: false
```

## Test Cases

The `test-calls.json` includes both allowed and blocked operations to demonstrate security:

### Allowed Operations
- SELECT queries
- DESCRIBE tables
- List tables/database info
- Aggregations and joins

### Blocked Operations
- CREATE TABLE (DDL blocked)
- INSERT/UPDATE/DELETE (DML blocked)
- DROP statements (denied)
- File exports with paths

## Production Recommendations

1. **Always disable `execute` tool** unless explicitly needed
2. **Use query denylists** to block dangerous patterns
3. **Run DuckDB in read-only mode** when possible
4. **Limit file system access** by blocking COPY TO/FROM
5. **Use a separate database file** for MCP access (not your production data)
