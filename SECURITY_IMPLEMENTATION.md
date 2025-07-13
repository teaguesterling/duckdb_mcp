# MCP Security Framework Implementation

## Overview
This document describes the security framework implemented for the DuckDB MCP extension to address command injection vulnerabilities and enforce secure server management.

## Security Features Implemented

### 1. Structured ATTACH Parameters
- **Old (vulnerable)**: `ATTACH 'stdio://python server.py' AS myserver (TYPE MCP)`
- **New (secure)**: `ATTACH DATABASE USING MCP(command='python', args=['server.py'], working_dir='/path/to/server') AS myserver`

### 2. Security Configuration
The extension provides security settings through the `mcp_configure()` function:

```sql
-- Set allowed MCP command paths (colon-delimited)
SELECT mcp_configure('allowed_mcp_commands=/usr/bin/python:/usr/bin/node');

-- Set allowed URLs for MCP servers (space-delimited)  
SELECT mcp_configure('allowed_mcp_urls=http://localhost https://api.example.com');

-- Set MCP server configuration file path
SELECT mcp_configure('mcp_server_file=./my-mcp-config.json');

-- Lock server configuration (prevents further changes)
SELECT mcp_configure('mcp_lock_servers=true');
```

### 3. Command Validation
- **Allowlist-based**: Only commands in `allowed_mcp_commands` can be executed
- **Path validation**: Commands must match exact paths (no relative paths, no shell injection)
- **Argument sanitization**: Dangerous characters (`..`, `|`, `;`, `&`, backticks, `$`) are blocked

### 4. Server Locking
- Once `mcp_lock_servers=true` is set, no further ATTACH/DETACH operations are allowed
- Prevents runtime modification of server configurations
- Enforces static server configurations for production environments

## Implementation Components

### MCPSecurityConfig Class
- Singleton pattern for global security settings
- Thread-safe configuration management
- Secure defaults (no commands/URLs allowed initially)

### ParseMCPAttachParams Function
- Parses structured ATTACH parameters safely
- Supports both new structured format and legacy stdio:// for backward compatibility
- Validates parameter types and structure

### Command Validation
- `IsCommandAllowed()`: Checks command against allowlist
- `ValidateAttachSecurity()`: Comprehensive security validation
- Prevents path traversal and shell injection attacks

## Security Benefits

1. **Command Injection Prevention**: Structured parameters eliminate shell injection risks
2. **Privilege Escalation Protection**: Allowlists prevent execution of unauthorized commands
3. **Configuration Immutability**: Server locking prevents runtime tampering
4. **Backward Compatibility**: Legacy stdio:// format still supported during transition
5. **Audit Trail**: All security violations logged with detailed error messages

## Example Secure Usage

```sql
-- Configure security settings
SELECT mcp_configure('allowed_mcp_commands=/usr/bin/python');
SELECT mcp_configure('mcp_lock_servers=true');

-- Secure ATTACH with structured parameters
ATTACH DATABASE USING MCP(
    command='/usr/bin/python',
    args=['fastmcp_server.py'],
    working_dir='/opt/mcp-servers/data-server'
) AS data_server;

-- Use MCP functions safely
SELECT mcp_list_resources('data_server');
SELECT mcp_get_resource('data_server', 'file:///data.csv');
```

## Backward Compatibility

The implementation maintains backward compatibility with the legacy stdio:// format:
```sql
ATTACH 'stdio://python server.py' AS myserver (TYPE MCP);
```

However, this format is subject to the same security validations and should be migrated to the structured format.

## Next Steps

1. Add .mcp.json server configuration file support
2. Implement process sandboxing for MCP server processes
3. Add resource limits and timeouts
4. Replace manual JSON construction with proper JSON library