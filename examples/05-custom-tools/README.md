# Custom Tools and Resources Example

A DuckDB MCP server demonstrating how to create custom SQL-based tools and publish database resources.

## What This Example Demonstrates

- Creating custom tools from SQL templates
- Publishing tables as MCP resources
- Publishing query results as dynamic resources
- Domain-specific tool creation

## Custom Tools

This example creates several custom tools:

### 1. `search_products`
Search products by name or category with fuzzy matching.

```json
{
  "name": "search_products",
  "arguments": {
    "search_term": "laptop",
    "category": "Electronics"
  }
}
```

### 2. `customer_report`
Generate a customer activity report.

```json
{
  "name": "customer_report",
  "arguments": {
    "customer_id": 1
  }
}
```

### 3. `inventory_check`
Check inventory levels with low-stock alerts.

```json
{
  "name": "inventory_check",
  "arguments": {
    "threshold": 50
  }
}
```

### 4. `sales_summary`
Get sales summary for a date range.

```json
{
  "name": "sales_summary",
  "arguments": {
    "start_date": "2024-06-01",
    "end_date": "2024-06-30"
  }
}
```

## Published Resources

Resources are published at URIs that MCP clients can read:

- `db://tables/products` - Product catalog
- `db://tables/customers` - Customer list
- `db://views/inventory_status` - Current inventory levels
- `db://reports/daily_summary` - Auto-refreshing daily summary

## Files

- `launch-mcp.sh` - Entry point
- `mcp.json` - MCP server configuration
- `init-mcp-db.sql` - Schema, data, and custom tool definitions
- `start-mcp-server.sql` - Server startup with resource publishing

## Usage

```bash
./launch-mcp.sh
```

## Creating Your Own Custom Tools

Custom tools are SQL templates with parameter substitution:

```sql
SELECT mcp_register_sql_tool(
    'my_tool',                              -- Tool name
    'Description of what it does',          -- Description
    'SELECT * FROM table WHERE col = $param', -- SQL template
    '{"param": "string"}'                   -- Parameter schema
);
```

Parameters use `$name` syntax and are substituted safely.

## Publishing Resources

Publish tables or queries as MCP resources:

```sql
-- Publish a table
SELECT mcp_publish_table('my_table', 'db://tables/my_table', 'json');

-- Publish a query with auto-refresh (every 60 seconds)
SELECT mcp_publish_query(
    'SELECT * FROM stats',
    'db://stats/current',
    'json',
    60
);
```
