-- Layer 2: DuckDB Enhanced Server Script
-- This script implements the middle layer of the nested MCP experiment
-- 1. Connects to Python MCP server as client (Layer 1)
-- 2. Creates enhanced SQL views/macros using Python resources  
-- 3. Starts own MCP server exposing original + enhanced resources

-- Load the MCP extension
LOAD 'duckdb_mcp';

-- Enable MCP commands for our Python server
SET allowed_mcp_commands='python3,/usr/bin/python3,python';

-- Connect to the Python MCP server (Layer 1)
ATTACH 'stdio://python3 test/fastmcp/sample_data_server.py' AS base_layer (TYPE mcp);

-- Verify connection to base layer
SHOW DATABASES;

-- Create enhanced views using base layer resources
-- View 1: Customer analysis using base data
CREATE VIEW enhanced_customer_stats AS
SELECT 
    'customers' as dataset_name,
    COUNT(*) as total_customers,
    AVG(CASE WHEN name LIKE 'A%' THEN 1.0 ELSE 0.0 END) as pct_names_start_with_a,
    CURRENT_TIMESTAMP as analysis_timestamp
FROM read_csv('mcp://base_layer/file:///customers.csv');

-- View 2: Products analysis  
CREATE VIEW enhanced_product_stats AS
SELECT 
    'products' as dataset_name,
    COUNT(*) as total_products,
    AVG(price) as avg_price,
    MAX(price) as max_price,
    MIN(price) as min_price,
    CURRENT_TIMESTAMP as analysis_timestamp
FROM read_csv('mcp://base_layer/file:///products.csv');

-- Macro 1: Get dataset info with enhanced metadata
CREATE MACRO get_enhanced_dataset_info(dataset_name) AS (
    SELECT 
        dataset_name as dataset,
        CASE 
            WHEN dataset_name = 'customers' THEN 
                (SELECT total_customers FROM enhanced_customer_stats)::VARCHAR
            WHEN dataset_name = 'products' THEN 
                (SELECT total_products FROM enhanced_product_stats)::VARCHAR
            ELSE 'Unknown dataset'
        END as enhanced_info,
        'Enhanced by DuckDB Layer 2' as source_layer
);

-- Macro 2: Cross-dataset analysis
CREATE MACRO cross_dataset_summary() AS (
    SELECT 
        'Cross-Dataset Analysis' as analysis_type,
        (SELECT total_customers FROM enhanced_customer_stats) as customer_count,
        (SELECT total_products FROM enhanced_product_stats) as product_count,
        (SELECT avg_price FROM enhanced_product_stats) as avg_product_price,
        CURRENT_TIMESTAMP as generated_at
);

-- Test our enhanced views and macros
SELECT '=== Testing Enhanced Views ===' as test_section;
SELECT * FROM enhanced_customer_stats;
SELECT * FROM enhanced_product_stats;

SELECT '=== Testing Enhanced Macros ===' as test_section;  
SELECT * FROM get_enhanced_dataset_info('customers');
SELECT * FROM get_enhanced_dataset_info('products');
SELECT * FROM cross_dataset_summary();

-- Publish some resources for our own MCP server
-- This would happen through the mcp_publish_resource function
-- For now, we'll simulate this with a status message
SELECT 'Enhanced DuckDB server ready to serve!' as status,
       'Original base_layer resources + enhanced views/macros' as capabilities;

-- Start the MCP server (this would normally be called programmatically)
-- For testing, we'll show what would be served
SELECT 'Would start MCP server exposing:' as server_info
UNION ALL
SELECT '- Base layer customers.csv resource'
UNION ALL  
SELECT '- Base layer products.csv resource'
UNION ALL
SELECT '- Enhanced customer_stats view'
UNION ALL
SELECT '- Enhanced product_stats view' 
UNION ALL
SELECT '- Enhanced dataset_info macro'
UNION ALL
SELECT '- Cross-dataset summary macro';

-- This would start the actual MCP server in practice:
-- SELECT mcp_server_start('stdio', 'localhost', 8080, '{}') AS server_result;