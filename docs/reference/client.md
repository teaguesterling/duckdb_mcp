# Client Functions Reference

These functions are used when DuckDB acts as an MCP client, connecting to external MCP servers.

## Connecting to MCP Servers

### ATTACH Statement

Attach an MCP server as a named connection:

```sql
-- Command as the ATTACH path (original syntax)
ATTACH '<command>' AS <server_name> (
    TYPE mcp,
    [TRANSPORT '<transport>'],
    [ARGS '<json_array>'],
    [CWD '<working_directory>'],
    [ENV '<json_object>']
);

-- Command as an explicit option (new syntax)
ATTACH '' AS <server_name> (
    TYPE mcp,
    COMMAND '<command>',
    [TRANSPORT '<transport>'],
    [ARGS '<json_array>'],
    [CWD '<working_directory>'],
    [ENV '<json_object>']
);
```

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `command` (path) | The executable to run, specified as the ATTACH path |
| `COMMAND` (option) | The executable to run, specified as an explicit option. Takes priority over the path. |
| `server_name` | Alias to reference this server in queries |
| `TRANSPORT` | Transport protocol: `stdio` (default), `tcp`, `websocket` |
| `ARGS` | JSON array of command-line arguments |
| `CWD` | Working directory for the server process |
| `ENV` | JSON object of environment variables |

!!! tip "COMMAND option vs path"
    The `COMMAND` option provides the same functionality as the ATTACH path but is more explicit. When both are provided, `COMMAND` takes priority. Use `COMMAND` when you want to keep the path slot empty or use it for a display name.

**Examples:**

```sql
-- Simple stdio server (command as path)
ATTACH 'python3' AS my_server (
    TYPE mcp,
    ARGS '["server.py"]'
);

-- Using explicit COMMAND option
ATTACH '' AS my_server (
    TYPE mcp,
    COMMAND 'python3',
    ARGS '["server.py"]'
);

-- With environment variables
ATTACH 'node' AS api_server (
    TYPE mcp,
    ARGS '["dist/server.js"]',
    ENV '{"NODE_ENV": "production", "API_KEY": "secret"}'
);

-- From configuration file
ATTACH 'my_server' AS server (
    TYPE mcp,
    FROM_CONFIG_FILE '/path/to/mcp.json'
);
```

---

## Resource Functions

### mcp_list_resources

List available resources from an MCP server.

```sql
mcp_list_resources(server_name [, cursor]) → VARCHAR (JSON)
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `server_name` | VARCHAR | Name of the attached MCP server |
| `cursor` | VARCHAR | Optional pagination cursor |

**Returns:** JSON object containing:

- `resources`: Array of resource objects with `uri`, `name`, `description`, `mimeType`
- `nextCursor`: Pagination cursor for next page (null if no more pages)

**Example:**

```sql
-- List all resources
SELECT mcp_list_resources('data_server');

-- With pagination
WITH pages AS (
    SELECT mcp_list_resources('data_server', '') as result
)
SELECT json_extract(result, '$.resources') as resources
FROM pages;
```

---

### mcp_get_resource

Retrieve the content of a specific resource.

```sql
mcp_get_resource(server_name, uri) → VARCHAR
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `server_name` | VARCHAR | Name of the attached MCP server |
| `uri` | VARCHAR | URI of the resource to retrieve |

**Returns:** Resource content as text.

**Example:**

```sql
SELECT mcp_get_resource('data_server', 'file:///config/settings.json');
```

---

### MCP URI Protocol

Use `mcp://` URIs with DuckDB's file readers:

```sql
-- Read CSV via MCP
SELECT * FROM read_csv('mcp://server_name/file:///path/to/data.csv');

-- Read Parquet via MCP
SELECT * FROM read_parquet('mcp://server_name/file:///data/table.parquet');

-- Read JSON via MCP
SELECT * FROM read_json('mcp://server_name/file:///api/response.json');
```

---

## Tool Functions

### mcp_list_tools

List available tools from an MCP server.

```sql
mcp_list_tools(server_name [, cursor]) → VARCHAR (JSON)
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `server_name` | VARCHAR | Name of the attached MCP server |
| `cursor` | VARCHAR | Optional pagination cursor |

**Returns:** JSON object containing:

- `tools`: Array of tool objects with `name`, `description`, `inputSchema`
- `nextCursor`: Pagination cursor for next page

**Example:**

```sql
SELECT mcp_list_tools('api_server');

