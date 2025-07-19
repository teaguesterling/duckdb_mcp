# MCP + DuckDB Integration Design

## Overview

This document outlines the comprehensive design for integrating the Model Context Protocol (MCP) with DuckDB through extension(s) that enable bidirectional communication between DuckDB and MCP servers/clients.

## Core Concepts

### 1. ATTACH-based MCP Server Connections
Use DuckDB's ATTACH syntax to connect to MCP servers as if they were databases, enabling SQL-native access to MCP capabilities.

### 2. MCPFS - MCP File System
Following DuckDB's tarfs/zipfs patterns, implement mcpfs with dual schema format:
```
mcp://<server_name>/<mcp_protocol>://<resource_path>
```

Examples:
- `mcp://vscode_server/file:///workspace/data.csv`
- `mcp://system_server/system://status`
- `mcp://workspace_server/resource://documents/report.json`

### 3. Bidirectional Integration
- **Client Mode**: DuckDB connects to external MCP servers
- **Server Mode**: DuckDB exposes its capabilities as an MCP server
- **Hybrid Mode**: Both simultaneously for complex workflows

## ATTACH Syntax and Parameters

### Basic ATTACH Syntax
```sql
ATTACH '<transport_spec>' AS <alias> (TYPE mcp, <parameters>);
```

### Transport-Specific Parameters

#### STDIO Transport
```sql
ATTACH 'stdio:///path/to/mcp-server' AS server (
    TYPE mcp,
    
    -- Command execution
    args := ['--config', '/path/to/config.json'],
    cwd := '/working/directory',
    env := {'MCP_DEBUG': '1', 'API_KEY': 'secret'},
    
    -- Process management
    timeout := '30s',
    restart_on_failure := true,
    max_restarts := 3,
    
    -- Security
    allowed_tools := ['search', 'read_file'],
    denied_tools := ['execute', 'write_file'],
    allowed_resource_schemes := ['file', 'resource'],
    denied_resource_schemes := ['system'],
    allowed_paths := ['/workspace/', '/tmp/'],
    denied_paths := ['/etc/', '/home/'],
    
    -- Performance
    max_resource_size := '100MB',
    resource_cache_ttl := '5m',
    concurrent_requests := 5
);
```

#### TCP/WebSocket Transport
```sql
ATTACH 'tcp://localhost:8080' AS remote_server (
    TYPE mcp,
    
    -- Connection
    protocol := 'websocket',  -- or 'tcp'
    timeout := '10s',
    keepalive := '30s',
    reconnect := true,
    max_reconnects := 5,
    
    -- Authentication
    auth_type := 'bearer',  -- or 'basic', 'api_key'
    auth_token := 'bearer_token_here',
    api_key := 'x-api-key-value',
    username := 'user',
    password := 'pass',
    
    -- TLS/Security
    tls := true,
    verify_cert := true,
    ca_cert := '/path/to/ca.pem',
    client_cert := '/path/to/client.pem',
    client_key := '/path/to/client.key',
    
    -- Security restrictions (same as stdio)
    allowed_tools := ['query', 'describe'],
    denied_tools := ['admin'],
    allowed_resource_schemes := ['data', 'reports'],
    
    -- Performance
    max_resource_size := '500MB',
    resource_cache_ttl := '10m',
    concurrent_requests := 10,
    compression := true
);
```

#### Named Pipe Transport (Windows/Unix)
```sql
ATTACH 'pipe://./pipe/mcp-server' AS pipe_server (
    TYPE mcp,
    
    -- Pipe-specific
    buffer_size := '64KB',
    timeout := '15s',
    
    -- Security (same patterns as above)
    allowed_tools := [...],
    allowed_paths := [...]
);
```

### Security Parameter Categories

#### Tool Access Control
```sql
-- Whitelist approach
allowed_tools := ['search_files', 'read_file', 'analyze_code']

-- Blacklist approach  
denied_tools := ['execute_command', 'write_file', 'delete_file']

-- Pattern matching
allowed_tool_patterns := ['read_*', 'search_*', 'analyze_*']
denied_tool_patterns := ['*_admin', 'delete_*', 'execute_*']
```

