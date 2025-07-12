# MCPFS - MCP File System Specification

## Overview

MCPFS (Model Context Protocol File System) is a virtual file system implementation for DuckDB that follows the same patterns as tarfs and zipfs, enabling seamless access to MCP resources through familiar file path syntax.

## URI Schema Design

### Format
```
mcp://<server_name>/<mcp_protocol>://<resource_path>
```

### Components

**Server Name**: The alias assigned during ATTACH operation
- Must match the alias in `ATTACH ... AS <server_name>`
- Case-sensitive identifier
- Must follow DuckDB identifier rules

**MCP Protocol**: The URI scheme from the MCP resource
- Examples: `file`, `resource`, `system`, `data`, `stream`, `memory`
- Defined by the MCP server implementation
- Case-sensitive, typically lowercase

**Resource Path**: The path within the MCP protocol namespace
- Format depends on the MCP protocol
- For `file://`: standard filesystem paths
- For `resource://`: server-defined resource identifiers
- For `system://`: system resource identifiers

## Schema Translation

### Path Parsing Algorithm
```
Input: "mcp://vscode_server/file:///workspace/data.csv"

1. Split by first "://" → ["mcp", "vscode_server/file:///workspace/data.csv"]
2. Validate protocol is "mcp"
3. Split remainder by "/" → ["", "vscode_server", "file:::workspace", "data.csv"]
4. Extract server_name: "vscode_server"  
5. Find next "://" to identify MCP protocol boundary
6. Extract mcp_protocol: "file"
7. Extract resource_path: "/workspace/data.csv"
8. Reconstruct MCP URI: "file:///workspace/data.csv"
```

### Translation Examples

| MCPFS Path | Server | MCP Protocol | Resource Path | MCP URI |
|------------|--------|--------------|---------------|---------|
| `mcp://ws/file:///data.csv` | ws | file | /data.csv | `file:///data.csv` |
| `mcp://api/resource://reports/daily` | api | resource | reports/daily | `resource://reports/daily` |
| `mcp://sys/system://status` | sys | system | status | `system://status` |
| `mcp://db/data://tables/users` | db | data | tables/users | `data://tables/users` |
| `mcp://cache/memory://temp/results` | cache | memory | temp/results | `memory://temp/results` |

## Integration with DuckDB File Functions

### Automatic Function Support

All DuckDB file functions automatically work with MCPFS paths:

```sql
-- CSV reading
SELECT * FROM read_csv('mcp://server/file:///data.csv');
SELECT * FROM read_csv(['mcp://s1/file:///a.csv', 'mcp://s2/data://b.csv']);

-- Parquet reading with glob patterns  
SELECT * FROM read_parquet('mcp://storage/file:///datasets/*.parquet');

-- JSON documents
SELECT * FROM read_json('mcp://api/resource://reports/daily.json');

-- Text files
SELECT read_text('mcp://docs/file:///readme.txt');

-- Binary files
SELECT read_blob('mcp://assets/resource://images/logo.png');

-- Markdown documents
SELECT * FROM read_markdown('mcp://wiki/file:///guides/*.md');
```

### Multi-File Operations

```sql
-- Union across different servers and protocols
SELECT * FROM read_csv([
    'mcp://server1/file:///dataset1.csv',
    'mcp://server2/data://table2.csv', 
    'mcp://server3/resource://export3.csv'
]);

-- Mixed local and MCP files
SELECT * FROM read_parquet([
    'local_data.parquet',
    'mcp://remote/file:///remote_data.parquet',
    's3://bucket/s3_data.parquet'
]);
```

## Resource Type Detection

### MIME Type Mapping

MCPFS uses MCP resource metadata to determine appropriate file handling:

```sql
-- Resource metadata provides MIME type
{
    "uri": "resource://reports/daily.json",
    "mimeType": "application/json",
    "size": 1024
}

-- Maps to appropriate DuckDB function
read_json('mcp://server/resource://reports/daily.json')
```