-- Parse tool names
SELECT json_extract(mcp_list_tools('api_server'), '$.tools[*].name');
```

---

### mcp_call_tool

Execute a tool on an MCP server.

```sql
mcp_call_tool(server_name, tool_name, arguments) → VARCHAR (JSON)
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `server_name` | VARCHAR | Name of the attached MCP server |
| `tool_name` | VARCHAR | Name of the tool to execute |
| `arguments` | VARCHAR | JSON object of tool arguments |

**Returns:** JSON result from the tool execution.

**Example:**

```sql
-- Call a simple tool
SELECT mcp_call_tool('utils', 'format_date', '{"date": "2024-01-15", "format": "ISO"}');

-- Use tool result in a query
SELECT json_extract(
    mcp_call_tool('analytics', 'get_metrics', '{"period": "monthly"}'),
    '$.revenue'
) as monthly_revenue;
```

---

## Prompt Functions

### mcp_list_prompts

List available prompts from an MCP server.

```sql
mcp_list_prompts(server_name [, cursor]) → VARCHAR (JSON)
```

**Returns:** JSON object with `prompts` array and optional `nextCursor`.

---

### mcp_get_prompt

Retrieve and render a prompt from an MCP server.

```sql
mcp_get_prompt(server_name, prompt_name, arguments) → VARCHAR (JSON)
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `server_name` | VARCHAR | Name of the attached MCP server |
| `prompt_name` | VARCHAR | Name of the prompt |
| `arguments` | VARCHAR | JSON object of prompt arguments |

**Example:**

```sql
SELECT mcp_get_prompt('ai_server', 'code_review', '{"language": "python"}');
```

---

## Local Prompt Templates

Manage reusable prompt templates locally within DuckDB.

### mcp_register_prompt_template

Register a new prompt template.

```sql
mcp_register_prompt_template(name, description, content) → VARCHAR
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | VARCHAR | Unique template name |
| `description` | VARCHAR | Human-readable description |
| `content` | VARCHAR | Template content with `{variable}` placeholders |

**Example:**

```sql
SELECT mcp_register_prompt_template(
    'sql_query',
    'Generate a SQL query',
    'Write a SQL query to {action} from the {table} table where {condition}.'
);
```

---

### mcp_list_prompt_templates

List all registered local prompt templates.

```sql
mcp_list_prompt_templates() → VARCHAR (JSON)
```

---

### mcp_render_prompt_template

Render a prompt template with arguments.

```sql
mcp_render_prompt_template(name, arguments) → VARCHAR
```

**Parameters:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | VARCHAR | Template name |
| `arguments` | VARCHAR | JSON object mapping variable names to values |

**Example:**

```sql
SELECT mcp_render_prompt_template(
    'sql_query',
    '{"action": "count users", "table": "users", "condition": "created_at > ''2024-01-01''"}'
);
-- Returns: "Write a SQL query to count users from the users table where created_at > '2024-01-01'."
```

---

## Pagination

MCP supports cursor-based pagination for large result sets. Use recursive CTEs to iterate through all pages:

```sql
WITH RECURSIVE all_resources AS (
    -- First page
    SELECT
        1 as page,
        mcp_list_resources('server', '') as result

    UNION ALL

    -- Subsequent pages
    SELECT
        page + 1,
        mcp_list_resources('server', json_extract_string(result, '$.nextCursor'))
    FROM all_resources
    WHERE json_extract(result, '$.nextCursor') IS NOT NULL
      AND page < 100  -- Safety limit
)
SELECT
    page,
    json_array_length(json_extract(result, '$.resources')) as resource_count
FROM all_resources;
```

---

## Error Handling

MCP functions return error information in the JSON response when operations fail:

```sql
-- Check for errors
SELECT
    CASE
        WHEN json_extract(result, '$.error') IS NOT NULL
        THEN json_extract_string(result, '$.error.message')
        ELSE 'Success'
    END as status
FROM (SELECT mcp_call_tool('server', 'risky_operation', '{}') as result);
```
