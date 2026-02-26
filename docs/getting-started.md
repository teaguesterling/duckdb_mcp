# Getting Started

This guide will get you up and running with the DuckDB MCP extension in just a few minutes.

## Installation

### Building from Source

```bash
# Clone the repository
git clone https://github.com/teague/duckdb_mcp.git
cd duckdb_mcp

# Build the extension
make

# Run DuckDB with the extension
./build/release/duckdb
```

### Loading the Extension

Once built, load the extension in DuckDB:

```sql
LOAD 'duckdb_mcp';
```

## Quick Start: Running as an MCP Server

The most common use case is running DuckDB as an MCP server that AI assistants can connect to.

### 1. Create a Server Init Script

Create a file called `init-server.sql`:

```sql
-- Load the extension
LOAD 'build/release/duckdb_mcp.duckdb_extension';

-- Create some sample data
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name VARCHAR,
    price DECIMAL(10,2),
    category VARCHAR
);

INSERT INTO products VALUES
    (1, 'Widget', 9.99, 'Tools'),
    (2, 'Gadget', 24.99, 'Electronics'),
    (3, 'Gizmo', 14.99, 'Electronics');

-- Start the MCP server (stdio transport for CLI integration)
PRAGMA mcp_server_start('stdio');
```

### 2. Run the Server

```bash
duckdb -init init-server.sql
```

The server is now running and listening for MCP requests on stdin/stdout.

### 3. Connect with Claude Desktop

Add to your Claude Desktop configuration (`~/.config/claude/mcp.json` on Linux, `~/Library/Application Support/Claude/claude_desktop_config.json` on macOS):

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

Restart Claude Desktop. Claude can now query your database directly!

## Built-in Server Tools

When running as an MCP server, DuckDB exposes these tools to clients:

| Tool | Description |
|------|-------------|
| `query` | Execute SQL SELECT queries |
| `describe` | Get table or query schema information |
| `list_tables` | List all tables and views |
| `database_info` | Get database overview (schemas, tables, extensions) |
| `export` | Export query results to files |
| `execute` | Run DDL/DML statements (disabled by default) |

### Example: Query Tool

From an MCP client (like Claude), you can ask:

> "What products do we have in the Electronics category?"

Claude will use the `query` tool:

```json
{
  "name": "query",
  "arguments": {
    "sql": "SELECT * FROM products WHERE category = 'Electronics'",
    "format": "markdown"
  }
}
```

## Quick Start: Using as an MCP Client

You can also use DuckDB to connect to external MCP servers.

### Connecting to an MCP Server

```sql
-- Load the extension
LOAD 'duckdb_mcp';

-- Attach an MCP server
ATTACH 'python3' AS data_server (
    TYPE mcp,
    TRANSPORT 'stdio',
    ARGS '["path/to/mcp_server.py"]'
);
```

### Reading Data via MCP

```sql
-- List available resources
SELECT mcp_list_resources('data_server');

-- Read a specific resource
SELECT mcp_get_resource('data_server', 'file:///data/sales.csv');

-- Use DuckDB's file readers with MCP URIs
SELECT * FROM read_csv('mcp://data_server/file:///data/sales.csv');
```

### Calling Tools

```sql
-- List available tools
SELECT mcp_list_tools('data_server');

-- Call a tool
SELECT mcp_call_tool('data_server', 'analyze_sales', '{"year": 2024}');
```

## Output Formats

The `query` tool supports multiple output formats:

=== "JSON (default)"

    ```json
    {"name": "query", "arguments": {"sql": "SELECT * FROM products", "format": "json"}}
    ```

    Returns a JSON array of objects - best for programmatic processing.

=== "Markdown"

    ```json
    {"name": "query", "arguments": {"sql": "SELECT * FROM products", "format": "markdown"}}
    ```

    Returns a markdown table - **recommended for AI assistants** as it's more token-efficient.

=== "CSV"

    ```json
    {"name": "query", "arguments": {"sql": "SELECT * FROM products", "format": "csv"}}
    ```

    Returns comma-separated values - best for data export.

## Next Steps

- **[Server Functions Reference](reference/server.md)** - Complete API for running as a server
- **[Client Functions Reference](reference/client.md)** - Complete API for connecting to MCP servers
- **[Configuration Guide](reference/configuration.md)** - Server options and settings
- **[Security Guide](guides/security.md)** - Production security best practices
- **[Examples](examples.md)** - Ready-to-use example configurations
