# Running DuckDB as an MCP Server

This guide covers running DuckDB as an MCP server that AI assistants like Claude can connect to.

## Overview

When running as an MCP server, DuckDB:

- Exposes built-in tools for querying and exploring the database
- Allows publishing tables and queries as resources
- Supports custom SQL-based tools
- Communicates via the MCP protocol over stdio, TCP, or WebSocket

## Starting the Server

### Basic Server

```sql
LOAD 'duckdb_mcp';

-- Start with default settings (PRAGMA produces no output)
PRAGMA mcp_server_start('stdio');
```

The server blocks and waits for requests on stdin, responding on stdout.

### Server with Configuration

```sql
PRAGMA mcp_server_start('stdio', '{
    "enable_execute_tool": false,
    "default_result_format": "markdown"
}');
```

### Transport Options

| Transport | Command | Use Case |
|-----------|---------|----------|
| stdio | `PRAGMA mcp_server_start('stdio')` | CLI tools, Claude Desktop |
| memory | `PRAGMA mcp_server_start('memory')` | Testing |
| http | `PRAGMA mcp_server_start('http', 'host', port, '{}')` | REST clients |
| https | `PRAGMA mcp_server_start('https', 'host', port, '{}')` | Secure web apps |

!!! tip "PRAGMA vs SELECT"
    All server lifecycle and publishing functions support both `PRAGMA` (no output) and `SELECT` (returns status) syntax. Use `PRAGMA` in init scripts for clean output; use `SELECT` when you need the return value.

## Built-in Tools

The server automatically exposes these tools:

### query

Execute read-only SQL queries:

```json
{
    "name": "query",
    "arguments": {
        "sql": "SELECT * FROM products WHERE price < 50",
        "format": "markdown"
    }
}
```

### describe

Get schema information:

```json
{
    "name": "describe",
    "arguments": {"table": "products"}
}
```

Or describe a query result:

```json
{
    "name": "describe",
    "arguments": {"query": "SELECT id, name FROM products"}
}
```

### list_tables

List all tables and views:

```json
{
    "name": "list_tables",
    "arguments": {"include_views": true}
}
```

### database_info

Get database overview:

```json
{
    "name": "database_info",
    "arguments": {}
}
```

### export

Export data to files:

```json
{
    "name": "export",
    "arguments": {
        "query": "SELECT * FROM products",
        "output": "/tmp/products.parquet",
        "format": "parquet"
    }
}
```

### execute (disabled by default)

Run DDL/DML statements:

```json
{
    "name": "execute",
    "arguments": {"sql": "INSERT INTO products VALUES (100, 'New Product', 29.99)"}
}
```

!!! warning
    Enable `execute` only when you trust the clients. It allows database modifications.

## Publishing Resources

### Publishing Tables

Expose tables for clients to read:

```sql
-- Simple form (auto-generates URI, defaults to json)
PRAGMA mcp_publish_table('products');

-- Full form with explicit URI and format
PRAGMA mcp_publish_table('products', 'data://tables/products', 'json');
PRAGMA mcp_publish_table('orders', 'data://tables/orders', 'csv');
PRAGMA mcp_publish_table('users', 'data://tables/users', 'markdown');
```

Clients can then read these resources:

```json
{
    "method": "resources/read",
    "params": {"uri": "data://tables/products"}
}
```

### Publishing Queries

Publish query results with optional refresh:

```sql
-- Simple form (just query and URI)
PRAGMA mcp_publish_query(
    'SELECT category, COUNT(*) as count FROM products GROUP BY category',
    'data://reports/product_counts'
);

-- Full form with format and auto-refresh (every hour)
PRAGMA mcp_publish_query(
    'SELECT * FROM recent_orders WHERE created_at > NOW() - INTERVAL 24 HOURS',
    'data://reports/daily_orders',
    'json',
    3600
);
```

## Publishing Custom Tools

Create domain-specific tools that wrap SQL queries:

### Basic Custom Tool

```sql
PRAGMA mcp_publish_tool(
    'get_product',
    'Get a product by ID',
    'SELECT * FROM products WHERE id = $id',
    '{"id": {"type": "integer", "description": "Product ID"}}',
    '["id"]'
);
```

Clients call it with:

```json
{
    "name": "get_product",
    "arguments": {"id": 42}
}
```

### Tool with Multiple Parameters

