# DuckDB MCP Extension 🦆🔗

A comprehensive **Model Context Protocol (MCP)** extension for DuckDB that enables seamless integration with MCP servers and powerful data processing workflows. This extension allows DuckDB to consume MCP resources, execute tools, and serve as an MCP server itself.

## 🌟 **Key Features**

### **📡 MCP Client Capabilities**
- **JSON-Based ATTACH Syntax**: Clean structured parameters with type safety
- **MCP Config File Support**: Direct .mcp.json integration with FROM_CONFIG_FILE
- **MCPFS Integration**: Direct file system access to MCP resources via SQL
- **Resource Consumption**: Read and query data from any MCP server
- **Tool Execution**: Execute MCP tools directly from SQL queries
- **Security Controls**: Configurable command allowlists and access controls
- **Multiple Transports**: Connect via stdio, TCP, and WebSocket protocols

### **🖥️ MCP Server Features**
- **Full JSON-RPC 2.0 Implementation**: Complete MCP protocol with all standard methods
- **Dual-Mode Operation**: Background and foreground server modes
- **Resource Publishing**: Expose database tables and views as MCP resources
- **Tool Registration**: Custom SQL-based tools for data processing
- **Graceful Shutdown**: Clean server termination with proper cleanup

### **🚀 Advanced Capabilities**
- **Composable Architectures**: Chain multiple MCP servers for complex data pipelines
- **SQL Enhancement**: Transform and augment data through SQL views and macros
- **High-Performance JSON**: Optimized JSON processing with yyjson library
- **Comprehensive Testing**: Full test suite covering client and server functionality

---

## 🚀 **Quick Start**

### **Installation**

```bash
# Clone the repository
git clone <repository-url>
cd duckdb_mcp

# Build the extension
make

# Start DuckDB with the extension
./build/release/duckdb
```

### **Client Usage - Consuming MCP Resources**

```sql
-- Load the MCP extension
LOAD 'duckdb_mcp';

-- Enable MCP commands (security requirement)
SET allowed_mcp_commands='/usr/bin/python3,python3';

-- Connect using structured parameters with JSON (recommended)
ATTACH 'python3' AS base_layer (
    TYPE mcp, 
    TRANSPORT 'stdio', 
    ARGS '["test/fastmcp/sample_data_server.py"]',
    CWD '/current/working/directory',
    ENV '{"PYTHONPATH": "/usr/local/lib/python3.8/site-packages", "MCP_DEBUG": "1"}'
);

-- Or connect using .mcp.json config file
ATTACH 'sample_data' AS configured_server (
    TYPE mcp, 
    FROM_CONFIG_FILE '/current/dir/.mcp.json'
);

-- Verify connection
SHOW DATABASES;

-- Query MCP resources through file functions
SELECT * FROM read_csv('mcp://base_layer/file:///customers.csv');
SELECT * FROM read_csv('mcp://base_layer/file:///orders.csv');

-- Use MCP client functions directly
SELECT mcp_list_resources('base_layer') AS available_resources;
SELECT mcp_call_tool('base_layer', 'get_data_info', '{"dataset": "customers"}') AS info;
```

### **Server Usage - Publishing Resources**

```sql
-- Load extension and create sample data
LOAD 'duckdb_mcp';

CREATE TABLE analytics_data AS
SELECT generate_series as id, 'Record_' || generate_series as name,
       (generate_series * 100 + random() * 50)::INT as value
FROM generate_series(1, 100);

-- Create analytical views to expose via MCP
CREATE VIEW summary_stats AS
SELECT COUNT(*) as total_records, AVG(value) as avg_value, 
       MIN(value) as min_value, MAX(value) as max_value
FROM analytics_data;

-- Start MCP server to expose these resources
SELECT mcp_server_start('stdio', 'localhost', 8080, '{}') AS server_status;
```

---

## 📋 **Detailed Examples**

### **Tool Execution Examples**

**Data Analysis Tools:**
```sql
-- Get dataset information
SELECT mcp_call_tool('data_server', 'get_data_info', '{"dataset": "customers"}') AS info;

-- Validate data quality
SELECT mcp_call_tool('data_server', 'validate_data', '{"table": "orders", "checks": ["nulls", "duplicates"]}') AS validation;

-- Generate statistical summaries
SELECT mcp_call_tool('analytics_server', 'summarize', '{"columns": ["amount", "quantity"], "groupby": "category"}') AS stats;
```