#### Resource Access Control
```sql
-- MCP protocol schemes
allowed_resource_schemes := ['file', 'resource', 'data']
denied_resource_schemes := ['system', 'admin', 'internal']

-- File system paths (for file:// resources)
allowed_paths := ['/workspace/', '/data/', '/tmp/output/']
denied_paths := ['/etc/', '/home/', '/root/', '/sys/']

-- Pattern matching for resource URIs
allowed_resource_patterns := ['file:///workspace/**', 'data://public/**']
denied_resource_patterns := ['file:///etc/**', 'system://**']
```

#### Performance Controls
```sql
-- Resource limits
max_resource_size := '100MB',
max_resources_per_request := 10,
max_concurrent_requests := 5,

-- Timeouts
request_timeout := '30s',
resource_download_timeout := '60s',
connection_timeout := '10s',

-- Caching
resource_cache_ttl := '5m',
tool_result_cache_ttl := '1m',
enable_caching := true
```

## MCPFS Implementation Design

### Dual Schema Format

Following DuckDB's tarfs/zipfs pattern, mcpfs uses nested URI schemes:

```
mcp://<server_name>/<mcp_protocol>://<resource_path>
```

Where:
- `<server_name>`: Alias from ATTACH statement
- `<mcp_protocol>`: MCP resource scheme (file, resource, system, data, etc.)
- `<resource_path>`: Path within the MCP protocol namespace

### Schema Translation Examples

```sql
-- File system resources
'mcp://vscode/file:///workspace/data.csv'
→ MCP call: {"method": "resources/read", "params": {"uri": "file:///workspace/data.csv"}}

-- Custom resource protocols
'mcp://workspace/resource://documents/report.json'
→ MCP call: {"method": "resources/read", "params": {"uri": "resource://documents/report.json"}}

-- System information
'mcp://system/system://status'
→ MCP call: {"method": "resources/read", "params": {"uri": "system://status"}}

-- Database resources
'mcp://db_server/data://tables/users'
→ MCP call: {"method": "resources/read", "params": {"uri": "data://tables/users"}}
```

### MCPFS Function Integration

All existing DuckDB file functions work seamlessly:

```sql
-- CSV files through MCP
SELECT * FROM read_csv('mcp://workspace/file:///data/sales.csv');

-- Parquet files with glob patterns
SELECT * FROM read_parquet('mcp://storage/data://tables/*.parquet');

-- JSON documents
SELECT * FROM read_json('mcp://api_server/resource://reports/daily.json');

-- Markdown documents
SELECT * FROM read_markdown('mcp://docs/file:///guides/*.md');

-- Multiple files with UNION
SELECT * FROM read_csv([
    'mcp://server1/file:///data1.csv',
    'mcp://server2/file:///data2.csv',
    'mcp://server3/resource://datasets/data3.csv'
]);
```

## MCP Client Extension Functions

### Connection Management
```sql
-- List attached MCP servers
mcp_list_servers() → table(
    name VARCHAR,
    transport VARCHAR, 
    status VARCHAR,
    capabilities VARCHAR[],
    connected_since TIMESTAMP
)

-- Get server information
mcp_server_info(server_name VARCHAR) → STRUCT(
    name VARCHAR,
    version VARCHAR,
    capabilities STRUCT(
        tools BOOLEAN,
        resources BOOLEAN,
        prompts BOOLEAN,
        sampling BOOLEAN
    ),
    protocol_version VARCHAR
)

-- Connection health
mcp_ping(server_name VARCHAR) → STRUCT(
    success BOOLEAN,
    latency_ms INTEGER,
    last_error VARCHAR
)
```

