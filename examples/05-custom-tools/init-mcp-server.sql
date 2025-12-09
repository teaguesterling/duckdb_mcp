-- Custom Tools Example - Database Initialization
-- Creates schema, sample data, and custom MCP tools

-- Load the MCP extension
LOAD 'duckdb_mcp';

-- ============================================
-- Schema and Sample Data (reuse from example 03)
-- ============================================

CREATE TABLE customers (
    id INTEGER PRIMARY KEY,
    name VARCHAR NOT NULL,
    email VARCHAR UNIQUE NOT NULL,
    created_at TIMESTAMP
);

CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name VARCHAR NOT NULL,
    category VARCHAR NOT NULL,
    price DECIMAL(10, 2) NOT NULL,
    stock INTEGER DEFAULT 0
);

CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    customer_id INTEGER REFERENCES customers(id),
    order_date DATE,
    status VARCHAR DEFAULT 'pending',
    total DECIMAL(10, 2)
);

CREATE TABLE order_items (
    id INTEGER PRIMARY KEY,
    order_id INTEGER REFERENCES orders(id),
    product_id INTEGER REFERENCES products(id),
    quantity INTEGER NOT NULL,
    price DECIMAL(10, 2) NOT NULL
);

-- Sample data
INSERT INTO customers (id, name, email, created_at) VALUES
    (1, 'Alice Johnson', 'alice@example.com', '2024-01-15'),
    (2, 'Bob Smith', 'bob@example.com', '2024-02-20'),
    (3, 'Carol Williams', 'carol@example.com', '2024-03-10');

INSERT INTO products (id, name, category, price, stock) VALUES
    (1, 'Laptop Pro 15"', 'Electronics', 1299.99, 50),
    (2, 'Wireless Mouse', 'Electronics', 29.99, 200),
    (3, 'USB-C Hub', 'Electronics', 49.99, 150),
    (4, 'Desk Lamp', 'Furniture', 45.99, 120),
    (5, 'Ergonomic Chair', 'Furniture', 399.99, 30),
    (6, 'Notebook Set', 'Office', 12.99, 500);

INSERT INTO orders (id, customer_id, order_date, status, total) VALUES
    (1, 1, '2024-06-01', 'completed', 1329.98),
    (2, 2, '2024-06-15', 'shipped', 449.98),
    (3, 1, '2024-06-20', 'pending', 49.99);

INSERT INTO order_items (id, order_id, product_id, quantity, price) VALUES
    (1, 1, 1, 1, 1299.99),
    (2, 1, 2, 1, 29.99),
    (3, 2, 5, 1, 399.99),
    (4, 2, 3, 1, 49.99),
    (5, 3, 3, 1, 49.99);

-- ============================================
-- Create useful views for tools
-- ============================================

CREATE VIEW inventory_status AS
SELECT
    p.id,
    p.name,
    p.category,
    p.stock,
    CASE
        WHEN p.stock = 0 THEN 'OUT_OF_STOCK'
        WHEN p.stock < 50 THEN 'LOW_STOCK'
        WHEN p.stock < 100 THEN 'MEDIUM_STOCK'
        ELSE 'WELL_STOCKED'
    END as stock_status
FROM products p;

CREATE VIEW daily_summary AS
SELECT
    CURRENT_DATE as report_date,
    (SELECT COUNT(*) FROM customers) as total_customers,
    (SELECT COUNT(*) FROM products) as total_products,
    (SELECT COUNT(*) FROM orders WHERE status = 'pending') as pending_orders,
    (SELECT COALESCE(SUM(total), 0) FROM orders WHERE order_date = CURRENT_DATE) as today_revenue;

-- ============================================
-- Custom SQL Macros (queryable via the 'query' tool)
-- ============================================

-- These macros provide custom business logic that can be queried via:
-- tools/call with name="query" and sql="SELECT * FROM macro_name(args)"

-- Product search with optional category filter
CREATE MACRO search_products(search_term, category_filter) AS TABLE
    SELECT id, name, category, price, stock
    FROM products
    WHERE (name ILIKE '%' || search_term || '%' OR search_term IS NULL)
      AND (category = category_filter OR category_filter IS NULL)
    ORDER BY name;

-- Customer report with order summary
CREATE MACRO customer_report(cust_id) AS TABLE
    SELECT
        c.id,
        c.name,
        c.email,
        c.created_at,
        COUNT(o.id) as total_orders,
        COALESCE(SUM(o.total), 0) as total_spent,
        MAX(o.order_date) as last_order
    FROM customers c
    LEFT JOIN orders o ON c.id = o.customer_id
    WHERE c.id = cust_id
    GROUP BY c.id, c.name, c.email, c.created_at;

-- Inventory check with threshold
CREATE MACRO inventory_check(stock_threshold) AS TABLE
    SELECT
        id,
        name,
        category,
        stock,
        stock_status,
        CASE WHEN stock < stock_threshold THEN 'NEEDS_REORDER' ELSE 'OK' END as action
    FROM inventory_status
    ORDER BY stock ASC;

-- Sales summary for date range
CREATE MACRO sales_summary(start_dt, end_dt) AS TABLE
    SELECT
        o.order_date,
        COUNT(*) as order_count,
        SUM(o.total) as daily_total
    FROM orders o
    WHERE o.order_date >= start_dt AND o.order_date <= end_dt
    GROUP BY o.order_date
    ORDER BY o.order_date;

-- ============================================
-- Start MCP Server (must be LAST - it blocks for stdio)
-- ============================================

-- Note: For stdio transport, mcp_server_start blocks.
-- Custom tools via mcp_publish_tool require 'memory' transport (background mode).
-- For stdio, use macros which are queryable via the built-in 'query' tool.

SELECT mcp_server_start('stdio', '{
  "enable_query_tool": true,
  "enable_describe_tool": true,
  "enable_list_tables_tool": true,
  "enable_database_info_tool": true,
  "enable_export_tool": true,
  "enable_execute_tool": true
}');