**File Processing Tools:**
```sql
-- Process CSV files
SELECT mcp_call_tool('file_server', 'convert_format', '{"input": "data.csv", "output": "data.parquet", "format": "parquet"}') AS result;

-- Data transformation pipeline
SELECT mcp_call_tool('transform_server', 'apply_pipeline', '{"steps": ["clean", "normalize", "aggregate"], "config": {"output_format": "json"}}') AS transformed;
```

**API Integration Tools:**
```sql
-- External API calls through MCP
SELECT mcp_call_tool('api_server', 'fetch_external', '{"endpoint": "https://api.example.com/data", "auth": "bearer_token"}') AS api_data;

-- Data enrichment
SELECT mcp_call_tool('enrichment_server', 'enrich_records', '{"source": "customers", "enrich_with": ["geography", "demographics"]}') AS enriched;
```

### **Resource Access Examples**

**Direct File Access:**
```sql
-- CSV files with proper error handling
SELECT * FROM read_csv('mcp://data_server/file:///raw_data/customers.csv') 
WHERE customer_id IS NOT NULL;

-- JSON data processing
SELECT json_extract(content, '$.metadata') as metadata
FROM read_text('mcp://data_server/file:///config/settings.json');

-- Parquet files for analytics
SELECT date_trunc('month', created_date) as month, SUM(amount) as total_sales
FROM read_parquet('mcp://analytics_server/file:///warehouse/sales.parquet')
GROUP BY month ORDER BY month;
```

**Dynamic Resource Discovery:**
```sql
-- List all available resources
WITH resources AS (
    SELECT json_extract_string(resource, '$.uri') as uri,
           json_extract_string(resource, '$.name') as name,
           json_extract_string(resource, '$.mimeType') as type
    FROM (SELECT unnest(json_extract_string_array(mcp_list_resources('data_server'), '$[*]')) as resource)
)
SELECT uri, name, type FROM resources WHERE type LIKE '%csv%';

-- Access resources dynamically based on discovery
SELECT * FROM read_csv('mcp://data_server/' || uri) 
FROM (SELECT 'file:///data/latest_export.csv' as uri);
```

**Complex Data Pipelines:**
```sql
-- Multi-stage data processing with MCP
WITH raw_data AS (
    SELECT * FROM read_csv('mcp://source_server/file:///raw/transactions.csv')
),
enriched_data AS (
    SELECT *, json_extract(
        mcp_call_tool('enrichment_server', 'geocode', json_object('address', billing_address))
    , '$.coordinates') as coordinates
    FROM raw_data
),
analytics_ready AS (
    SELECT transaction_id, amount, coordinates,
           json_extract_string(coordinates, '$.lat') as latitude,
           json_extract_string(coordinates, '$.lng') as longitude
    FROM enriched_data
)
SELECT * FROM analytics_ready;
```

### **Configuration Examples**

**Development Environment:**
```sql
-- Local development with debug output
ATTACH './venv/bin/python' AS dev_server (
    TYPE mcp,
    TRANSPORT 'stdio',
    ARGS '["src/dev_server.py", "--debug", "--port", "8001"]',
    CWD '/home/user/project',
    ENV '{"PYTHONPATH": "./src", "LOG_LEVEL": "DEBUG", "DEV_MODE": "true"}'
);
```

**Production Environment:**
```sql
-- Production with optimized settings
ATTACH 'data_processor' AS prod_server (
    TYPE mcp,
    FROM_CONFIG_FILE '/etc/mcp/production.mcp.json'
);
```

**Multi-Server Setup:**
```sql
-- Connect to multiple specialized servers
SET allowed_mcp_commands='python3:/usr/bin/node:./scripts/launcher.sh';

-- Data ingestion server
ATTACH 'python3' AS ingestion (
    TYPE mcp, ARGS '["services/ingestion_server.py"]',
    ENV '{"PYTHONPATH": "/opt/services", "WORKER_COUNT": "4"}'
);

-- Analytics processing server  
ATTACH '/usr/bin/node' AS analytics (
    TYPE mcp, ARGS '["analytics/server.js", "--cluster"],
    ENV '{"NODE_ENV": "production", "MEMORY_LIMIT": "8GB"}'
);

-- File transformation server
ATTACH './scripts/launcher.sh' AS transform (
    TYPE mcp, ARGS '["transform_service"],
    CWD '/opt/transform'
);
```

---

## 🧪 **Testing and Validation**