### Tool Execution
```sql
-- Direct tool calls
mcp_call_tool(
    server_name VARCHAR, 
    tool_name VARCHAR, 
    arguments JSON
) → JSON

-- Streaming tool calls (for long-running operations)
mcp_call_tool_streaming(
    server_name VARCHAR,
    tool_name VARCHAR, 
    arguments JSON
) → table(chunk_id INTEGER, content VARCHAR, is_final BOOLEAN)

-- List available tools
mcp_list_tools(server_name VARCHAR) → table(
    name VARCHAR,
    description VARCHAR,
    input_schema JSON,
    capabilities VARCHAR[]
)
```

### Resource Management
```sql
-- List resources with pattern matching
mcp_list_resources(
    server_name VARCHAR, 
    uri_pattern VARCHAR := '*'
) → table(
    uri VARCHAR,
    name VARCHAR,
    description VARCHAR,
    mime_type VARCHAR,
    size BIGINT
)

-- Check resource existence
mcp_resource_exists(
    server_name VARCHAR, 
    resource_uri VARCHAR
) → BOOLEAN

-- Get resource metadata
mcp_resource_info(
    server_name VARCHAR,
    resource_uri VARCHAR
) → STRUCT(
    uri VARCHAR,
    mime_type VARCHAR,
    size BIGINT,
    last_modified TIMESTAMP,
    etag VARCHAR
)

-- Direct resource reading (bypassing mcpfs)
mcp_read_resource(
    server_name VARCHAR,
    resource_uri VARCHAR
) → VARCHAR

-- Subscribe to resource changes
mcp_subscribe_resource(
    server_name VARCHAR,
    resource_uri VARCHAR
) → BOOLEAN
```

### Prompt Management
```sql
-- List available prompts
mcp_list_prompts(server_name VARCHAR) → table(
    name VARCHAR,
    description VARCHAR,
    arguments JSON
)

-- Get prompt with arguments
mcp_get_prompt(
    server_name VARCHAR,
    prompt_name VARCHAR,
    arguments JSON := {}
) → STRUCT(
    messages JSON[],
    model_preferences JSON
)
```

## MCP Server Extension Functions

### Server Lifecycle
```sql
-- Start MCP server
mcp_server_start(
    transport VARCHAR := 'stdio',  -- 'stdio', 'tcp', 'websocket', 'pipe'
    bind_address VARCHAR := NULL,  -- for tcp/websocket
    port INTEGER := NULL,          -- for tcp/websocket
    auth_config JSON := NULL       -- authentication configuration
) → BOOLEAN

-- Stop MCP server
mcp_server_stop() → BOOLEAN

-- Server status
mcp_server_status() → STRUCT(
    running BOOLEAN,
    transport VARCHAR,
    connections INTEGER,
    uptime_seconds INTEGER,
    requests_served BIGINT
)
```

### Resource Publishing
```sql
-- Publish tables as resources
mcp_publish_table(
    table_name VARCHAR,
    resource_uri VARCHAR := NULL,  -- defaults to 'data://tables/{table_name}'
    formats VARCHAR[] := ['arrow', 'json', 'csv']
) → BOOLEAN

-- Publish query results
mcp_publish_query(
    query VARCHAR,
    resource_uri VARCHAR,
    refresh_interval VARCHAR := NULL,  -- '5m', '1h', etc.
    formats VARCHAR[] := ['arrow', 'json']
) → BOOLEAN

-- Publish file system paths
mcp_publish_directory(
    directory_path VARCHAR,
    resource_prefix VARCHAR := 'file://',
    recursive BOOLEAN := true,
    file_patterns VARCHAR[] := ['*']
) → INTEGER

-- Unpublish resources
mcp_unpublish_resource(resource_uri VARCHAR) → BOOLEAN

-- List published resources
mcp_list_published_resources() → table(
    resource_uri VARCHAR,
    type VARCHAR,  -- 'table', 'query', 'file', 'directory'
    formats VARCHAR[],
    size BIGINT,
    published_at TIMESTAMP
)
```

