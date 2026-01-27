# Server Functions Reference

These functions are used when DuckDB runs as an MCP server, exposing your database to AI assistants and other MCP clients.

## Server Lifecycle

### mcp_server_start

Start the MCP server.

```sql
mcp_server_start(transport, host, port, config) → VARCHAR
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `transport` | VARCHAR | Transport type: `stdio`, `memory`, `tcp`, `websocket` |
| `host` | VARCHAR | Host to bind (used for tcp/websocket) |
| `port` | INTEGER | Port number (used for tcp/websocket, 0 for stdio) |
| `config` | VARCHAR | JSON configuration object (see [Configuration](configuration.md)) |

**Transport Types:**

| Transport | Description | Use Case |
|-----------|-------------|----------|
| `stdio` | Standard input/output | CLI integration, Claude Desktop |
| `memory` | In-process (no I/O) | Testing, unit tests |
| `http` | HTTP server | REST clients, web applications |
| `https` | HTTPS server (SSL/TLS) | Secure web applications |

**Examples:**

```sql
-- Stdio for Claude Desktop integration
SELECT mcp_server_start('stdio', 'localhost', 0, '{}');

-- Memory transport for testing
SELECT mcp_server_start('memory', 'localhost', 0, '{}');

-- HTTP server on port 8080
SELECT mcp_server_start('http', 'localhost', 8080, '{}');

-- HTTP with authentication
SELECT mcp_server_start('http', 'localhost', 8080, '{
    "auth_token": "your-secret-token"
}');

-- HTTPS with SSL certificates
SELECT mcp_server_start('https', 'localhost', 8443, '{
    "auth_token": "your-secret-token",
    "ssl_cert_path": "/path/to/cert.pem",
    "ssl_key_path": "/path/to/key.pem"
}');

-- With tool configuration
SELECT mcp_server_start('stdio', 'localhost', 0, '{
    "enable_execute_tool": false,
    "default_result_format": "markdown"
}');
```

---

### mcp_server_stop

Stop the running MCP server.

```sql
mcp_server_stop() → VARCHAR
```

**Returns:** Status message confirming shutdown.

---

### mcp_server_status

Get the current server status and statistics.

```sql
mcp_server_status() → VARCHAR (JSON)
```

**Returns:** JSON object containing:

| Field | Type | Description |
|-------|------|-------------|
| `running` | boolean | Whether the server is active |
| `transport` | string | Active transport type |
| `requests_received` | integer | Total requests processed |
| `responses_sent` | integer | Total responses sent |
| `errors_returned` | integer | Total error responses |

**Example:**

```sql
SELECT mcp_server_status();
-- {"running": true, "transport": "stdio", "requests_received": 42, "responses_sent": 42, "errors_returned": 2}
```

---

## Publishing Resources

### mcp_publish_table

Publish a table as an MCP resource.

```sql
mcp_publish_table(table_name, uri, format) → VARCHAR
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `table_name` | VARCHAR | Name of the table to publish |
| `uri` | VARCHAR | Resource URI (e.g., `data://tables/products`) |
| `format` | VARCHAR | Output format: `json`, `csv`, `markdown` |

**Example:**

```sql
SELECT mcp_publish_table('products', 'data://tables/products', 'json');
SELECT mcp_publish_table('users', 'data://tables/users', 'csv');
```

!!! note
    Published tables are static snapshots. The resource content is captured when published and won't reflect subsequent table changes unless republished.

!!! tip "Publish Before Server Starts"
    You can call `mcp_publish_table` before starting the server. The registration will be queued and automatically applied when the server starts. This is useful for initialization scripts.

---

### mcp_publish_query

Publish a query result as an MCP resource.

```sql
mcp_publish_query(sql, uri, format, refresh_interval) → VARCHAR
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `sql` | VARCHAR | SQL query to execute |
| `uri` | VARCHAR | Resource URI |
| `format` | VARCHAR | Output format: `json`, `csv`, `markdown` |
| `refresh_interval` | INTEGER | Refresh interval in seconds (minimum 60) |

**Example:**

```sql
-- Publish a summary that refreshes every hour
SELECT mcp_publish_query(
    'SELECT category, COUNT(*) as count, AVG(price) as avg_price FROM products GROUP BY category',
    'data://reports/product_summary',
    'json',
    3600
);
```

!!! tip "Publish Before Server Starts"
    You can call `mcp_publish_query` before starting the server. The registration will be queued and automatically applied when the server starts.

---

### mcp_publish_resource

Publish static content as an MCP resource. This is useful for configuration files, documentation, or any static data.

```sql
mcp_publish_resource(uri, content, mime_type, description) → VARCHAR
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `uri` | VARCHAR | Resource URI (e.g., `config://app-settings`) |
| `content` | VARCHAR | The static content to publish |
| `mime_type` | VARCHAR | MIME type (default: `text/plain`) |
| `description` | VARCHAR | Human-readable description (optional) |

