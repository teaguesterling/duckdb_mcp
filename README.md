# DuckDB MCP Extension 🦆🔗

A **Model Context Protocol (MCP)** extension for DuckDB that enables seamless integration between SQL databases and MCP servers. This extension allows DuckDB to both consume MCP resources and serve as an MCP server.

## 🌟 **Core Features**

### **📡 Client Capabilities**
- **Resource Access**: Direct SQL access to MCP resources via `mcp://` URIs
- **Tool Execution**: Call MCP tools from SQL queries with `mcp_call_tool()`
- **File System Integration**: Use MCP resources with standard file functions (`read_csv`, `read_parquet`, etc.)
- **Multiple Transports**: Connect via stdio, TCP, and WebSocket protocols
- **Flexible Security**: Permissive mode for development, strict allowlists for production

### **🖥️ Server Capabilities**  
- **Resource Publishing**: Expose database tables and views as MCP resources
- **Tool Registration**: Custom SQL-based tools for data processing
- **JSON-RPC 2.0**: Complete MCP protocol implementation
- **Dual Modes**: Background and foreground server operation

---

## 🚀 **Quick Start**

### **Installation**
```bash
# Build the extension
make
# Start DuckDB with the extension
./build/release/duckdb
```

### **Client Usage - Development Mode (Permissive)**

For development and testing, the extension runs in **permissive mode** when no security settings are configured:

```sql
-- Load the extension
LOAD 'duckdb_mcp';

-- Connect directly without security settings (permissive mode)
ATTACH 'python3' AS data_server (
    TYPE mcp, 
    TRANSPORT 'stdio', 
    ARGS '["test/fastmcp/sample_data_server.py"]'
);

-- Access MCP resources through SQL
SELECT * FROM read_csv('mcp://data_server/file:///customers.csv');
SELECT * FROM read_csv('mcp://data_server/file:///orders.csv');

-- Use MCP functions
SELECT mcp_list_resources('data_server') AS resources;
SELECT mcp_call_tool('data_server', 'get_data_info', '{"dataset": "customers"}') AS info;
```

### **Client Usage - Production Mode (Secure)**

For production, configure explicit security settings to enable strict validation:

```sql
-- Load extension and set security policy
LOAD 'duckdb_mcp';
SET allowed_mcp_commands='python3:/usr/bin/python3:/usr/bin/node';

-- Now connections require allowlist validation
ATTACH 'python3' AS secure_server (
    TYPE mcp, 
    TRANSPORT 'stdio', 
    ARGS '["production/data_server.py"]',
    ENV '{"PYTHONPATH": "/opt/services"}'
);
```

### **Server Usage**

```sql
-- Create sample data
CREATE TABLE sales_data AS
SELECT generate_series as id, 
       'Product_' || (generate_series % 10) as product,
       (random() * 1000)::INT as amount
FROM generate_series(1, 1000);

-- Start MCP server to expose data
SELECT mcp_server_start('stdio', 'localhost', 8080, '{}') AS server_status;

-- Check server status
SELECT mcp_server_status() AS status;
```

---

## 📖 **Client API Reference**

### **Connection Methods**

**1. Structured Parameters (Recommended)**
```sql
ATTACH 'command' AS server_name (
    TYPE mcp,
    TRANSPORT 'stdio',                    -- stdio, tcp, websocket
    ARGS '["script.py", "--option"]',     -- JSON array format
    CWD '/working/directory',             -- Working directory
    ENV '{"VAR": "value"}'                -- JSON object format
);
```

**2. Configuration File**
```sql
ATTACH 'server_name' AS alias (
    TYPE mcp,
    FROM_CONFIG_FILE '/path/to/.mcp.json'
);
```

Sample `.mcp.json`:
```json
{
  "mcpServers": {
    "data_server": {
      "command": "python3",
      "args": ["services/data_server.py"],
      "cwd": "/opt/project",
      "env": {"PYTHONPATH": "/opt/lib"}
    }
  }
}
```

### **Resource Access**

**File System Integration**
```sql
-- CSV files
SELECT * FROM read_csv('mcp://server/file:///data.csv');

-- Parquet files  
SELECT * FROM read_parquet('mcp://server/file:///warehouse/sales.parquet');

-- JSON data
SELECT * FROM read_json('mcp://server/file:///config/settings.json');
```

