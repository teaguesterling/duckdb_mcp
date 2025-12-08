# MCP Server with Sample Data

MCP server pre-loaded with e-commerce sample data.

## What it Demonstrates

- Database initialization with schema and sample data
- Tables: customers, products, orders, order_items
- Views: customer_orders, product_sales, order_details
- Querying pre-populated data via MCP

## Schema

- **customers** - Customer records (id, name, email, created_at)
- **products** - Product catalog (id, name, category, price, stock)
- **orders** - Order headers (id, customer_id, order_date, status, total)
- **order_items** - Order line items with product references

## Files

- `init-mcp-server.sql` - Schema creation, sample data, and server start
- `mcp.json` - MCP client configuration
- `test-calls.ldjson` - Test queries against the data

## Usage

Add to your MCP client configuration:

```json
{
  "mcpServers": {
    "duckdb-ecommerce": {
      "command": "duckdb",
      "args": ["-init", "init-mcp-server.sql"]
    }
  }
}
```

## Testing

```bash
cat test-calls.ldjson | duckdb -init init-mcp-server.sql 2>/dev/null | jq .
```

## Example Queries

```sql
-- List all customers
SELECT * FROM customers ORDER BY id;

-- Products by category
SELECT category, COUNT(*) as count, AVG(price) as avg_price
FROM products GROUP BY category;

-- Customer order summary
SELECT * FROM customer_orders ORDER BY total_spent DESC;
```