### **Automated Tests**
```bash
# Run the full test suite
make test

# Test new JSON-based ATTACH syntax
python3 test_new_attach_syntax.py

# Test MCP server functionality  
python3 simple_layer2_test.py

# Validate client-server integration
python3 test_layer2_step_by_step.py
```

### **Client Testing Examples**
```sql
-- Test MCP client resource consumption
LOAD 'duckdb_mcp';
SET allowed_mcp_commands='/usr/bin/python3,python3';

-- Connect using structured parameters with JSON
ATTACH 'python3' AS sample_data (
    TYPE mcp,
    TRANSPORT 'stdio',
    ARGS '["test/fastmcp/sample_data_server.py"]',
    ENV '{"MCP_DEBUG": "1"}'
);

-- Or connect using config file
ATTACH 'sample_data' AS configured_data (
    TYPE mcp,
    FROM_CONFIG_FILE 'test/.mcp.json'
);

-- Validate resource access
SELECT COUNT(*) FROM read_csv('mcp://sample_data/file:///customers.csv');
SELECT AVG(amount) FROM read_csv('mcp://sample_data/file:///orders.csv');

-- Test MCP functions
SELECT mcp_list_resources('sample_data') AS resources;
SELECT mcp_call_tool('sample_data', 'get_data_info', '{"dataset": "customers"}') AS result;
```

### **Server Testing Examples**
```sql
-- Test MCP server resource publishing
LOAD 'duckdb_mcp';

-- Create test data to expose
CREATE TABLE test_metrics AS
SELECT generate_series as id, random() * 100 as score
FROM generate_series(1, 1000);

-- Start server and validate
SELECT mcp_server_start('stdio', 'localhost', 8080, '{}') AS server;
SELECT mcp_server_status() AS status;
```

---

## 🔧 **Building**

### **Prerequisites**
- DuckDB development environment
- VCPKG for dependency management (optional)
- CMake and C++11 compiler
- Python 3.7+ for testing scripts

### **Build Steps**
```bash
# Setup VCPKG (optional, for dependencies)
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
export VCPKG_TOOLCHAIN_PATH=`pwd`/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build the extension
make

# Run tests
make test
```

### **Build Artifacts**
- `./build/release/duckdb` - DuckDB shell with extension
- `./build/release/test/unittest` - Test runner
- `./build/release/extension/duckdb_mcp/duckdb_mcp.duckdb_extension` - Loadable extension

---

## 🔒 **Security Model**

### **Command Allowlisting (Required)**
Before connecting to any MCP servers, you **must** configure allowed commands for security:

```sql
-- Set allowed commands (colon-separated list)
SET allowed_mcp_commands='python3:/usr/bin/python3:/usr/bin/node';

-- OR individual commands
SET allowed_mcp_commands='./launch_mcp_server.sh';
```

**Security Rules:**
- ✅ **Allowlist Required**: No MCP connections allowed without explicit allowlist
- ✅ **Immutable Once Set**: Cannot modify allowed commands after first use (prevents privilege escalation)
- ✅ **Basename Matching**: Relative commands like `python3` match absolute paths like `/usr/bin/python3`
- ✅ **Argument Validation**: Command arguments checked for dangerous characters (`..`, `|`, `;`, `&`, etc.)
- ✅ **Path Isolation**: Working directories and environment variables properly isolated

### **Error Messages**
```sql
-- If no allowlist configured
ERROR: No MCP commands are allowed. Set allowed_mcp_commands setting first.

-- If command not in allowlist  
ERROR: MCP command 'untrusted_script' not allowed. Allowed commands: 'python3', './launch_server.sh'

-- If trying to modify after use
ERROR: Cannot modify allowed MCP commands: commands are immutable once set for security
```

---

## 📖 **API Reference**

### **Client Functions**
```sql
-- Security configuration (REQUIRED before MCP operations)
SET allowed_mcp_commands='python3:/usr/bin/python3:/usr/bin/node';
```

### **JSON-Based ATTACH Syntax**

**Structured Parameters (Recommended):**
```sql
ATTACH 'command_or_url' AS server_name (
    TYPE mcp,
    TRANSPORT 'stdio',                    -- Transport: stdio, tcp, websocket
    ARGS '["arg1", "arg2"]',             -- JSON array: ["test/server.py", "--debug"]
    CWD '/working/directory',            -- Working directory (optional)
    ENV '{"VAR": "value", "DEBUG": "1"}' -- JSON object: {"PYTHONPATH": "/path", "DEBUG": "1"}
);
```

