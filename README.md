# DuckDB MCP Extension 🦆🔗

A comprehensive **Model Context Protocol (MCP)** extension for DuckDB that enables composable, multi-layered data architectures. This extension allows DuckDB to both consume from and serve MCP resources, creating powerful nested service ecosystems.

## 🌟 **Key Features**

### **🔌 MCP Protocol Support**
- **Full JSON-RPC 2.0 Implementation**: Complete MCP protocol with all standard methods
- **Dual-Mode Server**: Background and foreground operation modes
- **Graceful Shutdown**: Clean server termination with `shutdown` method
- **Resource & Tool Management**: Publish/consume resources and execute tools via MCP

### **🏗️ Nested Architecture Support**  
- **Multi-Layer Ecosystems**: Build composable MCP service meshes
- **SQL Enhancement Layer**: Wrap MCP resources with SQL views and macros
- **Server-to-Server Communication**: MCP servers consuming from other MCP servers
- **Resource Layering**: Transform and re-expose data through multiple processing tiers

### **🚀 Advanced Capabilities**
- **MCPFS Integration**: File system access via MCP protocol
- **Security Controls**: Configurable command allowlists and access controls  
- **Transport Flexibility**: stdio, TCP, and WebSocket transport support (stdio fully implemented)
- **JSON Processing**: High-performance JSON handling with yyjson library

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

### **Basic Usage**

```sql
-- Load the MCP extension
LOAD 'duckdb_mcp';

-- Test basic functionality
SELECT hello_mcp() as welcome;

-- Start an MCP server
SELECT mcp_server_start('stdio', 'localhost', 8080, '{}') AS server_status;
```

### **Layer 2 Enhanced Server Example**

```sql
-- Load extension
LOAD 'duckdb_mcp';

-- Create enhanced analytics
CREATE TABLE customers AS
SELECT generate_series as id, 'Customer_' || generate_series as name,
       (generate_series * 100 + random() * 50)::INT as value
FROM generate_series(1, 5);

CREATE VIEW customer_analytics AS
SELECT COUNT(*) as total_customers, AVG(value) as avg_value,
       'enhanced_by_layer2' as source
FROM customers;

-- Start enhanced MCP server
SELECT mcp_server_start('stdio', 'localhost', 8080, '{}') AS enhanced_server;
```

---

## 🧪 **Testing the Ecosystem**

### **Automated Tests**
```bash
# Test Layer 2 enhanced server
python3 simple_layer2_test.py

# Test complete 3-layer ecosystem  
python3 test_full_nested_ecosystem.py

# Step-by-step Layer 2 validation
python3 test_layer2_step_by_step.py

# Layer 3 client demonstration
python3 layer3_client.py
```

### **Manual SQL Testing**
```sql
-- Complete ecosystem simulation
LOAD 'duckdb_mcp';

-- Layer 1: Base data
CREATE TABLE raw_data AS
SELECT generate_series as id, random() as value 
FROM generate_series(1, 10);

-- Layer 2: Enhanced analytics
CREATE VIEW enhanced_metrics AS
SELECT COUNT(*) as total_records, AVG(value) as avg_value,
       'layer2_enhanced' as processing_layer
FROM raw_data;

-- Layer 3: Strategic insights  
CREATE VIEW strategic_dashboard AS
SELECT 'Multi-layer MCP ecosystem' as architecture,
       (SELECT total_records FROM enhanced_metrics) as data_points,
       'Nested MCP experiment successful!' as status;

-- View results
SELECT * FROM strategic_dashboard;
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

### **Core Functions**
```sql
-- Basic extension test
SELECT hello_mcp() AS greeting;

-- Server lifecycle management
SELECT mcp_server_start(transport, host, port, config) AS server_id;
SELECT mcp_server_stop(server_id) AS stopped;
SELECT mcp_server_status(server_id) AS status;
```

### **MCP Protocol Methods**
- `initialize` - Initialize MCP connection with capabilities
- `resources/list` - List available resources  
- `resources/read` - Read resource content
- `tools/list` - List available tools
- `tools/call` - Execute tool with parameters
- `shutdown` - Gracefully shut down server

### **Server Configuration**
```sql
-- Enable MCP commands (security)
SET allowed_mcp_commands='python3,/usr/bin/node';

-- Start server with custom config
SELECT mcp_server_start('stdio', 'localhost', 8080, 
  '{"enable_query_tool": true, "enable_describe_tool": true}') AS server;
```

---

## 🌍 **Use Cases**

### **🏢 Enterprise Data Architecture**
- **Data Warehouse Federation**: Connect multiple data sources via MCP
- **API Composition**: Combine and enhance multiple APIs  
- **Service Mesh Analytics**: Distributed SQL computation across services

### **🔄 Data Pipeline Orchestration** 
- **ETL Workflows**: Chain MCP servers for data transformation
- **Multi-Tenant Platforms**: Isolated data processing per tenant
- **Real-Time Analytics**: Stream processing with MCP integration

### **🤖 AI/ML Integration**
- **Model Serving**: Expose ML models via MCP protocol
- **Feature Engineering**: SQL-based feature transformation layers
- **Data Preparation**: Multi-stage data cleaning and enhancement

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
├── docs/
│   └── MCP_NESTED_EXPERIMENT.md   # Architecture documentation
├── simple_layer2_test.py          # Layer 2 testing
├── layer3_client.py               # Layer 3 client demo
└── test_full_nested_ecosystem.py  # Complete ecosystem test
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

- **[MCP Nested Experiment](docs/MCP_NESTED_EXPERIMENT.md)** - Complete architecture guide
- **[Implementation Roadmap](IMPLEMENTATION_ROADMAP.md)** - Development progress
- **[Security Implementation](SECURITY_IMPLEMENTATION.md)** - Security features

---

## 🎯 **Roadmap**

### **🔥 Completed**
- ✅ Complete MCP protocol implementation (JSON-RPC 2.0)
- ✅ Dual-mode server architecture (background/foreground)  
- ✅ Graceful shutdown with MCP shutdown method
- ✅ 3-layer nested MCP ecosystem demonstration
- ✅ SQL enhancement layer (views, macros, analytics)
- ✅ Comprehensive test infrastructure

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

**🎉 Experience the future of composable data architectures with DuckDB MCP Extension!**

*Built with ❤️ for the data community*
