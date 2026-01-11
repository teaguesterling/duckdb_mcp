# Using DuckDB as an MCP Client

This guide covers connecting DuckDB to external MCP servers to access their resources and tools.

## Overview

When acting as an MCP client, DuckDB can:

- Connect to MCP servers running as subprocesses
- Read resources (files, data) from those servers
- Execute tools provided by the servers
- Use MCP resources directly in SQL queries

## Connecting to an MCP Server

### Basic Connection

Use the `ATTACH` statement to connect to an MCP server:

```sql
LOAD 'duckdb_mcp';

ATTACH 'python3' AS data_server (
    TYPE mcp,
    ARGS '["path/to/server.py"]'
);
```

This starts `python3 path/to/server.py` as a subprocess and communicates via stdio.

### Connection Options

```sql
ATTACH 'node' AS api_server (
    TYPE mcp,
    TRANSPORT 'stdio',           -- Transport type (stdio is default)
    ARGS '["dist/server.js", "--port", "3000"]',  -- Command arguments
    CWD '/opt/myproject',        -- Working directory
    ENV '{"NODE_ENV": "production", "API_KEY": "secret"}'  -- Environment variables
);
```

### Using Configuration Files

For complex setups, define servers in a JSON file:

```json
{
  "mcpServers": {
    "analytics": {
      "command": "python3",
      "args": ["analytics_server.py"],
      "cwd": "/opt/analytics",
      "env": {"DATABASE_URL": "postgresql://..."}
    }
  }
}
```

Then attach by name:

```sql
ATTACH 'analytics' AS analytics (
    TYPE mcp,
    FROM_CONFIG_FILE '/path/to/mcp-config.json'
);
```

## Accessing Resources

### Listing Resources

```sql
-- Get all resources as JSON
SELECT mcp_list_resources('data_server');

-- Parse resource URIs
SELECT json_extract(mcp_list_resources('data_server'), '$.resources[*].uri');
```

### Reading Resource Content

```sql
-- Get raw content
SELECT mcp_get_resource('data_server', 'file:///data/config.json');

-- Parse JSON content
SELECT json_extract(
    mcp_get_resource('data_server', 'file:///data/config.json'),
    '$.database.host'
);
```

### Using MCP URIs with DuckDB Functions

The most powerful feature is using `mcp://` URIs with DuckDB's file readers:

```sql
-- Read CSV through MCP
SELECT * FROM read_csv('mcp://data_server/file:///data/sales.csv');

-- Read Parquet through MCP
SELECT * FROM read_parquet('mcp://data_server/file:///warehouse/orders.parquet');

-- Read JSON through MCP
SELECT * FROM read_json('mcp://api_server/api://users');

-- Join data from multiple MCP servers
SELECT
    s.order_id,
    s.amount,
    c.customer_name
FROM read_csv('mcp://sales_server/file:///orders.csv') s
JOIN read_csv('mcp://crm_server/file:///customers.csv') c
    ON s.customer_id = c.id;
```

## Calling Tools

### Listing Available Tools

```sql
SELECT mcp_list_tools('data_server');
```

Example response:

```json
{
  "tools": [
    {
      "name": "analyze_data",
      "description": "Run statistical analysis on a dataset",
      "inputSchema": {
        "type": "object",
        "properties": {
          "dataset": {"type": "string"},
          "metrics": {"type": "array", "items": {"type": "string"}}
        },
        "required": ["dataset"]
      }
    }
  ]
}
```

### Executing Tools

```sql
-- Simple tool call
SELECT mcp_call_tool('data_server', 'analyze_data', '{"dataset": "sales"}');

-- Tool with multiple arguments
SELECT mcp_call_tool('data_server', 'analyze_data', '{
    "dataset": "sales",
    "metrics": ["mean", "median", "std"]
}');

-- Use tool results in SQL
SELECT json_extract(
    mcp_call_tool('data_server', 'get_stats', '{"table": "orders"}'),
    '$.total_revenue'
) as revenue;
```

## Working with Prompts

### Listing Server Prompts

```sql
SELECT mcp_list_prompts('ai_server');
```

### Getting and Rendering Prompts

```sql
SELECT mcp_get_prompt('ai_server', 'code_review', '{
    "language": "sql",
    "focus": "performance"
}');
```

## Local Prompt Templates

You can create reusable prompt templates locally:

```sql
-- Register a template
SELECT mcp_register_prompt_template(
    'data_analysis',
    'Template for data analysis requests',
    'Analyze the {table} table, focusing on {metric}. Look for trends in {time_period}.'
);

-- List templates
SELECT mcp_list_prompt_templates();

-- Render with arguments
SELECT mcp_render_prompt_template('data_analysis', '{
    "table": "sales",
    "metric": "revenue growth",
    "time_period": "Q4 2024"
}');
-- Returns: "Analyze the sales table, focusing on revenue growth. Look for trends in Q4 2024."
```

## Pagination

MCP servers may paginate large result sets. Handle pagination with cursors:

```sql
-- Get first page
SELECT mcp_list_resources('data_server', '') as first_page;

-- Get next page using cursor
WITH page1 AS (
    SELECT mcp_list_resources('data_server', '') as result
)
SELECT mcp_list_resources(
    'data_server',
    json_extract_string(result, '$.nextCursor')
) as page2
FROM page1
WHERE json_extract(result, '$.nextCursor') IS NOT NULL;
```

### Iterating All Pages

Use a recursive CTE to process all pages:

```sql
WITH RECURSIVE all_pages AS (
    SELECT 1 as page_num, mcp_list_resources('data_server', '') as result

    UNION ALL

    SELECT
        page_num + 1,
        mcp_list_resources('data_server', json_extract_string(result, '$.nextCursor'))
    FROM all_pages
    WHERE json_extract(result, '$.nextCursor') IS NOT NULL
      AND page_num < 100
)
SELECT
    page_num,
    json_extract(result, '$.resources') as resources
FROM all_pages;
```

## Error Handling

Check for errors in responses:

```sql
WITH result AS (
    SELECT mcp_call_tool('server', 'risky_tool', '{}') as response
)
SELECT
    CASE
        WHEN json_extract(response, '$.error') IS NOT NULL THEN
            'Error: ' || json_extract_string(response, '$.error.message')
        ELSE
            json_extract_string(response, '$.content[0].text')
    END as result
FROM result;
```

## Best Practices

1. **Use configuration files** for complex server setups rather than inline ATTACH options

2. **Prefer MCP URIs** with `read_csv`, `read_parquet`, etc. over `mcp_get_resource` for structured data

3. **Handle pagination** for any list operation that might return large result sets

4. **Cache frequently accessed resources** by materializing them:

   ```sql
   CREATE TABLE local_products AS
   SELECT * FROM read_csv('mcp://server/file:///products.csv');
   ```

5. **Use prompt templates** for reusable prompts to avoid duplication
