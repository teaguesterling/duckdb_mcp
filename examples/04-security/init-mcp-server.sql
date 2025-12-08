-- Secure DuckDB MCP Server Initialization
-- Demonstrates security features and restrictions

-- Load the MCP extension
LOAD 'duckdb_mcp';

-- ============================================
-- Sample data (read-only access will be enforced)
-- ============================================

CREATE TABLE sensitive_data (
    id INTEGER PRIMARY KEY,
    user_id INTEGER,
    account_balance DECIMAL(10, 2),
    last_login TIMESTAMP
);

INSERT INTO sensitive_data VALUES
    (1, 1001, 5432.10, '2024-06-01 10:00:00'),
    (2, 1002, 1234.56, '2024-06-15 14:30:00'),
    (3, 1003, 9876.54, '2024-06-20 09:15:00');

CREATE TABLE audit_log (
    id INTEGER PRIMARY KEY,
    action VARCHAR,
    user_id INTEGER,
    timestamp TIMESTAMP
);

INSERT INTO audit_log VALUES
    (1, 'LOGIN', 1001, '2024-06-01 10:00:00'),
    (2, 'VIEW_BALANCE', 1001, '2024-06-01 10:01:00'),
    (3, 'LOGIN', 1002, '2024-06-15 14:30:00');

-- Create a safe view that masks sensitive data
CREATE VIEW safe_user_summary AS
SELECT
    user_id,
    CASE
        WHEN account_balance > 5000 THEN 'HIGH'
        WHEN account_balance > 1000 THEN 'MEDIUM'
        ELSE 'LOW'
    END as balance_tier,
    last_login
FROM sensitive_data;

-- Start MCP server with security-focused config
-- - Export and execute tools disabled
-- - Dangerous query patterns blocked
SELECT mcp_server_start('stdio', '{
  "enable_query_tool": true,
  "enable_describe_tool": true,
  "enable_list_tables_tool": true,
  "enable_database_info_tool": true,
  "enable_export_tool": false,
  "enable_execute_tool": false,
  "denied_queries": ["DROP", "TRUNCATE", "ALTER", "COPY"]
}');
