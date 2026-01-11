# DuckDB MCP Extension

A Model Context Protocol (MCP) extension for DuckDB that enables seamless integration between SQL databases and AI assistants.

## What is MCP?

The [Model Context Protocol](https://modelcontextprotocol.io/) is an open standard that enables AI assistants like Claude to securely access external data sources and tools. This extension brings MCP capabilities directly into DuckDB, allowing you to:

- **Query remote data** via MCP servers using familiar SQL
- **Expose your database** as an MCP server for AI assistants
- **Execute tools** on MCP servers from within SQL queries
- **Publish custom tools** that wrap SQL queries

## Core Capabilities

### As an MCP Client

Connect to external MCP servers and access their resources:

```sql
-- Attach an MCP server
ATTACH 'python3' AS data_server (
    TYPE mcp,
    ARGS '["path/to/server.py"]'
);

-- Read CSV data through MCP
SELECT * FROM read_csv('mcp://data_server/file:///data.csv');

-- Execute a tool on the server
SELECT mcp_call_tool('data_server', 'process_data', '{"table": "sales"}');
```

### As an MCP Server

Expose your DuckDB database as an MCP server for AI assistants:

```sql
-- Start the MCP server
SELECT mcp_server_start('stdio', 'localhost', 0, '{}');

-- Publish a table as a resource
SELECT mcp_publish_table('products', 'data://products', 'json');

-- Publish a custom tool
SELECT mcp_publish_tool(
    'search_products',
    'Search products by name',
    'SELECT * FROM products WHERE name ILIKE ''%'' || $query || ''%''',
    '{"query": {"type": "string", "description": "Search term"}}',
    '["query"]'
);
```

## Quick Links

<div class="grid cards" markdown>

-   :material-rocket-launch: **[Getting Started](getting-started.md)**

    Install and run your first MCP query in minutes

-   :material-api: **[Client Reference](reference/client.md)**

    Complete API reference for MCP client functions

-   :material-server: **[Server Reference](reference/server.md)**

    API reference for running DuckDB as an MCP server

-   :material-cog: **[Configuration](reference/configuration.md)**

    Server options, security settings, and config files

-   :material-code-tags: **[Examples](examples.md)**

    Ready-to-use example configurations

-   :material-shield-lock: **[Security Guide](guides/security.md)**

    Production security best practices

</div>

## Use Cases

### Data Analysis with AI

Give AI assistants like Claude direct SQL access to your data:

```json
{
  "mcpServers": {
    "analytics": {
      "command": "duckdb",
      "args": ["-init", "analytics-server.sql"]
    }
  }
}
```

### ETL Pipelines

Use DuckDB's MCP client to pull data from multiple MCP sources:

```sql
-- Combine data from multiple MCP servers
SELECT s.*, c.country_name
FROM read_csv('mcp://sales_server/exports/sales.csv') s
JOIN read_csv('mcp://geo_server/countries.csv') c ON s.country_code = c.code;
```

### Custom API Endpoints

Expose domain-specific SQL queries as tools for AI assistants:

```sql
SELECT mcp_publish_tool(
    'revenue_by_region',
    'Get revenue breakdown by region for a date range',
    'SELECT region, SUM(amount) as revenue
     FROM orders
     WHERE date BETWEEN $start_date AND $end_date
     GROUP BY region',
    '{"start_date": {"type": "string"}, "end_date": {"type": "string"}}',
    '["start_date", "end_date"]'
);
```

## Requirements

- DuckDB 1.0.0 or later
- C++17 compatible compiler (for building from source)
- Python 3.8+ (for running example MCP servers)

## License

MIT License - see [LICENSE](https://github.com/teague/duckdb_mcp/blob/main/LICENSE) for details.
