# Custom Tools Example

MCP server with domain-specific tools using DuckDB macros, queryable via the built-in `query` tool.

## What it Demonstrates

- **Custom SQL Macros**: Business logic as table-returning macros
- **Views**: Pre-computed analytics available as queryable views
- **All Built-in Tools**: Including execute for DDL/DML
- **Query Tool Integration**: Macros called via `SELECT * FROM macro_name(args)`

## Custom Macros

These macros are called via the `query` tool with SQL:

| Macro | Description | Example Call |
|-------|-------------|--------------|
| `search_products(term, category)` | Product search with filters | `SELECT * FROM search_products('laptop', NULL)` |
| `customer_report(id)` | Customer with order summary | `SELECT * FROM customer_report(1)` |
| `inventory_check(threshold)` | Stock level analysis | `SELECT * FROM inventory_check(100)` |
| `sales_summary(start, end)` | Date range sales report | `SELECT * FROM sales_summary('2024-06-01', '2024-06-30')` |

## Views

- **inventory_status** - Product stock levels with status labels
- **daily_summary** - Dashboard metrics

## Files

- `init-mcp-server.sql` - Schema, data, macros, views, and server start
- `mcp.json` - MCP client configuration
- `test-calls.ldjson` - Tests for custom tools

## Usage

Add to your MCP client configuration:

```json
{
  "mcpServers": {
    "duckdb-custom-tools": {
      "command": "duckdb",
      "args": ["-init", "init-mcp-server.sql"]
    }
  }
}
```

## Example Tool Calls

```json
// Search for laptop products
{"method": "tools/call", "params": {"name": "query", "arguments": {"sql": "SELECT * FROM search_products('laptop', NULL)"}}}

// Check inventory below 100 units
{"method": "tools/call", "params": {"name": "query", "arguments": {"sql": "SELECT * FROM inventory_check(100)"}}}

// Customer report
{"method": "tools/call", "params": {"name": "query", "arguments": {"sql": "SELECT * FROM customer_report(1)"}}}

// Sales summary with markdown output
{"method": "tools/call", "params": {"name": "query", "arguments": {"sql": "SELECT * FROM sales_summary('2024-06-01', '2024-06-30')", "format": "markdown"}}}
```

## Using mcp_publish_tool (Programmatic)

For programmatic use with the `memory` transport (background mode), you can register custom tools directly:

```sql
-- Start server in memory/background mode
SELECT mcp_server_start('memory');

-- Register custom tool
SELECT mcp_publish_tool(
    'get_product',
    'Get a product by ID',
    'SELECT * FROM products WHERE id = $id',
    '{"id": "integer"}',
    '["id"]'
);
```

**Note**: For stdio transport, `mcp_server_start` blocks, so `mcp_publish_tool` must be called before starting or use the macro approach shown above.

## Testing

```bash
cat test-calls.ldjson | duckdb -init init-mcp-server.sql 2>/dev/null | jq .
```
