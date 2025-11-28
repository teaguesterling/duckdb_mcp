-- DuckDB MCP Server with Sample E-commerce Data
-- Creates schema and loads sample data

-- Load the MCP extension
LOAD 'duckdb_mcp';

-- ============================================
-- Schema Definition
-- ============================================

-- Customers table
CREATE TABLE customers (
    id INTEGER PRIMARY KEY,
    name VARCHAR NOT NULL,
    email VARCHAR UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Products table
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name VARCHAR NOT NULL,
    category VARCHAR NOT NULL,
    price DECIMAL(10, 2) NOT NULL,
    stock INTEGER DEFAULT 0
);

-- Orders table
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    customer_id INTEGER REFERENCES customers(id),
    order_date DATE DEFAULT CURRENT_DATE,
    status VARCHAR DEFAULT 'pending',
    total DECIMAL(10, 2)
);

-- Order items table
CREATE TABLE order_items (
    id INTEGER PRIMARY KEY,
    order_id INTEGER REFERENCES orders(id),
    product_id INTEGER REFERENCES products(id),
    quantity INTEGER NOT NULL,
    price DECIMAL(10, 2) NOT NULL
);

-- ============================================
-- Sample Data
-- ============================================

-- Insert customers
INSERT INTO customers (id, name, email, created_at) VALUES
    (1, 'Alice Johnson', 'alice@example.com', '2024-01-15 10:30:00'),
    (2, 'Bob Smith', 'bob@example.com', '2024-02-20 14:45:00'),
    (3, 'Carol Williams', 'carol@example.com', '2024-03-10 09:15:00'),
    (4, 'David Brown', 'david@example.com', '2024-04-05 16:00:00'),
    (5, 'Eve Davis', 'eve@example.com', '2024-05-12 11:20:00');

-- Insert products
INSERT INTO products (id, name, category, price, stock) VALUES
    (1, 'Laptop Pro 15"', 'Electronics', 1299.99, 50),
    (2, 'Wireless Mouse', 'Electronics', 29.99, 200),
    (3, 'USB-C Hub', 'Electronics', 49.99, 150),
    (4, 'Mechanical Keyboard', 'Electronics', 149.99, 75),
    (5, 'Monitor Stand', 'Furniture', 79.99, 100),
    (6, 'Ergonomic Chair', 'Furniture', 399.99, 30),
    (7, 'Desk Lamp', 'Furniture', 45.99, 120),
    (8, 'Notebook Set', 'Office', 12.99, 500),
    (9, 'Pen Pack', 'Office', 8.99, 1000),
    (10, 'Desk Organizer', 'Office', 24.99, 200);

-- Insert orders
INSERT INTO orders (id, customer_id, order_date, status, total) VALUES
    (1, 1, '2024-06-01', 'completed', 1379.97),
    (2, 2, '2024-06-05', 'completed', 479.98),
    (3, 1, '2024-06-10', 'shipped', 199.98),
    (4, 3, '2024-06-15', 'pending', 1299.99),
    (5, 4, '2024-06-20', 'completed', 95.97),
    (6, 5, '2024-06-25', 'shipped', 449.98),
    (7, 2, '2024-07-01', 'pending', 29.99);

-- Insert order items
INSERT INTO order_items (id, order_id, product_id, quantity, price) VALUES
    (1, 1, 1, 1, 1299.99),
    (2, 1, 2, 2, 29.99),
    (3, 1, 9, 2, 8.99),
    (4, 2, 6, 1, 399.99),
    (5, 2, 5, 1, 79.99),
    (6, 3, 4, 1, 149.99),
    (7, 3, 3, 1, 49.99),
    (8, 4, 1, 1, 1299.99),
    (9, 5, 7, 1, 45.99),
    (10, 5, 8, 2, 12.99),
    (11, 5, 10, 1, 24.99),
    (12, 6, 6, 1, 399.99),
    (13, 6, 3, 1, 49.99),
    (14, 7, 2, 1, 29.99);

-- ============================================
-- Create useful views
-- ============================================

-- Customer order summary
CREATE VIEW customer_orders AS
SELECT
    c.id as customer_id,
    c.name as customer_name,
    c.email,
    COUNT(o.id) as total_orders,
    COALESCE(SUM(o.total), 0) as total_spent,
    MAX(o.order_date) as last_order_date
FROM customers c
LEFT JOIN orders o ON c.id = o.customer_id
GROUP BY c.id, c.name, c.email;

-- Product sales summary
CREATE VIEW product_sales AS
SELECT
    p.id as product_id,
    p.name as product_name,
    p.category,
    p.price as unit_price,
    p.stock as current_stock,
    COALESCE(SUM(oi.quantity), 0) as units_sold,
    COALESCE(SUM(oi.quantity * oi.price), 0) as total_revenue
FROM products p
LEFT JOIN order_items oi ON p.id = oi.product_id
GROUP BY p.id, p.name, p.category, p.price, p.stock;

-- Order details view
CREATE VIEW order_details AS
SELECT
    o.id as order_id,
    o.order_date,
    o.status,
    c.name as customer_name,
    o.total as order_total,
    COUNT(oi.id) as item_count
FROM orders o
JOIN customers c ON o.customer_id = c.id
LEFT JOIN order_items oi ON o.id = oi.order_id
GROUP BY o.id, o.order_date, o.status, c.name, o.total;

.print 'E-commerce sample database initialized'
.print 'Tables: customers, products, orders, order_items'
.print 'Views: customer_orders, product_sales, order_details'
