# MCP + DuckDB Implementation Agent Prompt

## Mission
You are tasked with implementing MCP (Model Context Protocol) integration for DuckDB as a client extension. This will enable DuckDB to connect to MCP servers and access their resources through familiar SQL syntax.

## Context and Vision
The goal is to create an extension that allows:
```sql
-- Connect to MCP servers like databases
ATTACH 'stdio:///path/to/mcp-server' AS workspace (TYPE mcp);

-- Read MCP resources like regular files
SELECT * FROM read_csv('mcp://workspace/file:///data.csv');
SELECT * FROM read_json('mcp://workspace/resource://reports/daily.json');

-- Execute MCP tools as functions
SELECT mcp_call_tool('workspace', 'search_files', {'pattern': '*.py'});
```

## Implementation Strategy: Client-First Approach

**Start with client implementation because:**
- Client is foundational (server builds on client protocol)
- Can test against existing MCP servers (VS Code, Claude Desktop, etc.)
- Provides immediate value with MCPFS file system
- Smaller, more manageable scope
- Validates architecture before building server

## Phase 1 Priority: Basic MCPFS + stdio Transport

### Core Deliverables
1. **Extension scaffolding** following DuckDB patterns
2. **MCPFS file system** with dual schema: `mcp://<server>/<protocol>://<resource>`
3. **stdio transport** for process-based MCP servers
4. **Basic ATTACH support** with essential parameters
5. **File function integration** (read_csv, read_json, etc.)

### Success Criteria for Phase 1
- [ ] `ATTACH 'stdio:///server' AS test (TYPE mcp);` works
- [ ] `read_csv('mcp://test/file:///data.csv')` returns data
- [ ] Path parsing correctly extracts server/protocol/resource
- [ ] Error handling for connection failures
- [ ] Basic test suite passes

## Key Design Documents
Reference these comprehensive specifications in this directory:
- **MCP_DUCKDB_INTEGRATION_DESIGN.md**: Complete architecture and API design
- **MCPFS_SPECIFICATION.md**: File system interface specification  
- **IMPLEMENTATION_ROADMAP.md**: 4-phase detailed implementation plan

## Critical Architecture Decisions

### 1. Dual Schema URI Format
```
mcp://<server_name>/<mcp_protocol>://<resource_path>

Examples:
- mcp://workspace/file:///data.csv
- mcp://api/resource://reports/daily.json
- mcp://system/system://status
```

### 2. ATTACH Parameter Design
Focus on stdio transport first, with expansion points for other transports:
```sql
ATTACH 'stdio:///path/to/server' AS server (
    TYPE mcp,
    args := ['--config', 'config.json'],
    timeout := '30s',
    allowed_resource_schemes := ['file', 'resource']
);
```

### 3. Extension Structure
Follow existing DuckDB extension patterns (see duckdb_markdown, duckdb_yaml examples):
```
duckdb_mcp/
├── src/
│   ├── mcp_extension.cpp        # Main extension entry
│   ├── mcpfs/                   # File system implementation
│   ├── protocol/                # MCP protocol handling
│   ├── client/                  # Client functions
│   └── include/
├── test/sql/                    # SQL test files
└── CMakeLists.txt
```

## Technical Implementation Guidance

### 1. Start with Extension Scaffolding
- Copy patterns from existing extensions in this workspace
- Use vcpkg for dependencies (nlohmann-json for MCP protocol)
- Follow DuckDB extension registration patterns

### 2. Implement MCPFS First
```cpp
class MCPFileSystem : public FileSystem {
    // Implement OpenFile, FileExists, Glob, etc.
    // Parse mcp:// URLs and route to MCP connections
};
```

### 3. MCP Protocol Implementation
- JSON-RPC 2.0 over stdio pipes initially
- Focus on `resources/list` and `resources/read` methods
- Handle initialization handshake properly

### 4. Integration Points
- Register MCPFS with DuckDB file system
- Register ATTACH handler for MCP type
- Ensure all existing file functions work transparently

## Dependencies and Build
- **nlohmann-json**: MCP protocol handling
- **DuckDB extension SDK**: Follow existing patterns
- **Process management**: For stdio transport (stdin/stdout pipes)

## Testing Strategy
Create test MCP servers in Python/Node.js that provide:
- File resources (file://) for testing read_csv, read_json
- Custom resources (resource://) for testing protocol handling
- Error scenarios for robust error handling

## Common Pitfalls to Avoid
1. **Don't change implementation and tests simultaneously** - change one, test, then change the other
2. **Follow existing DuckDB patterns** - study markdown/YAML extensions first
3. **Start simple** - basic stdio transport before TCP/WebSocket
4. **Security from day one** - validate paths, limit resource access
5. **Memory management** - follow RAII patterns, avoid leaks

## Success Progression
**Week 1-2**: Extension scaffolding, basic MCPFS URI parsing
**Week 3-4**: stdio transport, MCP protocol handshake
**Week 5-6**: Resource reading, file function integration
**Week 7-8**: Error handling, test suite, documentation

## Key Files to Study First
In this workspace:
- `../duckdb_markdown/src/markdown_extension.cpp` - Extension registration patterns
- `../duckdb_markdown/src/markdown_utils.cpp` - File system integration
- `../duckdb_yaml/src/yaml_extension.cpp` - vcpkg dependency patterns

In DuckDB source:
- `duckdb/extension/parquet/parquet_extension.cpp` - Advanced file handling
- `duckdb/src/common/file_system/` - File system interface

## Final Notes
- **Be systematic**: Implement one component fully before moving to the next
- **Test incrementally**: Each component should have working tests
- **Follow the roadmap**: The 4-phase plan in IMPLEMENTATION_ROADMAP.md is your guide
- **Ask for clarification**: If design documents are unclear, ask before implementing

The foundation you build in Phase 1 will determine the success of the entire project. Focus on correctness and following DuckDB patterns rather than speed.