**JSON Format Requirements:**
- **ARGS**: Must be valid JSON array starting with `[` (e.g., `'["server.py", "--verbose"]'`)
- **ENV**: Must be valid JSON object starting with `{` (e.g., `'{"PATH": "/usr/bin", "DEBUG": "1"}'`)
- **Fallback**: Non-JSON strings treated as single argument/environment variable

**Config File Mode:**
```sql
ATTACH 'server_name' AS alias (
    TYPE mcp,
    FROM_CONFIG_FILE '/path/to/.mcp.json'
);
```

**Sample .mcp.json Format:**
```json
{
  "mcpServers": {
    "sample_data": {
      "command": "python3",
      "args": ["test/fastmcp/sample_data_server.py"],
      "cwd": "/mnt/aux-data/teague/Projects/duckdb_mcp",
      "env": {
        "PYTHONPATH": "/usr/local/lib/python3.8/site-packages",
        "MCP_DEBUG": "1"
      }
    }
  }
}
```

### **Resource Access and Tool Execution**
```sql
-- Resource access through file functions
SELECT * FROM read_csv('mcp://server_name/file:///data.csv');

-- Direct MCP functions
SELECT mcp_list_resources('server_name') AS resources;
SELECT mcp_call_tool('server_name', 'tool_name', '{"param": "value"}') AS result;
SELECT mcp_get_resource('server_name', 'resource_uri') AS content;
```

### **Server Functions**
```sql
-- Server lifecycle management
SELECT mcp_server_start(transport, host, port, config) AS server_id;
SELECT mcp_server_stop(server_id) AS stopped;
SELECT mcp_server_status(server_id) AS status;

-- Resource publishing (tables/views automatically exposed)
-- Server configuration
SELECT mcp_server_start('stdio', 'localhost', 8080, 
  '{"enable_query_tool": true, "enable_describe_tool": true}') AS server;
```

### **MCP Protocol Methods**
- `initialize` - Initialize MCP connection with capabilities
- `resources/list` - List available resources  
- `resources/read` - Read resource content
- `tools/list` - List available tools
- `tools/call` - Execute tool with parameters
- `shutdown` - Gracefully shut down server

---

## 🛠️ **Troubleshooting**

### **Common Connection Issues**

**Problem: "No MCP commands are allowed"**
```sql
-- Solution: Set allowed commands first
SET allowed_mcp_commands='python3:/usr/bin/python3';
```

**Problem: "MCP command 'script.py' not allowed"**
```sql
-- Solution: Add script to allowlist
SET allowed_mcp_commands='python3:./script.py:/full/path/to/script.py';
```

**Problem: "Cannot modify allowed MCP commands: commands are immutable"**
```sql
-- Solution: Restart DuckDB session to reset security settings
-- This is by design for security - once set, commands cannot be changed
```

### **JSON Parameter Issues**

**Problem: "Invalid JSON in ARGS parameter"**
```sql
-- ❌ Wrong: ARGS 'arg1, arg2'
-- ✅ Correct: 
ARGS '["arg1", "arg2"]'
```

**Problem: "Invalid JSON in ENV parameter"**
```sql
-- ❌ Wrong: ENV 'KEY=value KEY2=value2'
-- ✅ Correct:
ENV '{"KEY": "value", "KEY2": "value2"}'
```

### **Process Execution Issues**

**Problem: "Child process died immediately after fork"**
- **Check command path**: Ensure command exists and is executable
- **Check arguments**: Verify JSON array format in ARGS
- **Check working directory**: Ensure CWD path exists
- **Check environment**: Verify ENV JSON object format

**Problem: "Timeout waiting for process response"**
- **Server startup time**: Some MCP servers need time to initialize
- **Python environment**: Use correct Python path with required libraries
- **Virtual environment**: Activate venv before launching DuckDB

### **Resource Access Issues**

**Problem: "MCP server connection failed"**
```sql
-- Check server status
SELECT mcp_list_resources('server_name') AS status;

-- Verify ATTACH succeeded
SHOW DATABASES;
```

**Problem: "Resource not found: file:///data.csv"**
- **URI format**: Ensure proper MCP URI format `mcp://server_name/resource_uri`
- **Server capabilities**: Check if server provides requested resource
- **Path resolution**: Verify resource path relative to server working directory

### **Development and Testing**

**Debug Mode:**
```sql
-- Enable debug output in environment
ENV '{"MCP_DEBUG": "1"}'
```

**Test Connection:**
```sql
-- Basic connectivity test
SELECT mcp_list_resources('server_name') AS resources;

-- Tool execution test  
SELECT mcp_call_tool('server_name', 'get_data_info', '{}') AS result;
```

