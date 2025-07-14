#!/bin/bash
# Layer 2: DuckDB Enhanced Server Implementation
# This script implements the middle layer of the nested MCP experiment

set -e

echo "🚀 Starting Layer 2: DuckDB Enhanced Server"
echo "============================================="

# Step 1: Start Python MCP server (Layer 1) in background
echo "1. Starting Python MCP server (Layer 1)..."
python3 test/fastmcp/sample_data_server.py &
PYTHON_SERVER_PID=$!

# Give the Python server time to start
sleep 2

# Step 2: Create enhancement script that connects to Python server
echo "2. Creating DuckDB enhancement session..."
cat > /tmp/enhance_session.sql << 'EOF'
-- Load MCP extension
LOAD 'duckdb_mcp';

-- Enable MCP commands  
SET allowed_mcp_commands='python3';

-- Connect to Python MCP server
ATTACH 'stdio://python3 test/fastmcp/sample_data_server.py' AS base_layer (TYPE mcp);

-- Create enhanced views
CREATE VIEW enhanced_customer_stats AS
SELECT 
    'customers' as dataset_name,
    COUNT(*) as total_customers,
    AVG(CASE WHEN name LIKE 'A%' THEN 1.0 ELSE 0.0 END) as pct_names_start_with_a,
    CURRENT_TIMESTAMP as analysis_timestamp
FROM read_csv('mcp://base_layer/file:///customers.csv');

CREATE VIEW enhanced_product_stats AS  
SELECT 
    'products' as dataset_name,
    COUNT(*) as total_products,
    AVG(price) as avg_price,
    MAX(price) as max_price,
    MIN(price) as min_price,
    CURRENT_TIMESTAMP as analysis_timestamp
FROM read_csv('mcp://base_layer/file:///products.csv');

-- Test the views work
SELECT '=== Enhanced Customer Stats ===' as section;
SELECT * FROM enhanced_customer_stats;

SELECT '=== Enhanced Product Stats ===' as section;
SELECT * FROM enhanced_product_stats;

-- Start our own MCP server (this will start in foreground mode)
SELECT 'Starting enhanced MCP server...' as status;
SELECT mcp_server_start('stdio', 'localhost', 8080, '{}') AS server_result;
EOF

# Step 3: Run the enhancement and server startup
echo "3. Running DuckDB enhancement and starting enhanced server..."
export DUCKDB_MCP_FOREGROUND=1
build/release/duckdb -init /tmp/enhance_session.sql

# Cleanup
echo "4. Cleaning up..."
kill $PYTHON_SERVER_PID 2>/dev/null || true
rm -f /tmp/enhance_session.sql

echo "✅ Layer 2 enhancement complete!"