```sql
PRAGMA mcp_publish_tool(
    'search_orders',
    'Search orders by customer and date range',
    'SELECT * FROM orders
     WHERE customer_id = $customer_id
       AND order_date BETWEEN $start_date AND $end_date
     ORDER BY order_date DESC',
    '{
        "customer_id": {"type": "integer", "description": "Customer ID"},
        "start_date": {"type": "string", "description": "Start date (YYYY-MM-DD)"},
        "end_date": {"type": "string", "description": "End date (YYYY-MM-DD)"}
    }',
    '["customer_id", "start_date", "end_date"]'
);
```

### Tool with Optional Parameters

```sql
PRAGMA mcp_publish_tool(
    'list_products',
    'List products with optional filters',
    'SELECT * FROM products
     WHERE ($category IS NULL OR category = $category)
       AND ($max_price IS NULL OR price <= $max_price)
     LIMIT COALESCE($limit, 100)',
    '{
        "category": {"type": "string", "description": "Filter by category"},
        "max_price": {"type": "number", "description": "Maximum price"},
        "limit": {"type": "integer", "description": "Max results (default 100)"}
    }',
    '[]'
);
```

### Tool with Custom Output Format

```sql
PRAGMA mcp_publish_tool(
    'inventory_report',
    'Generate inventory summary',
    'SELECT
        category,
        COUNT(*) as items,
        SUM(quantity) as total_stock,
        AVG(price) as avg_price
     FROM inventory
     GROUP BY category
     ORDER BY total_stock DESC',
    '{}',
    '[]',
    'markdown'  -- Output as markdown table
);
```

## Init Script Pattern

Create an init script for your server. Use `PRAGMA` syntax for side-effectful operations — no output clutter:

```sql
-- init-server.sql

-- Load extension
LOAD 'duckdb_mcp';

-- Create schema
CREATE TABLE IF NOT EXISTS products (
    id INTEGER PRIMARY KEY,
    name VARCHAR NOT NULL,
    price DECIMAL(10,2),
    category VARCHAR
);

CREATE TABLE IF NOT EXISTS orders (
    id INTEGER PRIMARY KEY,
    product_id INTEGER REFERENCES products(id),
    quantity INTEGER,
    order_date DATE
);

-- Load sample data
INSERT INTO products VALUES
    (1, 'Widget', 9.99, 'Tools'),
    (2, 'Gadget', 24.99, 'Electronics'),
    (3, 'Gizmo', 14.99, 'Electronics')
ON CONFLICT DO NOTHING;

-- Publish custom tools (queued until server starts)
PRAGMA mcp_publish_tool(
    'products_by_category',
    'Get products in a category',
    'SELECT * FROM products WHERE category = $category',
    '{"category": {"type": "string"}}',
    '["category"]',
    'markdown'
);

PRAGMA mcp_publish_table('products');

-- Start server
PRAGMA mcp_server_start('stdio', '{
    "enable_execute_tool": false,
    "default_result_format": "markdown"
}');
```

Run with:

```bash
duckdb -init init-server.sql
```

## Integrating with Claude Desktop

Add to your Claude Desktop configuration:

```json
{
  "mcpServers": {
    "my-database": {
      "command": "duckdb",
      "args": ["-init", "/path/to/init-server.sql"]
    }
  }
}
```

Now Claude can:

- Query your database directly
- Use your custom tools
- Read published resources

## Monitoring

Check server status:

```sql
SELECT mcp_server_status();
```

Returns:

```json
{
    "running": true,
    "transport": "stdio",
    "requests_received": 42,
    "responses_sent": 42,
    "errors_returned": 0
}
```

## Best Practices

1. **Disable `execute` tool** unless absolutely necessary

2. **Use markdown format** for AI assistants - it's more token-efficient

3. **Create domain-specific tools** rather than exposing raw SQL access

4. **Use descriptive tool names and descriptions** - AI assistants use these to decide which tool to call

5. **Validate inputs in SQL** when possible:

   ```sql
   PRAGMA mcp_publish_tool(
       'safe_lookup',
       'Lookup by ID (validated)',
       'SELECT * FROM items WHERE id = CAST($id AS INTEGER) LIMIT 1',
       '{"id": {"type": "string"}}',
       '["id"]'
   );
   ```

6. **Test with memory transport** before deploying:

   ```sql
   PRAGMA mcp_server_start('memory');
   SELECT mcp_server_send_request('{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}');
   ```
