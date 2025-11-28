# DuckDB MCP Server Examples

This folder contains example configurations for running DuckDB as an MCP (Model Context Protocol) server.

## Examples Overview

| Example | Description | Key Features |
|---------|-------------|--------------|
| [01-simple](./01-simple/) | Minimal setup | Empty database, default tools |
| [02-configured](./02-configured/) | Custom configuration | Persistent DB, env vars, selective tools |
| [03-with-data](./03-with-data/) | Pre-loaded data | E-commerce schema, sample data, views |
| [04-security](./04-security/) | Security features | Query restrictions, read-only mode |
| [05-custom-tools](./05-custom-tools/) | Custom tools | DuckDB macros as domain tools |
| [06-comprehensive](./06-comprehensive/) | Full featured | All features combined |

## Quick Start

Each example follows the same pattern:

```bash
cd examples/01-simple
./launch-mcp.sh
```

Or with a specific DuckDB binary:

```bash
DUCKDB=/path/to/duckdb ./launch-mcp.sh
```

## Structure

Each example contains:

```
example-name/
├── launch-mcp.sh         # Entry point (called by MCP clients)
├── mcp.json              # MCP client configuration
├── init-mcp-db.sql       # Database initialization
├── start-mcp-server.sql  # Server startup
├── test-calls.json       # Test requests
└── README.md             # Documentation
```

## Using with MCP Clients

### Claude Desktop

Add to your Claude Desktop configuration (`~/.config/claude/mcp.json` or similar):

```json
{
  "mcpServers": {
    "duckdb": {
      "command": "/path/to/examples/01-simple/launch-mcp.sh"
    }
  }
}
```

### Other MCP Clients

Most MCP clients accept a command to spawn the server. Point it to the `launch-mcp.sh` script of your chosen example.

## Environment Variables

All examples support these environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `DUCKDB` | `duckdb` | Path to DuckDB binary |
| `DB_PATH` | (varies) | Database file path |

Additional variables may be supported by specific examples.

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
2. Modify `init-mcp-db.sql` for your schema
3. Adjust `start-mcp-server.sql` for your security needs
4. Update `mcp.json` with your settings

## Testing

Each example includes a `test-calls.json` file with sample MCP requests. Use these to verify your setup works correctly.

## Security Notes

- The `execute` tool is disabled by default for safety
- Use `denied_queries` to block dangerous SQL patterns
- Consider read-only mode for analytics use cases
- Always review what tools and queries you expose
