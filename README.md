# DuckDB MCP Extension 🦆🔗

A comprehensive **Model Context Protocol (MCP)** extension for DuckDB that enables seamless integration with MCP servers and powerful data processing workflows. This extension allows DuckDB to consume MCP resources, execute tools, and serve as an MCP server itself.

## 🌟 **Key Features**

### **📡 MCP Client Capabilities**
- **Structured ATTACH Syntax**: Clean parameter separation with TRANSPORT, ARGS, CWD, ENV
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

-- Test basic functionality
SELECT hello_mcp() as welcome;

-- Enable MCP commands (security requirement)
SET allowed_mcp_commands='/usr/bin/python3,python3';

-- Connect using structured parameters (recommended)
ATTACH 'python3' AS base_layer (
    TYPE mcp, 
    TRANSPORT stdio, 
    ARGS ['test/fastmcp/sample_data_server.py'],
    CWD '/current/working/directory',
    ENV {'PYTHONPATH': '/usr/local/lib/python3.8/site-packages', 'MCP_DEBUG': '1'}
);

-- Or connect using .mcp.json config file
ATTACH 'sample_data' AS configured_server (
    TYPE mcp, 
    FROM_CONFIG_FILE '/current/dir/.mcp.json'
);

-- Legacy format still supported
ATTACH 'stdio://python3 test/fastmcp/sample_data_server.py' AS legacy_server (TYPE mcp);

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

## 🧪 **Testing and Validation**

### **Automated Tests**
```bash
# Run the full test suite
make test

# Test MCP client functionality
python3 test/fastmcp/sample_data_server.py &
./build/release/duckdb -c "LOAD 'duckdb_mcp'; SELECT * FROM mcpfs_test();"

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

-- Connect using structured parameters
ATTACH 'python3' AS sample_data (
    TYPE mcp,
    TRANSPORT stdio,
    ARGS ['test/fastmcp/sample_data_server.py'],
    ENV {'MCP_DEBUG': '1'}
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

## 📖 **API Reference**

### **Client Functions**
```sql
-- Basic extension test
SELECT hello_mcp() AS greeting;

-- Security configuration (required before MCP operations)
SET allowed_mcp_commands='/usr/bin/python3,python3,/usr/bin/node';

-- Connect using structured parameters (recommended)
ATTACH 'command_or_url' AS server_name (
    TYPE mcp,
    TRANSPORT stdio,                    -- stdio, tcp, websocket
    ARGS ['arg1', 'arg2'],             -- Command arguments
    CWD '/working/directory',          -- Working directory
    ENV {'VAR': 'value'}               -- Environment variables
);

-- Connect using .mcp.json config file
ATTACH 'server_name' AS alias (
    TYPE mcp,
    FROM_CONFIG_FILE '/path/to/.mcp.json'
);

-- Legacy format (still supported)
ATTACH 'stdio://command args' AS server_name (TYPE mcp);

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
- ✅ Structured ATTACH syntax with TRANSPORT, ARGS, CWD, ENV parameters
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
