# Custom Tools Example

MCP server with domain-specific tools using DuckDB macros.

## What it Demonstrates

- DuckDB table macros as queryable functions
- Custom business logic exposed through SQL
- Views for pre-computed analytics
- All tools enabled including execute

## Custom Macros

- **search_products(term, category)** - Product search with filters
- **customer_report(customer_id)** - Customer summary with orders
- **inventory_check(threshold)** - Stock level analysis
- **sales_summary(start_date, end_date)** - Date range sales report

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

## Example Queries

```sql
-- Search for laptop products
SELECT * FROM search_products('laptop', NULL);

-- Check inventory below threshold
SELECT * FROM inventory_check(100);

-- Customer report
SELECT * FROM customer_report(1);
```

## Testing

```bash
cat test-calls.ldjson | duckdb -init init-mcp-server.sql 2>/dev/null | jq .
```
