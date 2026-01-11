# MCP Prompt Templates Support

The DuckDB MCP extension now supports Model Context Protocol (MCP) prompt templates, allowing you to create reusable prompt templates with parameter substitution and integrate with MCP servers that provide prompts.

## Overview

MCP prompt templates enable:
- **Local prompt template management**: Register, list, and render prompt templates within DuckDB
- **Server prompt integration**: Retrieve and use prompts from attached MCP servers
- **Parameter substitution**: Use `{variable}` syntax for dynamic content
- **Validation**: Ensure required parameters are provided

## Prompt Template Functions

### Local Prompt Template Management

#### `mcp_register_prompt_template(name, description, template_content)`
Register a new prompt template locally.

```sql
SELECT mcp_register_prompt_template(
    'greeting', 
    'A friendly greeting template',
    'Hello {name}, welcome to {location}! Today is {date}.'
) as result;
```

#### `mcp_list_prompt_templates()`
List all registered local prompt templates.

```sql
SELECT mcp_list_prompt_templates() as templates;
```

#### `mcp_render_prompt_template(template_name, arguments_json)`
Render a prompt template with provided arguments.

```sql
SELECT mcp_render_prompt_template(
    'greeting',
    '{"name": "Alice", "location": "DuckDB", "date": "2024-01-15"}'
) as rendered_text;
```

### Server Prompt Integration

#### `mcp_list_prompts(server_name)`
List prompts available from an attached MCP server.

```sql
-- First attach an MCP server
SELECT mcp_server_start('my_server', 'path/to/server', 3000, '') as status;

-- Then list available prompts
SELECT mcp_list_prompts('my_server') as server_prompts;
```

#### `mcp_get_prompt(server_name, prompt_name, arguments_json)`
Retrieve and render a prompt from an MCP server.

```sql
SELECT mcp_get_prompt(
    'my_server',
    'code_review',
    '{"language": "python", "style": "detailed"}'
) as server_prompt;
```

## Template Syntax

Templates use simple `{variable}` syntax for parameter substitution:

```sql
-- Register a template with multiple parameters
SELECT mcp_register_prompt_template(
    'sql_query',
    'Generate a SQL query template',
    'SELECT {columns} FROM {table} WHERE {condition} LIMIT {limit};'
);

-- Render with specific values
SELECT mcp_render_prompt_template(
    'sql_query',
    '{"columns": "name, age", "table": "users", "condition": "age > 18", "limit": "100"}'
);
```

### Required vs Optional Parameters

Templates can have required and optional parameters:

```sql
-- Register template with validation
SELECT mcp_register_prompt_template(
    'email',
    'Email template with required recipient',
    'To: {recipient}\nSubject: {subject}\n\nDear {recipient},\n\n{body}\n\nBest regards,\n{sender}'
);

-- This will work if all parameters provided
SELECT mcp_render_prompt_template(
    'email',
    '{"recipient": "alice@example.com", "subject": "Hello", "body": "How are you?", "sender": "Bob"}'
);
```

## Integration with MCP Servers

### Workflow Example

1. **Start an MCP server** that provides templates:
```sql
SELECT mcp_server_start(
    'prompt_server',
    '/path/to/mcp-prompt-server',
    3000,
    ''
) as status;
```

2. **List available prompts** from the server:
```sql
SELECT mcp_list_prompts('prompt_server') as available_prompts;
```

3. **Use a server prompt**:
```sql
SELECT mcp_get_prompt(
    'prompt_server',
    'code_review_template',
    '{"language": "typescript", "complexity": "intermediate"}'
) as review_prompt;
```

### Error Handling

The template functions provide helpful error messages:

```sql
-- Missing server
SELECT mcp_list_server_templates('nonexistent_server');
-- Returns: "Error: MCP server not attached: nonexistent_server"

-- Missing required parameter (if template requires it)
SELECT mcp_render_template('greeting', '{"name": "Alice"}');
-- May return: "Error: Missing required argument: location"
```

## Use Cases

### 1. Code Generation Templates
```sql
-- Register a function template
SELECT mcp_register_template(
    'function_stub',
    'Generate a function stub',
    'def {function_name}({parameters}):\n    """{description}"""\n    {implementation}'
);

-- Use it
SELECT mcp_render_template(
    'function_stub',
    '{"function_name": "calculate_total", "parameters": "items, tax_rate", "description": "Calculate total with tax", "implementation": "return sum(items) * (1 + tax_rate)"}'
);
```

### 2. Documentation Templates
```sql
SELECT mcp_register_template(
    'api_doc',
    'API endpoint documentation',
    '# {endpoint}\n\n**Method**: {method}\n**Description**: {description}\n\n## Parameters\n{parameters}\n\n## Example\n```\n{example}\n```'
);
```

### 3. Data Analysis Prompts
```sql
-- Use server templates for AI analysis
SELECT mcp_get_server_template(
    'analysis_server',
    'data_insights',
    '{"dataset": "sales_data", "metric": "revenue", "timeframe": "quarterly"}'
) as analysis_prompt;
```

## Best Practices

1. **Use descriptive names**: Choose clear, descriptive names for your templates
2. **Document parameters**: Include parameter descriptions in template descriptions
3. **Validate inputs**: Test templates with various parameter combinations
4. **Handle missing data**: Design templates to work gracefully with missing optional parameters
5. **Version control**: Keep template definitions in version control for team collaboration

## Integration with Existing MCP Functions

Templates work seamlessly with other MCP extension features:

```sql
-- Use templates with tool calls
SELECT mcp_call_tool(
    'ai_server',
    'generate_code',
    mcp_render_template('code_prompt', '{"language": "sql", "task": "data aggregation"}')
);

-- Store template results as resources
SELECT mcp_publish_query(
    mcp_render_template('report_query', '{"date": "2024-01-15"}'),
    'data://reports/daily',
    'json',
    3600
);
```

This completes the MCP templates documentation, providing users with comprehensive guidance on using the new template functionality.