**Check Logs:**
```bash
# Check MCP server logs (if logging enabled)
tail -f mcp_server.log

# Check DuckDB debug output in stderr
```

---

## 🌍 **Use Cases**

### **🏢 Enterprise Data Architecture**
- **Standardized MCP Integration**: Use .mcp.json configs for consistent server connections
- **Data Warehouse Federation**: Connect multiple data sources via MCP with environment-specific settings
- **API Composition**: Combine and enhance multiple APIs with proper credential management
- **Service Mesh Analytics**: Distributed SQL computation across services with working directory isolation

### **🔄 Data Pipeline Orchestration** 
- **ETL Workflows**: Chain MCP servers with structured parameters for reliable data transformation
- **Multi-Tenant Platforms**: Isolated data processing per tenant using CWD and ENV parameters
- **Real-Time Analytics**: Stream processing with MCP integration and transport flexibility
- **Configuration Management**: Centralized .mcp.json files for development, staging, and production environments

### **🤖 AI/ML Integration**
- **Model Serving**: Expose ML models via MCP protocol with proper Python path and environment setup
- **Feature Engineering**: SQL-based feature transformation layers with configurable working directories
- **Data Preparation**: Multi-stage data cleaning and enhancement using structured server parameters

---

## 🗂️ **Project Structure**

```
duckdb_mcp/
├── src/
│   ├── duckdb_mcp_extension.cpp     # Main extension entry point
│   ├── server/                      # MCP server implementation
│   │   ├── mcp_server.cpp          # Core server logic
│   │   ├── resource_providers.cpp   # Resource management
│   │   └── tool_handlers.cpp       # Tool execution
│   ├── protocol/                    # MCP protocol implementation
│   │   ├── mcp_message.cpp         # JSON-RPC message handling
│   │   └── mcp_transport.cpp       # Transport layer
│   └── mcpfs/                      # MCP file system
│       └── mcp_file_system.cpp     # File access via MCP
├── test/
│   ├── sql/                        # SQL test cases
│   └── fastmcp/                    # Python MCP server tests
├── docs/                           # Architecture documentation
├── simple_layer2_test.py          # MCP server testing
└── test_layer2_step_by_step.py    # Integration validation
```

---

## 🤝 **Contributing**

We welcome contributions! Areas of interest:

- **Transport Layers**: TCP/WebSocket transport implementation
- **Security Features**: Authentication and authorization
- **Performance**: Resource streaming for large datasets  
- **Tooling**: Enhanced debugging and monitoring
- **Documentation**: More examples and use cases

### **Development Workflow**
1. Fork the repository
2. Create a feature branch
3. Implement changes with tests
4. Submit a pull request

---

## 📚 **Documentation**

- **[Implementation Roadmap](IMPLEMENTATION_ROADMAP.md)** - Development progress and milestones
- **[Architecture Documentation](docs/)** - Technical design documents
- **[Security Implementation](SECURITY_IMPLEMENTATION.md)** - Security features and best practices

---

## 🎯 **Roadmap**

### **🔥 Completed**
- ✅ Complete MCP protocol implementation (JSON-RPC 2.0)
- ✅ JSON-based ATTACH syntax with structured parameters
- ✅ .mcp.json configuration file support with FROM_CONFIG_FILE
- ✅ MCPFS client integration for resource consumption
- ✅ Dual-mode server architecture (background/foreground)  
- ✅ Graceful shutdown with MCP shutdown method
- ✅ SQL-based tool execution and resource publishing
- ✅ Comprehensive test infrastructure with 187+ assertions

### **🚧 In Progress**
- 🔄 TCP/WebSocket transport support
- 🔄 Advanced error handling and validation
- 🔄 Resource streaming for large datasets

### **📋 Planned**
- 📅 Bash-style parameter parsing (ARGS 'arg1 arg2' syntax)
- 📅 Authentication and authorization framework
- 📅 Configuration management system
- 📅 Container orchestration integration  
- 📅 Monitoring and metrics collection
- 📅 Multi-protocol bridge support

---

## 📄 **License**

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🙏 **Acknowledgments**

- **DuckDB Team** for the excellent extension framework
- **Model Context Protocol** specification and community
- **FastMCP** library for Python MCP server implementation
- **yyjson** library for high-performance JSON processing

---

**🎉 Seamlessly integrate MCP servers with SQL workflows using DuckDB MCP Extension!**

*Built with ❤️ for the data community*
