-- DuckDB MCP HTTP Server Example
-- Starts an HTTP server with authentication and sample data

LOAD 'duckdb_mcp';

-- Create sample data
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name VARCHAR,
    category VARCHAR,
    price DECIMAL(10,2),
    stock INTEGER
);

INSERT INTO products VALUES
    (1, 'Laptop', 'Electronics', 999.99, 50),
    (2, 'Mouse', 'Electronics', 29.99, 200),
    (3, 'Keyboard', 'Electronics', 79.99, 150),
    (4, 'Monitor', 'Electronics', 349.99, 75),
    (5, 'Desk Chair', 'Furniture', 199.99, 30),
    (6, 'Standing Desk', 'Furniture', 499.99, 20);

-- Publish a custom search tool
SELECT mcp_publish_tool(
    'search_products',
    'Search products by name or category',
    'SELECT * FROM products WHERE name ILIKE ''%'' || $query || ''%'' OR category ILIKE ''%'' || $query || ''%''',
    '{"query": {"type": "string", "description": "Search term"}}',
    '["query"]',
    'markdown'
);

-- Start HTTP server with authentication
-- Token: demo-token (for demonstration only - use a secure token in production!)
SELECT mcp_server_start('http', 'localhost', 8080, '{
    "auth_token": "demo-token",
    "default_result_format": "json"
}');
