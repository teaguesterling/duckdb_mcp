-- MCP Templates Examples
-- This file demonstrates the complete MCP template functionality

-- Load the extension (use appropriate path for your setup)
-- LOAD 'build/release/extension/duckdb_mcp/duckdb_mcp.duckdb_extension';

-- =============================================================================
-- Basic Template Operations
-- =============================================================================

-- 1. Register basic templates
SELECT 'Registering basic templates...' as step;

SELECT mcp_register_template(
    'greeting',
    'A simple greeting template',
    'Hello {name}, welcome to {location}!'
) as result;

SELECT mcp_register_template(
    'sql_query',
    'SQL query template with parameters',
    'SELECT {columns} FROM {table} WHERE {condition} ORDER BY {order_by} LIMIT {limit};'
) as result;

SELECT mcp_register_template(
    'email',
    'Professional email template',
    'Subject: {subject}\n\nDear {recipient},\n\n{body}\n\nBest regards,\n{sender}\n{signature}'
) as result;

-- 2. List all registered templates
SELECT 'Listing all templates...' as step;
SELECT mcp_list_templates() as templates;

-- =============================================================================
-- Template Rendering Examples
-- =============================================================================

-- 3. Render simple greeting
SELECT 'Rendering greeting template...' as step;
SELECT mcp_render_template(
    'greeting',
    '{"name": "Alice", "location": "DuckDB Database"}'
) as rendered_greeting;

-- 4. Render SQL query template
SELECT 'Rendering SQL query template...' as step;
SELECT mcp_render_template(
    'sql_query',
    '{"columns": "id, name, age", "table": "users", "condition": "age >= 18", "order_by": "name", "limit": "100"}'
) as rendered_query;

-- 5. Render email template
SELECT 'Rendering email template...' as step;
SELECT mcp_render_template(
    'email',
    '{"subject": "Project Update", "recipient": "John Smith", "body": "I wanted to update you on the progress of our database integration project.", "sender": "Alice Johnson", "signature": "Senior Database Engineer"}'
) as rendered_email;

-- =============================================================================
-- Advanced Template Examples
-- =============================================================================

-- 6. Code generation template
SELECT 'Registering code generation template...' as step;
SELECT mcp_register_template(
    'python_function',
    'Python function generator',
    'def {function_name}({parameters}):\n    """{docstring}"""\n    {implementation}\n    return {return_value}'
) as result;

SELECT mcp_render_template(
    'python_function',
    '{"function_name": "calculate_average", "parameters": "numbers: list", "docstring": "Calculate the average of a list of numbers", "implementation": "total = sum(numbers)\n    count = len(numbers)", "return_value": "total / count if count > 0 else 0"}'
) as python_code;

-- 7. Documentation template
SELECT 'Registering documentation template...' as step;
SELECT mcp_register_template(
    'api_endpoint',
    'REST API endpoint documentation',
    '# {endpoint_name}\n\n**URL:** `{method} {path}`\n\n**Description:** {description}\n\n## Request Parameters\n{parameters}\n\n## Response\n```json\n{response_example}\n```\n\n## Example Usage\n```bash\ncurl -X {method} "{base_url}{path}" {curl_options}\n```'
) as result;

SELECT mcp_render_template(
    'api_endpoint',
    '{"endpoint_name": "Get User Profile", "method": "GET", "path": "/api/users/{id}", "description": "Retrieve user profile information by user ID", "parameters": "- id (integer): User ID in the path", "response_example": "{\n  \"id\": 123,\n  \"name\": \"John Doe\",\n  \"email\": \"john@example.com\"\n}", "base_url": "https://api.example.com", "curl_options": "-H \"Authorization: Bearer TOKEN\""}'
) as api_doc;

-- =============================================================================
-- Error Handling Examples
-- =============================================================================

-- 8. Test error handling
SELECT 'Testing error conditions...' as step;

-- Try to render non-existent template
SELECT mcp_render_template(
    'nonexistent_template',
    '{"param": "value"}'
) as error_test_1;

-- Try to render with invalid JSON
SELECT mcp_render_template(
    'greeting',
    'invalid json'
) as error_test_2;

-- =============================================================================
-- Server Template Examples (these will fail gracefully if no server attached)
-- =============================================================================

-- 9. Test server template functions
SELECT 'Testing server template functions...' as step;

-- These will return error messages if no server is attached, which is expected
SELECT mcp_list_server_templates('test_server') as server_templates;

SELECT mcp_get_server_template(
    'test_server',
    'example_template',
    '{"param1": "value1", "param2": "value2"}'
) as server_template;

-- Test with NULL parameters
SELECT mcp_list_server_templates(NULL) as null_server_test;
SELECT mcp_get_server_template(NULL, 'template', '{}') as null_test_1;
SELECT mcp_get_server_template('server', NULL, '{}') as null_test_2;

-- =============================================================================
-- Complex Template Examples
-- =============================================================================

-- 10. Multi-step data analysis template
SELECT 'Creating data analysis template...' as step;
SELECT mcp_register_template(
    'data_analysis',
    'Generate data analysis steps',
    'Data Analysis Plan for {dataset}\n\n1. Data Loading\n   - Source: {data_source}\n   - Format: {data_format}\n\n2. Data Cleaning\n   - Handle missing values: {missing_strategy}\n   - Remove duplicates: {dedup_strategy}\n\n3. Analysis Methods\n   - Primary analysis: {analysis_type}\n   - Key metrics: {metrics}\n   - Grouping: {group_by}\n\n4. Visualization\n   - Chart types: {chart_types}\n   - Key insights: {insights_focus}\n\n5. Expected Outcomes\n   {expected_outcomes}'
) as result;

SELECT mcp_render_template(
    'data_analysis',
    '{"dataset": "Sales Performance Q4 2023", "data_source": "PostgreSQL database", "data_format": "CSV exports", "missing_strategy": "interpolation for numeric, mode for categorical", "dedup_strategy": "remove exact duplicates, flag near-duplicates", "analysis_type": "trend analysis and comparative study", "metrics": "revenue, customer acquisition, retention rate", "group_by": "region, product category, time period", "chart_types": "line charts for trends, bar charts for comparisons", "insights_focus": "seasonal patterns, top-performing regions", "expected_outcomes": "Identify key growth drivers and optimization opportunities"}'
) as analysis_plan;

-- =============================================================================
-- Template with Dynamic SQL Generation
-- =============================================================================

-- 11. Database schema documentation template
SELECT 'Creating schema documentation template...' as step;
SELECT mcp_register_template(
    'table_doc',
    'Database table documentation generator',
    '# {table_name} Table\n\n**Database:** {database_name}\n**Schema:** {schema_name}\n\n## Description\n{description}\n\n## Columns\n{columns_info}\n\n## Indexes\n{indexes_info}\n\n## Relationships\n{relationships}\n\n## Example Queries\n```sql\n-- Basic select\nSELECT * FROM {schema_name}.{table_name} LIMIT 10;\n\n-- Common operations\n{example_queries}\n```\n\n## Notes\n{additional_notes}'
) as result;

-- =============================================================================
-- Summary
-- =============================================================================

SELECT 'Template examples completed!' as status;
SELECT 'Templates registered: ' || count(*) as summary
FROM (SELECT mcp_list_templates() as templates) t;