**Examples:**

```sql
-- Publish JSON configuration
SELECT mcp_publish_resource(
    'config://app-settings',
    '{"theme": "dark", "language": "en"}',
    'application/json',
    'Application settings'
);

-- Publish plain text documentation
SELECT mcp_publish_resource(
    'docs://readme',
    'Welcome to the API. Use the query tool to explore data.',
    'text/plain',
    'API documentation'
);

-- Publish with defaults (text/plain, no description)
SELECT mcp_publish_resource(
    'info://version',
    'v1.3.2',
    NULL,
    NULL
);
```

!!! tip "Publish Before Server Starts"
    You can call `mcp_publish_resource` before starting the server. The registration will be queued and automatically applied when the server starts.

---

## Publishing Tools

### mcp_publish_tool

Publish a custom SQL-based tool that MCP clients can execute.

**5-parameter version:**

```sql
mcp_publish_tool(name, description, sql_template, properties, required) → VARCHAR
```

**6-parameter version (with format):**

```sql
mcp_publish_tool(name, description, sql_template, properties, required, format) → VARCHAR
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | VARCHAR | Tool name (used by clients) |
| `description` | VARCHAR | Human-readable description |
| `sql_template` | VARCHAR | SQL with `$param` placeholders |
| `properties` | VARCHAR | JSON Schema for parameters |
| `required` | VARCHAR | JSON array of required parameter names |
| `format` | VARCHAR | Output format: `json`, `csv`, `markdown` (default: `json`) |

**Parameter Substitution:**

Parameters in the SQL template use `$name` syntax:

```sql
'SELECT * FROM products WHERE category = $category AND price < $max_price'
```

**Examples:**

```sql
-- Simple search tool
SELECT mcp_publish_tool(
    'search_products',
    'Search products by name',
    'SELECT * FROM products WHERE name ILIKE ''%'' || $query || ''%''',
    '{"query": {"type": "string", "description": "Search term"}}',
    '["query"]'
);

-- Tool with multiple parameters
SELECT mcp_publish_tool(
    'sales_by_region',
    'Get sales totals by region for a date range',
    'SELECT region, SUM(amount) as total
     FROM sales
     WHERE sale_date BETWEEN $start_date AND $end_date
     GROUP BY region
     ORDER BY total DESC',
    '{
        "start_date": {"type": "string", "description": "Start date (YYYY-MM-DD)"},
        "end_date": {"type": "string", "description": "End date (YYYY-MM-DD)"}
    }',
    '["start_date", "end_date"]'
);

-- Tool with markdown output for AI assistants
SELECT mcp_publish_tool(
    'inventory_report',
    'Generate inventory status report',
    'SELECT category, COUNT(*) as items, SUM(quantity) as total_qty
     FROM inventory
     GROUP BY category',
    '{}',
    '[]',
    'markdown'
);
```

!!! tip "Publish Before Server Starts"
    You can call `mcp_publish_tool` before starting the server. The registration will be queued and automatically applied when the server starts. This is useful for setting up tools in initialization scripts.

---

## Built-in Server Tools

When running as an MCP server, these tools are automatically available to clients:

### query

Execute read-only SQL queries.

**Arguments:**

| Argument | Type | Required | Description |
|----------|------|----------|-------------|
| `sql` | string | Yes | SQL SELECT statement |
| `format` | string | No | Output format: `json` (default), `markdown`, `csv` |

**Example request:**

```json
{
    "name": "query",
    "arguments": {
        "sql": "SELECT * FROM products LIMIT 10",
        "format": "markdown"
    }
}
```

---

### describe

Get schema information for a table or query.

**Arguments:**

| Argument | Type | Required | Description |
|----------|------|----------|-------------|
| `table` | string | No | Table name to describe |
| `query` | string | No | Query to analyze (returns result schema) |

Provide either `table` or `query`, not both.

---

### list_tables

List all tables and views in the database.

**Arguments:**

| Argument | Type | Required | Description |
|----------|------|----------|-------------|
| `schema` | string | No | Filter by schema name |
| `include_views` | boolean | No | Include views (default: true) |

---

### database_info

Get comprehensive database information including schemas, tables, and extensions.

**Arguments:** None.

**Returns:** JSON with `databases`, `schemas`, `tables`, `views`, and `extensions`.

---

### export

Export query results to a file.

**Arguments:**

| Argument | Type | Required | Description |
|----------|------|----------|-------------|
| `query` | string | Yes | SQL query to export |
| `output` | string | No | Output file path |
| `format` | string | No | Format: `json`, `csv`, `parquet` (parquet requires output path) |

!!! warning
    When `output` is not specified, results are returned inline. Inline export only supports `json`, `csv`, and `markdown` formats. Use a file path for `parquet` exports.

---

### execute

Execute DDL/DML statements (CREATE, INSERT, UPDATE, DELETE, etc.).

**Arguments:**

| Argument | Type | Required | Description |
|----------|------|----------|-------------|
| `sql` | string | Yes | SQL statement to execute |

!!! danger "Security Warning"
    The `execute` tool is **disabled by default** because it allows modifying the database. Only enable it when you trust the MCP clients connecting to your server.

    ```sql
    SELECT mcp_server_start('stdio', 'localhost', 0, '{"enable_execute_tool": true}');
    ```

---

## Memory Transport (Testing)

The `memory` transport is designed for testing MCP server functionality without external processes.

### mcp_server_send_request

Send a JSON-RPC request directly to the memory transport server.

```sql
mcp_server_send_request(request_json) → VARCHAR (JSON)
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `request_json` | VARCHAR | Complete JSON-RPC 2.0 request |