### Tool Registration
```sql
-- Register SQL-based tools
mcp_register_sql_tool(
    name VARCHAR,
    description VARCHAR,
    parameters JSON,
    sql_template VARCHAR
) → BOOLEAN

-- Example: Register analytics tool
SELECT mcp_register_sql_tool(
    'analyze_sales',
    'Analyze sales data for specific time period and region',
    {
        'start_date': {'type': 'string', 'format': 'date'},
        'end_date': {'type': 'string', 'format': 'date'},
        'region': {'type': 'string', 'enum': ['US', 'EU', 'APAC']}
    },
    'SELECT region, SUM(amount) as total, COUNT(*) as transactions 
     FROM sales 
     WHERE date BETWEEN $start_date AND $end_date 
       AND region = $region 
     GROUP BY region'
);

-- Register function-based tools
mcp_register_function_tool(
    name VARCHAR,
    description VARCHAR,
    parameters JSON,
    function_name VARCHAR
) → BOOLEAN

-- Unregister tools
mcp_unregister_tool(name VARCHAR) → BOOLEAN

-- List registered tools
mcp_list_registered_tools() → table(
    name VARCHAR,
    description VARCHAR,
    type VARCHAR,  -- 'sql', 'function'
    parameters JSON
)
```

### Built-in Tools

The MCP server automatically exposes core DuckDB capabilities:

```sql
-- Tool: "query"
{
    "name": "query",
    "description": "Execute SQL query",
    "inputSchema": {
        "type": "object",
        "properties": {
            "sql": {"type": "string"},
            "format": {"type": "string", "enum": ["json", "arrow", "csv"]}
        }
    }
}

-- Tool: "describe"
{
    "name": "describe",
    "description": "Get table or query schema information",
    "inputSchema": {
        "type": "object",
        "properties": {
            "table": {"type": "string"},
            "query": {"type": "string"}
        }
    }
}

-- Tool: "export"
{
    "name": "export", 
    "description": "Export query results to various formats",
    "inputSchema": {
        "type": "object",
        "properties": {
            "query": {"type": "string"},
            "format": {"type": "string", "enum": ["csv", "json", "parquet", "arrow"]},
            "output": {"type": "string"}
        }
    }
}

-- Tool: "attach"
{
    "name": "attach",
    "description": "Attach external database",
    "inputSchema": {
        "type": "object", 
        "properties": {
            "connection_string": {"type": "string"},
            "alias": {"type": "string"},
            "type": {"type": "string"}
        }
    }
}
```

## Advanced Integration Patterns

### Multi-Server Workflows
```sql
-- Connect to multiple MCP servers
ATTACH 'stdio:///workspace-server' AS workspace (TYPE mcp);
ATTACH 'tcp://analysis-server:8080' AS analysis (TYPE mcp);
ATTACH 'stdio:///reporting-server' AS reports (TYPE mcp);

-- Cross-server data pipeline
WITH source_data AS (
    SELECT * FROM read_csv('mcp://workspace/file:///data/raw.csv')
),
processed_data AS (
    SELECT 
        *,
        analysis.calculate_score(features) AS ml_score
    FROM source_data
)
SELECT reports.generate_report(
    template := 'ml_analysis',
    data := (SELECT * FROM processed_data WHERE ml_score > 0.8)
);
```

### Dynamic Resource Discovery
```sql
-- Discover and process all CSV files from workspace
WITH available_csvs AS (
    SELECT uri
    FROM mcp_list_resources('workspace', 'file://**.csv')
),
processed_files AS (
    SELECT 
        uri,
        COUNT(*) as row_count,
        array_agg(DISTINCT column_name) as columns
    FROM available_csvs,
         LATERAL read_csv('mcp://workspace/' || regexp_replace(uri, '^file://', ''))
    GROUP BY uri
)
SELECT * FROM processed_files;
```