**Direct Resource Functions**
```sql
-- List available resources
SELECT mcp_list_resources('server_name') AS resources;

-- Get resource content
SELECT mcp_get_resource('server_name', 'file:///data.csv') AS content;
```

### **Tool Execution**

```sql
-- Data processing tools
SELECT mcp_call_tool('server', 'process_data', '{"table": "sales", "operation": "summarize"}') AS result;

-- External API integration
SELECT mcp_call_tool('api_server', 'fetch_data', '{"endpoint": "/customers", "limit": 100}') AS data;

-- File operations
SELECT mcp_call_tool('file_server', 'convert_format', '{"input": "data.csv", "output": "data.parquet"}') AS status;
```

---

## 🖥️ **Server API Reference**

### **Server Management**

```sql
-- Start server
SELECT mcp_server_start(
    transport,    -- 'stdio', 'tcp', 'websocket'  
    host,         -- 'localhost' for TCP/WebSocket
    port,         -- Port number for TCP/WebSocket
    config        -- JSON configuration object
) AS server_id;

-- Server control
SELECT mcp_server_stop() AS stopped;
SELECT mcp_server_status() AS status;
```

### **Resource Publishing**

```sql
-- Publish table as resource
SELECT mcp_publish_table('table_name', 'data://tables/table_name', 'json') AS published;

-- Publish query as resource  
SELECT mcp_publish_query(
    'SELECT * FROM sales WHERE amount > 500',
    'data://views/high_value_sales', 
    'json',
    300  -- Refresh interval in seconds
) AS published;
```

---

## 🔒 **Security Model**

### **Permissive Mode (Development)**

When **no security settings** are configured, the extension operates in permissive mode:
- ✅ All commands and URLs allowed
- ✅ No explicit configuration required
- ⚠️ **Use only for development/testing**

```sql
-- Permissive mode - no security configuration needed
ATTACH 'python3' AS dev_server (TYPE mcp, ARGS '["test_server.py"]');
```

### **Strict Mode (Production)**

When **any security setting** is configured, strict validation is enforced:

```sql
-- Enable strict mode by setting allowed commands
SET allowed_mcp_commands='python3:/usr/bin/python3';

-- Now all connections must match allowlist
ATTACH 'python3' AS prod_server (TYPE mcp, ARGS '["prod_server.py"]');
```

**Security Rules:**
- ✅ **Allowlist Required**: Commands must be explicitly allowed
- ✅ **Immutable**: Cannot modify allowlist after first use
- ✅ **Path Validation**: Relative commands matched against absolute paths
- ✅ **Argument Safety**: Dangerous characters blocked in arguments

### **Configuration Options**

```sql
-- Command allowlist (colon-separated)
SET allowed_mcp_commands='python3:/usr/bin/node:./scripts/launcher.sh';

-- URL allowlist (space-separated)  
SET allowed_mcp_urls='https://api.example.com https://trusted.domain';

-- Server configuration
SET mcp_server_file='/etc/mcp/servers.json';
SET mcp_lock_servers=true;           -- Prevent runtime changes
SET mcp_disable_serving=true;        -- Client-only mode
```

---

## 🏗️ **Three-Tier Architecture Example**

This example demonstrates a complete data pipeline using three MCP servers in different tiers:

