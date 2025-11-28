-- Test script for MCP template functionality

-- Load the extension
LOAD 'duckdb_mcp';

-- Test 1: Register a simple template
SELECT 'Test 1: Registering a simple template' as test;
SELECT mcp_register_template('hello', 'A simple hello template', 'Hello, {name}!') as result;

-- Test 2: List available templates
SELECT 'Test 2: Listing templates' as test;
SELECT mcp_list_templates() as templates;

-- Test 3: Render template with arguments
SELECT 'Test 3: Rendering template with arguments' as test;
SELECT mcp_render_template('hello', '{"name": "World"}') as rendered;

-- Test 4: Register a more complex template
SELECT 'Test 4: Registering complex template' as test;
SELECT mcp_register_template(
    'git_commit', 
    'Generate git commit message', 
    'feat: {feature}

{description}

- {change1}
- {change2}

Closes #{issue}'
) as result;

-- Test 5: Render complex template
SELECT 'Test 5: Rendering complex template' as test;
SELECT mcp_render_template('git_commit', '{
    "feature": "add template support",
    "description": "Implement MCP template system with parameter substitution",
    "change1": "Add template registration and management",
    "change2": "Support fmt-based parameter rendering",
    "issue": "123"
}') as rendered;

-- Test 6: Test template with missing arguments
SELECT 'Test 6: Template with missing arguments' as test;
SELECT mcp_render_template('hello', '{}') as rendered;

-- Test 7: List all templates after registration
SELECT 'Test 7: All registered templates' as test;
SELECT mcp_list_templates() as all_templates;