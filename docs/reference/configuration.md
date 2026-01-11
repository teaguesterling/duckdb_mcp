# Configuration Reference

This page documents all configuration options for the DuckDB MCP extension.

## Server Configuration

Configuration is passed as a JSON object to `mcp_server_start()`:

```sql
SELECT mcp_server_start('stdio', 'localhost', 0, '{
    "enable_query_tool": true,
    "enable_execute_tool": false,
    "default_result_format": "markdown"
}');
```

### Tool Enable/Disable Options

Control which tools are exposed to MCP clients:

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enable_query_tool` | boolean | `true` | Enable SQL SELECT queries |
| `enable_describe_tool` | boolean | `true` | Enable schema description |
| `enable_list_tables_tool` | boolean | `true` | Enable table listing |
| `enable_database_info_tool` | boolean | `true` | Enable database info |
| `enable_export_tool` | boolean | `true` | Enable data export |
| `enable_execute_tool` | boolean | `false` | Enable DDL/DML execution |

**Example: Read-only server with minimal tools**

```sql
SELECT mcp_server_start('stdio', 'localhost', 0, '{
    "enable_query_tool": true,
    "enable_describe_tool": true,
    "enable_list_tables_tool": true,
    "enable_database_info_tool": false,
    "enable_export_tool": false,
    "enable_execute_tool": false
}');
```

### Output Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `default_result_format` | string | `"json"` | Default format for query results |

**Supported formats:**

- `json` - JSON array of objects (best for programmatic processing)
- `markdown` - Markdown table (most token-efficient for AI)
- `csv` - Comma-separated values

### Runtime Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `max_requests` | integer | `0` | Maximum requests before auto-shutdown (0 = unlimited) |
| `background` | boolean | `false` | Run server in background thread |

---

## DuckDB Settings

These DuckDB settings control MCP extension behavior:

### Security Settings

| Setting | Description | Default |
|---------|-------------|---------|
| `allowed_mcp_commands` | Allowlist of executable commands | Empty (permissive) |
| `allowed_mcp_urls` | Allowlist of permitted URLs | Empty (permissive) |
| `mcp_server_file` | Path to MCP server configuration file | Empty |
| `mcp_lock_servers` | Prevent runtime server changes | `false` |
| `mcp_disable_serving` | Disable server functionality (client-only) | `false` |

**Example: Production security settings**

```sql
-- Only allow specific Python interpreters
SET allowed_mcp_commands = '/usr/bin/python3:/opt/venv/bin/python';

-- Restrict URL access
SET allowed_mcp_urls = 'https://api.example.com https://data.example.com';

-- Use a specific config file
SET mcp_server_file = '/etc/mcp/servers.json';

-- Lock configuration
SET mcp_lock_servers = true;
```

---

## Configuration Files

### MCP Server Configuration File

For client connections, you can define servers in a JSON configuration file:

```json
{
  "mcpServers": {
    "data_server": {
      "command": "python3",
      "args": ["services/data_server.py"],
      "cwd": "/opt/project",
      "env": {
        "PYTHONPATH": "/opt/lib",
        "DATABASE_URL": "postgresql://..."
      }
    },
    "api_server": {
      "command": "node",
      "args": ["dist/server.js"],
      "env": {
        "NODE_ENV": "production"
      }
    }
  }
}
```

**Using the configuration file:**

```sql
-- Set the config file
SET mcp_server_file = '/path/to/mcp-servers.json';

-- Attach a server by name
ATTACH 'data_server' AS data (
    TYPE mcp,
    FROM_CONFIG_FILE '/path/to/mcp-servers.json'
);
```

### Claude Desktop Configuration

When using DuckDB as an MCP server with Claude Desktop, add to your Claude configuration:

=== "Linux"

    `~/.config/claude/mcp.json`:

    ```json
    {
      "mcpServers": {
        "my-database": {
          "command": "duckdb",
          "args": ["-init", "/path/to/init-server.sql"]
        }
      }
    }
    ```

=== "macOS"

    `~/Library/Application Support/Claude/claude_desktop_config.json`:

    ```json
    {
      "mcpServers": {
        "my-database": {
          "command": "duckdb",
          "args": ["-init", "/path/to/init-server.sql"]
        }
      }
    }
    ```

=== "Windows"

    `%APPDATA%\Claude\claude_desktop_config.json`:

    ```json
    {
      "mcpServers": {
        "my-database": {
          "command": "duckdb.exe",
          "args": ["-init", "C:\\path\\to\\init-server.sql"]
        }
      }
    }
    ```

---

## Environment Variables

These environment variables affect extension behavior:

| Variable | Description |
|----------|-------------|
| `DUCKDB_MCP_DEBUG` | Enable debug logging (`1` = enabled) |
| `DUCKDB_MCP_BACKGROUND` | Run server in background mode (`1` = enabled) |

---

## Init Script Pattern

The recommended pattern for MCP servers is an init script that:

1. Loads the extension
2. Sets up the database (create tables, load data)
3. Configures security settings
4. Starts the MCP server

**Example: `init-server.sql`**

```sql
-- 1. Load extension
LOAD 'duckdb_mcp';

-- 2. Set up database
CREATE TABLE IF NOT EXISTS products (
    id INTEGER PRIMARY KEY,
    name VARCHAR NOT NULL,
    price DECIMAL(10,2)
);

-- Load data if table is empty
INSERT INTO products
SELECT * FROM read_csv('data/products.csv')
WHERE NOT EXISTS (SELECT 1 FROM products);

-- 3. Configure (optional)
-- SET allowed_mcp_commands = '...';

-- 4. Start server
SELECT mcp_server_start('stdio', 'localhost', 0, '{
    "enable_execute_tool": false,
    "default_result_format": "markdown"
}');
```

Run with:

```bash
duckdb -init init-server.sql
```