### Real-time Data Streaming
```sql
-- Subscribe to resource changes and process updates
SELECT mcp_subscribe_resource('data_server', 'stream://live_data');

-- Process streaming data (conceptual - would need streaming table support)
CREATE LIVE TABLE live_analysis AS
SELECT 
    timestamp,
    value,
    LAG(value) OVER (ORDER BY timestamp) as prev_value,
    value - LAG(value) OVER (ORDER BY timestamp) as delta
FROM read_json_stream('mcp://data_server/stream://live_data');
```

## Security Architecture

### Authentication Models
```sql
-- Token-based authentication
ATTACH 'tcp://secure-server:443' AS secure (
    TYPE mcp,
    auth_type := 'bearer',
    auth_token := get_secret('MCP_TOKEN'),
    tls := true
);

-- Certificate-based authentication
ATTACH 'tcp://enterprise-server:8443' AS enterprise (
    TYPE mcp,
    auth_type := 'certificate',
    client_cert := '/certs/client.pem',
    client_key := '/certs/client.key',
    ca_cert := '/certs/ca.pem'
);
```

### Resource Sandboxing
```sql
-- Strict resource access control
ATTACH 'stdio:///untrusted-server' AS untrusted (
    TYPE mcp,
    allowed_resource_schemes := ['data'],
    denied_resource_schemes := ['file', 'system'],
    max_resource_size := '10MB',
    request_timeout := '5s'
);
```

### Audit Logging
```sql
-- Enable comprehensive audit logging
mcp_configure_audit_log(
    enabled := true,
    log_file := '/var/log/duckdb-mcp-audit.log',
    log_level := 'INFO',
    include_data := false  -- Don't log actual data for privacy
);

-- Query audit logs
SELECT * FROM mcp_audit_log() 
WHERE event_type = 'tool_call' 
  AND timestamp > now() - INTERVAL '1 hour';
```

## Performance Considerations

### Caching Strategies
```sql
-- Resource caching configuration
mcp_configure_cache(
    enabled := true,
    max_size := '1GB',
    ttl_default := '5m',
    ttl_by_mime_type := {
        'application/json': '1m',
        'text/csv': '10m',
        'application/vnd.apache.parquet': '30m'
    }
);
```

### Parallel Processing
```sql
-- Concurrent resource access
SELECT *
FROM read_parquet([
    'mcp://server1/data://dataset1.parquet',
    'mcp://server2/data://dataset2.parquet', 
    'mcp://server3/data://dataset3.parquet'
])
-- DuckDB automatically parallelizes across servers
```

### Connection Pooling
```sql
-- Connection pool configuration
ATTACH 'tcp://busy-server:8080' AS busy (
    TYPE mcp,
    max_connections := 10,
    connection_pool_timeout := '30s',
    keepalive := true
);
```

## Implementation Phases

### Phase 1: Basic MCPFS
- Implement mcpfs file system interface
- Basic ATTACH support for stdio transport
- Core resource reading functionality
- Integration with existing DuckDB file functions

### Phase 2: Client Extension
- Full MCP client protocol implementation
- Tool execution capabilities
- Advanced transport support (TCP, WebSocket)
- Security and authentication

### Phase 3: Server Extension  
- MCP server implementation
- Resource publishing capabilities
- Built-in tool registration
- Server management functions

### Phase 4: Advanced Features
- Streaming resource support
- Real-time subscriptions
- Advanced caching and performance optimization
- Comprehensive audit and monitoring

## Conclusion

This design creates a comprehensive MCP integration for DuckDB that:

1. **Maintains DuckDB Idioms**: Uses familiar ATTACH syntax and file function patterns
2. **Provides Rich Configuration**: Extensive parameters for security, performance, and functionality
3. **Enables Dual Schema**: Clean separation of server names and MCP resource protocols
4. **Supports Bidirectional Flow**: Both client and server capabilities
5. **Ensures Security**: Comprehensive access control and sandboxing options
6. **Optimizes Performance**: Caching, connection pooling, and parallel processing

The result is a powerful platform for SQL-native agent interactions and collaborative data analysis workflows.