**Example:**

```sql
-- Start memory server
SELECT mcp_server_start('memory', 'localhost', 0, '{}');

-- Send initialize request
SELECT mcp_server_send_request('{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "initialize",
    "params": {
        "protocolVersion": "2024-11-05",
        "clientInfo": {"name": "test", "version": "1.0"},
        "capabilities": {}
    }
}');

-- List tools
SELECT mcp_server_send_request('{
    "jsonrpc": "2.0",
    "id": 2,
    "method": "tools/list",
    "params": {}
}');

-- Execute a query
SELECT mcp_server_send_request('{
    "jsonrpc": "2.0",
    "id": 3,
    "method": "tools/call",
    "params": {
        "name": "query",
        "arguments": {"sql": "SELECT 1 + 1 as result"}
    }
}');
```

This is invaluable for writing automated tests that verify MCP protocol compliance.

---

## HTTP Transport

The `http` and `https` transports provide an HTTP server for MCP clients, enabling browser-based applications and standard HTTP tools to interact with DuckDB.

### Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/health` | GET | Health check, returns `{"status":"ok"}` |
| `/` | POST | MCP JSON-RPC endpoint |
| `/mcp` | POST | Alternative MCP JSON-RPC endpoint |

### Authentication

When `auth_token` is configured, all MCP endpoints require Bearer token authentication:

```bash
curl -X POST http://localhost:8080/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer your-secret-token" \
    -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'
```

**Response codes:**

| Status | Condition |
|--------|-----------|
| 200 OK | Request successful |
| 401 Unauthorized | No `Authorization` header provided |
| 403 Forbidden | Invalid token provided |

### CORS

CORS is enabled by default, allowing browser-based clients to connect from any origin.

### Example: Using curl

```bash
# Start server
duckdb -c "LOAD 'duckdb_mcp'; SELECT mcp_server_start('http', 'localhost', 8080, '{}');"

# Health check
curl http://localhost:8080/health

# Initialize
curl -X POST http://localhost:8080/mcp \
    -H "Content-Type: application/json" \
    -d '{
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "protocolVersion": "2024-11-05",
            "clientInfo": {"name": "curl", "version": "1.0"},
            "capabilities": {}
        }
    }'

# List tools
curl -X POST http://localhost:8080/mcp \
    -H "Content-Type: application/json" \
    -d '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}'

# Execute query
curl -X POST http://localhost:8080/mcp \
    -H "Content-Type: application/json" \
    -d '{
        "jsonrpc": "2.0",
        "id": 3,
        "method": "tools/call",
        "params": {
            "name": "query",
            "arguments": {"sql": "SELECT 1 + 1 as result"}
        }
    }'
```

### HTTPS Configuration

For production use, enable HTTPS with SSL certificates:

```sql
SELECT mcp_server_start('https', '0.0.0.0', 8443, '{
    "auth_token": "secure-token-here",
    "ssl_cert_path": "/etc/ssl/certs/server.crt",
    "ssl_key_path": "/etc/ssl/private/server.key"
}');
```

!!! warning "Security Recommendations"
    - Always use HTTPS in production
    - Use strong, randomly-generated auth tokens
    - Bind to specific interfaces rather than `0.0.0.0` when possible
    - Consider firewall rules to restrict access
