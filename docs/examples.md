# Examples

The `examples/` directory contains ready-to-use MCP server configurations for various use cases.

## Quick Start

Each example includes:

- `init-mcp-server.sql` - SQL script that loads the extension, sets up data, and starts the server
- `mcp.json` - Client configuration for connecting to the server
- `test-calls.ldjson` - Line-delimited JSON test requests

### Running an Example

```bash
cd examples/01-simple
duckdb -init init-mcp-server.sql
```

### Testing with JSON-RPC

```bash
cat test-calls.ldjson | duckdb -init init-mcp-server.sql 2>/dev/null | jq .
```

---

## 01-simple

**Minimal setup with empty in-memory database**

The simplest possible MCP server - an empty database with default tools.

```sql
LOAD duckdb_mcp;
PRAGMA mcp_server_start('stdio');
```

**Use case:** Testing MCP client connections, learning the basics.

---

## 02-configured

**Custom configuration with persistent database**

Demonstrates server configuration options:

- Persistent database file
- Environment variables
- Tool enable/disable settings

```sql
PRAGMA mcp_server_start('stdio', '{
    "enable_execute_tool": false,
    "default_result_format": "markdown"
}');
```

**Use case:** Production-like configuration.

---

## 03-with-data

**E-commerce schema with sample data**

A realistic e-commerce database with:

- Products, orders, customers tables
- Sample data
- Pre-built views

```sql
CREATE TABLE products (...);
CREATE TABLE orders (...);
CREATE TABLE customers (...);

INSERT INTO products VALUES ...;

CREATE VIEW order_summary AS ...;
```

**Use case:** Demonstrating data exploration, realistic queries.

---

## 04-security

**Security features demonstration**

Shows security hardening:

- Read-only mode
- Disabled execute tool
- Limited tool exposure

```sql
PRAGMA mcp_server_start('stdio', '{
    "enable_query_tool": true,
    "enable_execute_tool": false,
    "enable_export_tool": false
}');
```

**Use case:** Production security patterns.

---

## 05-custom-tools

**Domain-specific tools using SQL and macros**

Demonstrates creating custom tools:

- Simple parameterized queries via `mcp_publish_tool`
- Multi-statement tools via `mcp_publish_execution_tool` (v2.0+)
- DuckDB macros as tool targets
- Multiple output formats

```sql
-- Single-statement tool backed by a macro
CREATE MACRO product_search(term) AS TABLE
    SELECT * FROM products WHERE name ILIKE '%' || term || '%';

PRAGMA mcp_publish_tool(
    'search',
    'Search products',
    'SELECT * FROM product_search($query)',
    '{"query": {"type": "string"}}',
    '["query"]',
    'markdown'
);

-- Multi-statement tool: stage a temp table, then report on it
PRAGMA mcp_publish_execution_tool(
    'category_report',
    'Build and report on products in a category',
    'CREATE OR REPLACE TEMP TABLE staged AS
        SELECT * FROM products WHERE category = $cat;
     SELECT name, price FROM staged ORDER BY price DESC',
    '{"cat": {"type": "string"}}',
    '["cat"]',
    '{"cat": "string"}',
    'markdown'
);
```

See the [Custom Tools guide](guides/custom-tools.md#multi-statement-tools) for when to reach for `mcp_publish_execution_tool` instead of the single-statement form.

**Use case:** Building domain-specific APIs.

---

## 06-comprehensive

**Full-featured SaaS application**

A complete SaaS-style database with:

- Multi-tenant schema
- Complex relationships
- Modular SQL files
- Views and macros
- Custom analytics tools

**Use case:** Enterprise application patterns.

---

## 07-devops

**DevOps and infrastructure data**

Demonstrates DuckDB's ability to work with:

- Git history analysis
- Test result parsing
- YAML configuration files
- OpenTelemetry data

**Use case:** DevOps dashboards, CI/CD analytics.

---

## 08-web-api

**Web API integration**

Shows integration with web services:

- HTTP client functions
- HTML parsing
- JSON schema handling
- Cloud storage access

**Use case:** API aggregation, web scraping.

---

## 09-docs

**Documentation system**

A documentation-focused example with:

- Markdown file processing
- Full-text search (FTS)
- Fuzzy matching
- Template rendering

**Use case:** Documentation search, knowledge bases.

---

## 10-developer

**Developer tools**

Advanced developer-focused features:

- AST parsing
- Test data generation
- Cryptographic functions
- Code analysis

**Use case:** Development workflows, code analysis.

---

## Using with Claude Desktop

Add any example to your Claude Desktop configuration:

=== "Linux"

    ```json
    {
      "mcpServers": {
        "duckdb-example": {
          "command": "duckdb",
          "args": ["-init", "/path/to/examples/03-with-data/init-mcp-server.sql"]
        }
      }
    }
    ```

=== "macOS"

    ```json
    {
      "mcpServers": {
        "duckdb-example": {
          "command": "/opt/homebrew/bin/duckdb",
          "args": ["-init", "/path/to/examples/03-with-data/init-mcp-server.sql"]
        }
      }
    }
    ```

---

## Creating Your Own

Use the examples as templates:

1. Copy an example directory
2. Modify `init-mcp-server.sql` with your schema and data
3. Add custom tools with `mcp_publish_tool()`
4. Update `mcp.json` for your path
5. Test with `test-calls.ldjson`

### Template

```sql
-- init-mcp-server.sql

-- Load extension
LOAD duckdb_mcp;

-- Create your schema
CREATE TABLE my_data (...);

-- Load data
COPY my_data FROM 'data.csv';

-- Create views
CREATE VIEW my_summary AS SELECT ...;

-- Publish custom tools (PRAGMA is silent — keeps init output clean)
PRAGMA mcp_publish_tool(
    'my_tool',
    'Description of what it does',
    'SELECT * FROM my_data WHERE field = $param',
    '{"param": {"type": "string", "description": "Parameter description"}}',
    '["param"]',
    'markdown'
);

-- Start server
PRAGMA mcp_server_start('stdio', '{
    "enable_execute_tool": false,
    "default_result_format": "markdown"
}');
```