### Automatic Format Detection

```sql
-- File extension-based detection
'mcp://server/file:///data.csv'      → read_csv()
'mcp://server/file:///data.json'     → read_json()  
'mcp://server/file:///data.parquet'  → read_parquet()
'mcp://server/file:///data.xlsx'     → read_excel()

-- MIME type-based detection (when available)
'application/json'                   → read_json()
'text/csv'                          → read_csv()
'application/vnd.apache.parquet'    → read_parquet()
'text/plain'                        → read_text()
```

## Glob Pattern Support

### Server-Side Globbing

When MCP servers support resource listing:

```sql
-- Server lists all matching resources
SELECT * FROM read_csv('mcp://server/file:///data/*.csv');

-- Recursive patterns
SELECT * FROM read_json('mcp://server/file:///logs/**/*.json');

-- Multiple patterns
SELECT * FROM read_parquet('mcp://server/data://tables/{users,orders,products}.parquet');
```

### Client-Side Globbing

When servers don't support pattern matching:

```sql
-- MCPFS expands patterns by listing resources
WITH available_files AS (
    SELECT uri 
    FROM mcp_list_resources('server', 'file://**/data*.csv')
)
SELECT * FROM read_csv(
    (SELECT array_agg('mcp://server/' || uri) FROM available_files)
);
```

## Caching and Performance

### Resource Caching

```sql
-- Configure caching behavior per server
ATTACH 'stdio:///server' AS server (
    TYPE mcp,
    resource_cache_ttl := '5m',
    max_resource_size := '100MB',
    cache_compression := true
);
```

### Cache Key Generation

```
Cache Key = hash(server_name + mcp_uri + file_modification_time)
Example: "server1:file:///data.csv:2024-01-15T10:30:00Z" → hash → "abc123..."
```

### Performance Optimizations

1. **Parallel Resource Access**: Multiple MCP resources fetched concurrently
2. **Streaming Downloads**: Large resources streamed rather than buffered
3. **Metadata Caching**: Resource metadata cached separately from content
4. **Connection Reuse**: HTTP connections pooled across requests

## Error Handling

### Path Validation Errors

```sql
-- Invalid server name
read_csv('mcp://nonexistent/file:///data.csv')
→ Error: MCP server 'nonexistent' not attached

-- Invalid MCP protocol  
read_csv('mcp://server/invalid:///data.csv')
→ Error: Resource not found: invalid:///data.csv

-- Invalid resource path
read_csv('mcp://server/file:///nonexistent.csv')  
→ Error: Resource not found: file:///nonexistent.csv
```

### Network and Protocol Errors

```sql
-- Server timeout
read_csv('mcp://slow_server/file:///big_file.csv')
→ Error: Request timeout after 30s

-- Authentication failure
read_csv('mcp://secure_server/file:///protected.csv')
→ Error: Access denied: insufficient permissions

-- Protocol error
read_csv('mcp://broken_server/file:///data.csv')
→ Error: MCP protocol error: invalid response format
```

### Graceful Degradation

```sql
-- Continue processing available files when some fail
SELECT * FROM read_csv([
    'mcp://server1/file:///good.csv',      -- succeeds
    'mcp://server2/file:///missing.csv',   -- fails
    'mcp://server3/file:///good2.csv'      -- succeeds  
], ignore_errors := true);
-- Returns data from good.csv and good2.csv, skips missing.csv
```

## Security Considerations

### Path Traversal Prevention

```sql
-- Blocked: attempt to escape resource namespace
'mcp://server/file:///../../../etc/passwd'
→ Normalized to: 'file:///etc/passwd'
→ Checked against allowed_paths restrictions

-- Blocked: protocol injection
'mcp://server/file:///data.csv#system://admin'
→ Fragment ignored, path: 'file:///data.csv'
```

### Access Control Integration

