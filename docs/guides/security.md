# Security Guide

This guide covers security best practices for the DuckDB MCP extension in both client and server modes.

## Overview

The MCP extension has different security considerations depending on whether DuckDB is acting as:

- **MCP Client**: Connecting to external MCP servers
- **MCP Server**: Exposing your database to external clients

## Server Security

When running DuckDB as an MCP server, you're exposing database access to external clients.

### Disable the Execute Tool

The `execute` tool allows DDL/DML operations (CREATE, INSERT, UPDATE, DELETE). It's **disabled by default** for good reason.

```sql
-- Default: execute is disabled
SELECT mcp_server_start('stdio', 'localhost', 0, '{}');

-- Only enable if you trust your clients
SELECT mcp_server_start('stdio', 'localhost', 0, '{
    "enable_execute_tool": true
}');
```

!!! danger
    Only enable `execute` when:

    - You fully trust the MCP client
    - You need write operations
    - You've considered the implications

### Use Read-Only Databases

For purely analytical use cases, open the database in read-only mode:

```bash
duckdb -readonly mydata.duckdb -init init-server.sql
```

Or in the init script:

```sql
ATTACH 'mydata.duckdb' AS main (READ_ONLY);
```

### Limit Exposed Tools

Only enable the tools you need:

```sql
SELECT mcp_server_start('stdio', 'localhost', 0, '{
    "enable_query_tool": true,
    "enable_describe_tool": true,
    "enable_list_tables_tool": true,
    "enable_database_info_tool": false,
    "enable_export_tool": false,
    "enable_execute_tool": false
}');
```

### Create Custom Read-Only Tools

Instead of exposing the general `query` tool, create specific tools:

```sql
-- Disable general query
SELECT mcp_server_start('stdio', 'localhost', 0, '{
    "enable_query_tool": false
}');

-- Publish specific, safe tools
SELECT mcp_publish_tool(
    'get_public_stats',
    'Get aggregated public statistics',
    'SELECT category, COUNT(*) as count, AVG(price) as avg_price
     FROM products
     GROUP BY category',
    '{}',
    '[]'
);
```

### Validate and Sanitize Inputs

While parameters are safely substituted (not concatenated), add SQL-level validation:

```sql
SELECT mcp_publish_tool(
    'safe_lookup',
    'Lookup item by numeric ID only',
    'SELECT * FROM items WHERE id = CAST($id AS INTEGER) LIMIT 1',
    '{"id": {"type": "string"}}',
    '["id"]'
);
```

### Limit Result Sizes

Prevent large data exfiltration:

```sql
SELECT mcp_publish_tool(
    'list_items',
    'List items (max 100)',
    'SELECT * FROM items LIMIT LEAST($limit, 100)',
    '{"limit": {"type": "integer"}}',
    '[]'
);
```

## Client Security

When DuckDB acts as an MCP client, connecting to external servers.

### Development Mode (Permissive)

By default, the extension runs in permissive mode - any command can be executed:

```sql
-- Permissive: runs any command
ATTACH 'python3' AS server (TYPE mcp, ARGS '["server.py"]');
```

This is convenient for development but risky in production.

### Production Mode (Strict)

Enable strict mode by configuring an allowlist:

```sql
-- Enable strict validation
SET allowed_mcp_commands = '/usr/bin/python3:/opt/venv/bin/python';

-- Now only allowed commands work
ATTACH 'python3' AS server (TYPE mcp, ARGS '["server.py"]');
-- Error: python3 is not in allowed_mcp_commands

ATTACH '/usr/bin/python3' AS server (TYPE mcp, ARGS '["server.py"]');
-- Success: exact path is allowed
```

### URL Allowlists

Restrict which URLs can be accessed:

```sql
-- Only allow specific domains
SET allowed_mcp_urls = 'https://api.company.com https://data.company.com';
```

### Lock Configuration

Prevent runtime changes to server configuration:

```sql
-- Configure servers
SET mcp_server_file = '/etc/mcp/servers.json';

-- Lock - no more changes allowed
SET mcp_lock_servers = true;
```

### Client-Only Mode

If you only need client functionality, disable serving entirely:

```sql
SET mcp_disable_serving = true;

-- Now mcp_server_start() will fail
SELECT mcp_server_start('stdio', 'localhost', 0, '{}');
-- Error: MCP serving is disabled
```

## Configuration Security

### Secure Init Scripts

Don't hardcode secrets in init scripts. Use environment variables:

```sql
-- init-server.sql
LOAD 'duckdb_mcp';

-- Get API key from environment
CREATE OR REPLACE MACRO get_env(name) AS (
    SELECT value FROM duckdb_settings() WHERE name = 'env_' || name
);

-- Use in queries
SELECT mcp_publish_tool(
    'call_api',
    'Call external API',
    'SELECT http_get(''https://api.example.com?key='' || get_env(''API_KEY''))',
    '{}',
    '[]'
);
```

Run with:

```bash
API_KEY=secret123 duckdb -init init-server.sql
```

### Secure Configuration Files

Protect your MCP configuration files:

```bash
# Restrict permissions
chmod 600 /etc/mcp/servers.json
chown duckdb:duckdb /etc/mcp/servers.json
```

### Use Separate Database Files

For sensitive data, use a separate read-only database:

```sql
-- Main database for writes
CREATE TABLE audit_log (...);

-- Read-only copy for MCP access
COPY (SELECT * FROM sensitive_data) TO 'public_data.parquet';
```

## Network Security

### Prefer stdio Transport

The `stdio` transport is most secure for local use:

- No network exposure
- Communication through process pipes
- Easy to run in sandboxed environments

### TCP/WebSocket Considerations

If using TCP or WebSocket:

```sql
-- Bind to localhost only
SELECT mcp_server_start('tcp', '127.0.0.1', 8080, '{}');

-- NOT: '0.0.0.0' which exposes to all interfaces
```

Use a reverse proxy (nginx, traefik) for:

- TLS termination
- Authentication
- Rate limiting
- Access logging

### Containerization

Run MCP servers in containers for isolation:

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y duckdb

COPY init-server.sql /app/
COPY data.duckdb /app/

USER nobody
WORKDIR /app

CMD ["duckdb", "-init", "init-server.sql"]
```

## Audit and Monitoring

### Server Statistics

Monitor server activity:

```sql
SELECT mcp_server_status();
```

Track:

- `requests_received`: Total requests
- `responses_sent`: Total responses
- `errors_returned`: Error count

### Logging

Enable DuckDB logging for audit trails:

```sql
SET enable_logging = true;
SET logging_level = 'info';
```

### Alerting on Errors

High error rates may indicate:

- Attempted SQL injection
- Malformed requests
- Client bugs

## Security Checklist

### For MCP Servers

- [ ] Disable `execute` tool unless required
- [ ] Use read-only database when possible
- [ ] Create specific tools rather than exposing `query`
- [ ] Limit result sizes in all tools
- [ ] Validate inputs at SQL level
- [ ] Bind to localhost, not `0.0.0.0`
- [ ] Use containers for isolation
- [ ] Monitor server statistics
- [ ] Keep DuckDB and extension updated

### For MCP Clients

- [ ] Use `allowed_mcp_commands` in production
- [ ] Use `allowed_mcp_urls` to restrict access
- [ ] Lock server configuration after setup
- [ ] Don't hardcode secrets in SQL
- [ ] Review server code before trusting
- [ ] Use `mcp_disable_serving` if not needed

## Reporting Security Issues

If you discover a security vulnerability:

1. **Do not** open a public GitHub issue
2. Email security details to the maintainers
3. Allow time for a fix before disclosure

See the project's SECURITY.md for contact information.
