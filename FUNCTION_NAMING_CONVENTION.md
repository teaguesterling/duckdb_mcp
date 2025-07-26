# MCP Function Naming Convention

## Overview

The DuckDB MCP extension follows a clear naming convention that distinguishes between **client operations** (when DuckDB acts as an MCP client) and **server operations** (when DuckDB manages local content or acts as an MCP server).

## Naming Pattern

### Client Operations (talking to MCP servers)
When DuckDB acts as a client connecting to external MCP servers, functions use the MCP specification terminology:

- `mcp_list_prompts(server_name)` - List prompts available on a remote MCP server
- `mcp_get_prompt(server_name, prompt_name, arguments)` - Get and render a prompt from a remote MCP server
- `mcp_list_resources(server_name)` - List resources from a remote MCP server
- `mcp_get_resource(server_name, resource_uri)` - Get a resource from a remote MCP server
- `mcp_call_tool(server_name, tool_name, arguments)` - Call a tool on a remote MCP server

### Server Operations (managing local content)
When DuckDB manages local content or acts as a server, functions use descriptive names that clarify they operate on local entities:

- `mcp_register_prompt_template(name, description, content)` - Register a prompt template locally
- `mcp_list_prompt_templates()` - List all local prompt templates
- `mcp_render_prompt_template(name, arguments)` - Render a local prompt template
- `mcp_server_start()`, `mcp_server_stop()`, `mcp_server_status()` - Manage the local MCP server
- `mcp_publish_table()`, `mcp_publish_query()` - Publish local data as MCP resources

## Rationale

This naming convention provides several benefits:

1. **Semantic Clarity**: Users can immediately understand whether a function operates on local data or communicates with remote servers
2. **MCP Compliance**: Client functions use terminology directly from the MCP specification ("prompts", "resources", "tools")
3. **Future Extensibility**: Sets up for future SQL syntax like `CREATE MCP_PROMPT`, `CREATE MCP_TOOL`, `CREATE MCP_RESOURCE`
4. **Consistency**: Follows established patterns already present in the codebase

## Function Categories

### 🌐 Client Functions (External MCP Servers)
```sql
-- These functions communicate with attached MCP servers
SELECT mcp_list_prompts('ai_server');
SELECT mcp_get_prompt('ai_server', 'code_review', '{"lang": "python"}');
SELECT mcp_list_resources('data_server'); 
SELECT mcp_get_resource('data_server', 'file:///data.csv');
SELECT mcp_call_tool('util_server', 'formatter', '{"code": "..."}');
```

### 🏠 Server Functions (Local Management)
```sql
-- These functions manage local content and services
SELECT mcp_register_prompt_template('greeting', 'Say hello', 'Hello {name}!');
SELECT mcp_list_prompt_templates();
SELECT mcp_render_prompt_template('greeting', '{"name": "Alice"}');
SELECT mcp_server_start('stdio', 'localhost', 0, '{}');
SELECT mcp_publish_table('users', 'data://tables/users', 'json');
```

## Migration Guide

If you have existing code using the old template function names, update as follows:

### Old Names → New Names
```sql
-- OLD (inconsistent)
mcp_register_template() → mcp_register_prompt_template()
mcp_list_templates() → mcp_list_prompt_templates()  
mcp_render_template() → mcp_render_prompt_template()
mcp_list_server_templates() → mcp_list_prompts()
mcp_get_server_template() → mcp_get_prompt()

-- EXAMPLE MIGRATION
-- Before:
SELECT mcp_register_template('query', 'SQL template', 'SELECT {cols} FROM {table}');
SELECT mcp_list_server_templates('ai_server');

-- After:
SELECT mcp_register_prompt_template('query', 'SQL template', 'SELECT {cols} FROM {table}');
SELECT mcp_list_prompts('ai_server');
```

## Future Enhancements

This naming convention supports future SQL syntax enhancements:

```sql
-- Potential future syntax
CREATE MCP_PROMPT greeting AS 'Hello {name}, welcome to {location}!';
CREATE MCP_TOOL formatter USING 'python3 /tools/format.py';
CREATE MCP_RESOURCE users AS SELECT * FROM users_table;

-- These would integrate with existing functions
SELECT mcp_render_prompt_template('greeting', '{"name": "Alice", "location": "DuckDB"}');
```

## Best Practices

1. **Use descriptive prompt template names**: `sql_query_builder`, `email_template`, `code_review_prompt`
2. **Organize by purpose**: Group related templates with prefixes like `email_*`, `sql_*`, `analysis_*`
3. **Document parameters**: Include parameter descriptions in template descriptions
4. **Test with real servers**: Verify prompt compatibility with target MCP servers
5. **Version control**: Keep prompt templates in version control for team collaboration

This naming convention ensures the DuckDB MCP extension remains intuitive, consistent, and ready for future enhancements while maintaining full compatibility with the MCP specification.