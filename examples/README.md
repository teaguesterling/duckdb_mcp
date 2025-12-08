# DuckDB MCP Server Examples

This folder contains example configurations for running DuckDB as an MCP (Model Context Protocol) server.

## Examples Overview

| Example | Description | Key Features |
|---------|-------------|--------------|
| [01-simple](./01-simple/) | Minimal setup | Empty database, default tools |
| [02-configured](./02-configured/) | Custom configuration | Selective tool enabling |
| [03-with-data](./03-with-data/) | Pre-loaded data | E-commerce schema, sample data, views |
| [04-security](./04-security/) | Security features | Query restrictions, disabled tools |
| [05-custom-tools](./05-custom-tools/) | Custom tools | DuckDB macros as domain tools |
| [06-comprehensive](./06-comprehensive/) | Full featured | All features combined |

## Quick Start

Each example can be run directly with duckdb:

```bash
cd examples/01-simple
cat test-calls.ldjson | duckdb -init init-mcp-server.sql 2>/dev/null | jq .
```

## Structure

Each example contains:

```
example-name/
├── init-mcp-server.sql   # SQL file that loads extension and starts server
├── mcp.json              # MCP client configuration
├── test-calls.ldjson     # Test requests (line-delimited JSON)
└── README.md             # Documentation
```

## Using with MCP Clients

### Claude Desktop

Add to your Claude Desktop configuration (`~/.config/claude/mcp.json` or similar):

```json
{
  "mcpServers": {
    "duckdb": {
      "command": "duckdb",
      "args": ["-init", "/path/to/examples/01-simple/init-mcp-server.sql"]
    }
  }
}
```

### Other MCP Clients

Most MCP clients accept a command and arguments. Use `duckdb` with `-init` pointing to the example's `init-mcp-server.sql` file.

## Available Tools

The MCP server exposes these built-in tools:

| Tool | Description | Default |
|------|-------------|---------|
| `query` | Execute SQL SELECT queries | Enabled |
| `describe` | Describe tables/queries | Enabled |
| `list_tables` | List tables and views | Enabled |
| `database_info` | Get database overview | Enabled |
| `export` | Export query results | Enabled |
| `execute` | Run DDL/DML statements | **Disabled** |

Tools can be enabled/disabled in the server configuration.

## Creating Your Own Configuration

1. Copy an example folder
2. Modify the SQL init file for your schema and server config
3. Update `mcp.json` with your settings

## Testing

Each example includes a `test-calls.ldjson` file with sample MCP requests:

```bash
cat test-calls.ldjson | duckdb -init init-mcp-server.sql 2>/dev/null | jq .
```

## Security Notes

- The `execute` tool is disabled by default for safety
- Use `denied_queries` to block dangerous SQL patterns
- Consider read-only mode for analytics use cases
- Always review what tools and queries you expose