```sql
-- Server-level restrictions apply to all MCPFS access
ATTACH 'stdio:///server' AS server (
    TYPE mcp,
    allowed_resource_schemes := ['file', 'data'],
    allowed_paths := ['/workspace/', '/tmp/'],
    denied_paths := ['/etc/', '/home/']
);

-- These are automatically blocked:
read_csv('mcp://server/system://users')      -- scheme not allowed
read_csv('mcp://server/file:///etc/passwd')  -- path not allowed
```

### Resource Size Limits

```sql
-- Large file handling
ATTACH 'stdio:///server' AS server (
    TYPE mcp,
    max_resource_size := '100MB',
    streaming_threshold := '10MB'
);

-- Files > 10MB are streamed, files > 100MB are rejected
SELECT * FROM read_csv('mcp://server/file:///huge_file.csv');
```

## Implementation Details

### File System Interface

MCPFS implements the DuckDB FileSystem interface:

```cpp
class MCPFileSystem : public FileSystem {
public:
    // Core file operations
    unique_ptr<FileHandle> OpenFile(const string &path, uint8_t flags) override;
    void ReadFile(FileHandle &handle, void *buffer, int64_t nr_bytes, idx_t location) override;
    int64_t GetFileSize(FileHandle &handle) override;
    time_t GetLastModifiedTime(FileHandle &handle) override;
    
    // Directory operations  
    bool DirectoryExists(const string &directory) override;
    vector<string> Glob(const string &path) override;
    
    // File metadata
    bool FileExists(const string &filename) override;
    FileType GetFileType(const string &filename) override;
};
```

### Path Resolution Pipeline

```cpp
// 1. Parse MCPFS URI
MCPPath ParseMCPPath(const string &path) {
    // Extract server_name, mcp_protocol, resource_path
    return {server_name, mcp_protocol, resource_path};
}

// 2. Validate server connection
MCPConnection* GetServer(const string &server_name) {
    // Look up attached MCP server
    // Validate connection is active
    return connection;
}

// 3. Construct MCP resource URI
string BuildMCPURI(const string &protocol, const string &path) {
    return protocol + "://" + path;
}

// 4. Call MCP resource API
MCPResource GetResource(MCPConnection *conn, const string &uri) {
    // Send resources/read request
    // Handle response and errors
    return resource;
}
```

### Streaming Implementation

```cpp
class MCPFileHandle : public FileHandle {
private:
    MCPConnection *connection;
    string resource_uri;
    unique_ptr<MCPResourceStream> stream;
    
public:
    void Read(void *buffer, int64_t nr_bytes, idx_t location) override {
        if (!stream || location != current_position) {
            // Seek operation - restart stream or use range requests
            stream = connection->ReadResourceStream(resource_uri, location);
        }
        return stream->Read(buffer, nr_bytes);
    }
};
```

## Extension Integration

### Registration with DuckDB

```cpp
void MCPExtension::Load(DuckDB &db) {
    // Register MCPFS file system
    auto &fs = db.GetFileSystem();
    fs.RegisterSubSystem("mcp", make_unique<MCPFileSystem>());
    
    // Register MCP attachment handler
    auto &config = DBConfig::GetConfig(*db.instance);
    config.attachment_handlers["mcp"] = make_unique<MCPAttachmentHandler>();
}
```

### File Function Integration

```cpp
// Automatic integration - no changes needed to existing functions
// read_csv(), read_json(), etc. automatically work with mcp:// paths

// DuckDB file function call flow:
read_csv('mcp://server/file:///data.csv')
→ FileSystem::OpenFile('mcp://server/file:///data.csv')  
→ MCPFileSystem::OpenFile(...)
→ Parse path, get server, fetch resource
→ Return FileHandle for streaming read
```

This specification provides a comprehensive foundation for implementing MCPFS as a robust, secure, and performant file system interface for MCP resources within DuckDB.