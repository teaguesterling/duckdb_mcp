CREATE TABLE test_customers (id INT, name VARCHAR, city VARCHAR, spent DECIMAL(10,2));
INSERT INTO test_customers VALUES 
  (1, 'Alice', 'NYC', 1200.50),
  (2, 'Bob', 'Chicago', 850.75),
  (3, 'Carol', 'SF', 2100.25);

SELECT mcp_server_start('stdio', 'localhost', 8080, '{}');
SELECT mcp_publish_table('test_customers', 'data://tables/customers', 'json');