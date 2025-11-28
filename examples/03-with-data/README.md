# MCP Server with Schema and Sample Data

A DuckDB MCP server pre-loaded with a sample e-commerce schema and data for demonstration and testing.

## What This Example Demonstrates

- Pre-defined database schema
- Sample data loading
- Realistic table relationships
- Query examples across multiple tables

## Schema Overview

```
customers (id, name, email, created_at)
    |
    +--< orders (id, customer_id, order_date, status, total)
            |
            +--< order_items (id, order_id, product_id, quantity, price)
                    |
products (id, name, category, price, stock) >--+
```

## Files

- `launch-mcp.sh` - Entry point
- `mcp.json` - MCP server configuration
- `init-mcp-db.sql` - Creates schema and loads sample data
- `start-mcp-server.sql` - Starts the MCP server

## Usage

```bash
# Start the server
./launch-mcp.sh

# With development build
DUCKDB=../../build/release/duckdb ./launch-mcp.sh
```

## Sample Queries

Once connected, try these queries:

```sql
-- List all customers
SELECT * FROM customers;

-- Orders with customer names
SELECT o.id, c.name, o.order_date, o.total
FROM orders o
JOIN customers c ON o.customer_id = c.id;

-- Products by category with stock levels
SELECT category, COUNT(*) as products, SUM(stock) as total_stock
FROM products
GROUP BY category;

-- Top customers by order value
SELECT c.name, COUNT(o.id) as orders, SUM(o.total) as total_spent
FROM customers c
JOIN orders o ON c.id = o.customer_id
GROUP BY c.id, c.name
ORDER BY total_spent DESC;
```

## Test Calls

See `test-calls.json` for example MCP tool calls demonstrating the sample data.