```sql
-- Load extension (permissive mode for simplicity)
LOAD 'duckdb_mcp';

-- Tier 1: Data Ingestion Layer
ATTACH 'python3' AS ingestion (
    TYPE mcp,
    ARGS '["services/ingestion_server.py"]',
    ENV '{"TIER": "ingestion", "LOG_LEVEL": "INFO"}'
);

-- Tier 2: Processing Layer  
ATTACH 'python3' AS processing (
    TYPE mcp,
    ARGS '["services/processing_server.py"]', 
    ENV '{"TIER": "processing", "WORKERS": "4"}'
);

-- Tier 3: Analytics Layer
ATTACH '/usr/bin/node' AS analytics (
    TYPE mcp,
    ARGS '["services/analytics_server.js", "--cluster"],
    ENV '{"NODE_ENV": "production", "TIER": "analytics"}'
);

-- Multi-tier data pipeline
WITH raw_data AS (
    -- Tier 1: Ingest raw data
    SELECT * FROM read_csv('mcp://ingestion/file:///raw/transactions.csv')
),
processed_data AS (
    -- Tier 2: Clean and transform
    SELECT *, json_extract(
        mcp_call_tool('processing', 'clean_transaction', 
            json_object('transaction_id', transaction_id, 'amount', amount)
        ), '$.cleaned_amount'
    )::DECIMAL as clean_amount
    FROM raw_data
),
analytics_ready AS (
    -- Tier 3: Enrich with analytics
    SELECT *, json_extract(
        mcp_call_tool('analytics', 'score_transaction',
            json_object('amount', clean_amount, 'merchant', merchant_name)
        ), '$.risk_score'
    )::DECIMAL as risk_score
    FROM processed_data
)
SELECT transaction_id, merchant_name, clean_amount, risk_score
FROM analytics_ready 
WHERE risk_score < 0.8
ORDER BY clean_amount DESC;
```

This pipeline demonstrates:
- **Tier 1** (Ingestion): Raw data access and initial loading
- **Tier 2** (Processing): Data cleaning and transformation  
- **Tier 3** (Analytics): Advanced analysis and scoring
- **SQL Integration**: Seamless data flow through standard SQL

---

## 🛠️ **Common Patterns**

### **Development Workflow**
```sql
-- Quick development setup (permissive mode)
ATTACH 'python3' AS dev (TYPE mcp, ARGS '["dev_server.py", "--debug"]');
SELECT mcp_list_resources('dev') AS available;
```

### **Production Deployment**  
```sql
-- Secure production setup
SET allowed_mcp_commands='/opt/services/data_server:/opt/services/api_server';
ATTACH '/opt/services/data_server' AS production (
    TYPE mcp,
    FROM_CONFIG_FILE '/etc/mcp/production.json'
);
```

### **Error Handling**
```sql
-- Check connection health
SELECT mcp_server_health('server_name') AS health_status;

-- Reconnect if needed
SELECT mcp_reconnect_server('server_name') AS reconnect_result;
```

---

## 🧪 **Testing**

```bash
# Run full test suite
make test

# Test client functionality
python3 test_new_attach_syntax.py

# Test server functionality  
python3 simple_layer2_test.py
```

**Test SQL**
```sql
-- Basic connectivity test
LOAD 'duckdb_mcp';
ATTACH 'python3' AS test (TYPE mcp, ARGS '["test/sample_server.py"]');
SELECT COUNT(*) FROM read_csv('mcp://test/file:///test_data.csv');
```

---

## 🔧 **Troubleshooting**

### **Security Issues**

**Permissive Mode Not Working?**
- Ensure no security settings are configured
- Check that extension is loaded first

**Strict Mode Errors?**
```sql
-- Check current security state
SELECT 
    CASE WHEN setting_value = '' THEN 'PERMISSIVE' ELSE 'STRICT' END as mode,
    setting_value as allowed_commands
FROM duckdb_settings() 
WHERE setting_name = 'allowed_mcp_commands';
```

### **Connection Issues**

**Server Not Responding**
```sql
-- Check server status
SELECT mcp_list_resources('server_name') AS status;
SELECT mcp_server_health('server_name') AS health;
```

**Resource Access Errors**
- Verify URI format: `mcp://server_name/resource_uri`
- Check working directory and file paths
- Validate server configuration

---

## 📚 **Documentation**

- **[Architecture Documentation](docs/)** - Technical design details
- **[Security Implementation](SECURITY_IMPLEMENTATION.md)** - Security features
- **[Implementation Roadmap](IMPLEMENTATION_ROADMAP.md)** - Development progress

---

## 🎯 **Status**

### **✅ Completed**
- Complete MCP protocol implementation
- Client and server functionality  
- File system integration (MCPFS)
- Security model with permissive/strict modes
- JSON-based ATTACH syntax
- Configuration file support
- Comprehensive testing

### **🚧 In Progress**  
- TCP/WebSocket transport support
- Enhanced error handling
- Performance optimizations

---

## 📄 **License**

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

**🎉 Seamlessly integrate MCP servers with SQL workflows using DuckDB MCP Extension!**

*Built with ❤️ for the data community*