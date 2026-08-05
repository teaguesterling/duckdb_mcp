# Creating Custom Tools

This guide covers creating custom SQL-based tools that MCP clients can execute.

## Overview

Custom tools let you expose domain-specific SQL queries as named operations that AI assistants can discover and call. Instead of requiring clients to write raw SQL, you provide:

- A descriptive **name** and **description**
- A **parameterized SQL template**
- A **JSON Schema** defining the parameters

## Basic Custom Tool

### Simple Example

```sql
PRAGMA mcp_publish_tool(
    'get_user',                                    -- Tool name
    'Retrieve user information by ID',             -- Description
    'SELECT * FROM users WHERE id = $id',          -- SQL template
    '{"id": {"type": "integer"}}',                 -- Parameter schema
    '["id"]'                                       -- Required parameters
);
```

The client calls it with:

```json
{
    "name": "get_user",
    "arguments": {"id": 42}
}
```

### Parameter Substitution

Parameters use `$name` syntax in the SQL template:

```sql
PRAGMA mcp_publish_tool(
    'search_products',
    'Search products by name and category',
    'SELECT * FROM products
     WHERE name ILIKE ''%'' || $query || ''%''
       AND category = $category',
    '{
        "query": {"type": "string", "description": "Search term"},
        "category": {"type": "string", "description": "Product category"}
    }',
    '["query", "category"]'
);
```

!!! note "SQL String Escaping"
    In SQL strings, use `''` for literal single quotes. The `$param` values are safely substituted.

### Table Function Arguments

`$param` works in table function arguments too — both positional and named. Many
extensions expose their functionality this way, and no special handling is needed:

```sql
-- Positional argument
PRAGMA mcp_publish_tool(
    'search_articles',
    'Full-text search across an archive',
    'SELECT title, snippet FROM zim_search(getenv(''ZIM_FILES''), $query)',
    '{"query": {"type": "string", "description": "Search term"}}',
    '["query"]'
);

-- Named argument
PRAGMA mcp_publish_tool(
    'search_articles_capped',
    'Full-text search with a caller-supplied result cap',
    'SELECT title FROM zim_search(getenv(''ZIM_FILES''), $query, max_results := $max_results)',
    '{
        "query": {"type": "string", "description": "Search term"},
        "max_results": {"type": "integer", "description": "Maximum rows to return"}
    }',
    '["query", "max_results"]'
);
```

A table function is bound from its argument *values*, so DuckDB re-binds the
statement on every call — each invocation sees the argument it was given.

!!! warning "Wrap optional parameters in COALESCE"
    An omitted optional parameter binds as SQL `NULL`, and table functions
    generally reject a NULL argument at bind time — sometimes with a confusing
    error, since the failure comes from the function, not from this extension.
    Give every optional parameter a default with `COALESCE`:

    ```sql
    'SELECT title FROM zim_search(getenv(''ZIM_FILES''), $query,
                                  max_results := COALESCE($max_results, 25))'
    ```

    `COALESCE` is folded to a constant before the table function is bound, so the
    function sees `25` when the caller omits the argument. Required parameters
    need no such wrapper.

## Parameter Schema

The parameter schema follows JSON Schema format:

### Basic Types

```json
{
    "id": {"type": "integer"},
    "name": {"type": "string"},
    "price": {"type": "number"},
    "active": {"type": "boolean"}
}
```

### With Descriptions

```json
{
    "start_date": {
        "type": "string",
        "description": "Start date in YYYY-MM-DD format"
    },
    "end_date": {
        "type": "string",
        "description": "End date in YYYY-MM-DD format"
    }
}
```

### With Enum Values

```json
{
    "status": {
        "type": "string",
        "enum": ["pending", "approved", "rejected"],
        "description": "Order status filter"
    }
}
```

## Required vs Optional Parameters

The fifth argument specifies which parameters are required:

```sql
PRAGMA mcp_publish_tool(
    'list_orders',
    'List orders with optional filters',
    'SELECT * FROM orders
     WHERE ($status IS NULL OR status = $status)
       AND ($min_amount IS NULL OR amount >= $min_amount)
     ORDER BY created_at DESC
     LIMIT COALESCE($limit, 50)',
    '{
        "status": {"type": "string", "description": "Filter by status"},
        "min_amount": {"type": "number", "description": "Minimum order amount"},
        "limit": {"type": "integer", "description": "Max results (default 50)"}
    }',
    '[]'  -- No required parameters
);
```

For required parameters:

```sql
'["customer_id", "start_date"]'  -- These must be provided
```

A property the SQL template never references is ignored — only the parameters the
statement actually uses are bound, so a schema stays valid while you edit the
template. The reverse is an error: a `$token` in the template with no matching
property fails the call with `Values were not provided for the following prepared
statement parameters: <name>`.

## Output Format

Specify the output format with the optional 6th parameter:

```sql
PRAGMA mcp_publish_tool(
    'sales_report',
    'Generate sales report',
    'SELECT region, SUM(amount) as total FROM sales GROUP BY region',
    '{}',
    '[]',
    'markdown'  -- Output as markdown table
);
```

**Supported formats:**

| Format | Best For |
|--------|----------|
| `json` | Programmatic processing (default) |
| `markdown` | AI assistants (token-efficient) |
| `csv` | Data export |

## Advanced Patterns

### Aggregation Tools

```sql
PRAGMA mcp_publish_tool(
    'revenue_summary',
    'Get revenue summary by time period',
    'SELECT
        DATE_TRUNC($period, order_date) as period,
        COUNT(*) as order_count,
        SUM(amount) as total_revenue,
        AVG(amount) as avg_order_value
     FROM orders
     WHERE order_date >= $start_date
       AND order_date < $end_date
     GROUP BY 1
     ORDER BY 1',
    '{
        "period": {
            "type": "string",
            "enum": ["day", "week", "month", "quarter", "year"],
            "description": "Aggregation period"
        },
        "start_date": {"type": "string", "description": "Start date"},
        "end_date": {"type": "string", "description": "End date"}
    }',
    '["period", "start_date", "end_date"]',
    'markdown'
);
```

### Search with LIKE Patterns

```sql
PRAGMA mcp_publish_tool(
    'search_customers',
    'Search customers by name or email',
    'SELECT id, name, email, created_at
     FROM customers
     WHERE name ILIKE ''%'' || $query || ''%''
        OR email ILIKE ''%'' || $query || ''%''
     ORDER BY name
     LIMIT $limit',
    '{
        "query": {"type": "string", "description": "Search term"},
        "limit": {"type": "integer", "description": "Max results (default 20)"}
    }',
    '["query"]'
);
```

### Join Queries

```sql
PRAGMA mcp_publish_tool(
    'order_details',
    'Get full order details including items and customer',
    'SELECT
        o.id as order_id,
        o.order_date,
        c.name as customer_name,
        c.email,
        oi.product_name,
        oi.quantity,
        oi.unit_price,
        oi.quantity * oi.unit_price as line_total
     FROM orders o
     JOIN customers c ON o.customer_id = c.id
     JOIN order_items oi ON o.id = oi.order_id
     WHERE o.id = $order_id',
    '{"order_id": {"type": "integer", "description": "Order ID"}}',
    '["order_id"]',
    'markdown'
);
```

### Computed Analytics

```sql
PRAGMA mcp_publish_tool(
    'customer_lifetime_value',
    'Calculate customer lifetime value metrics',
    'SELECT
        c.id,
        c.name,
        COUNT(DISTINCT o.id) as total_orders,
        SUM(o.amount) as lifetime_value,
        MIN(o.order_date) as first_order,
        MAX(o.order_date) as last_order,
        DATEDIFF(''day'', MIN(o.order_date), MAX(o.order_date)) as customer_tenure_days
     FROM customers c
     LEFT JOIN orders o ON c.id = o.customer_id
     WHERE c.id = $customer_id
     GROUP BY c.id, c.name',
    '{"customer_id": {"type": "integer"}}',
    '["customer_id"]'
);
```

## Using DuckDB Macros

For complex tools, you can use DuckDB macros:

```sql
-- Define the macro
CREATE OR REPLACE MACRO product_search(search_term, category, max_price) AS TABLE
    SELECT *
    FROM products
    WHERE name ILIKE '%' || search_term || '%'
      AND (category IS NULL OR products.category = category)
      AND (max_price IS NULL OR price <= max_price)
    ORDER BY price;

-- Publish as a tool
PRAGMA mcp_publish_tool(
    'product_search',
    'Search products with filters',
    'SELECT * FROM product_search($query, $category, $max_price)',
    '{
        "query": {"type": "string", "description": "Search term"},
        "category": {"type": "string", "description": "Category filter (optional)"},
        "max_price": {"type": "number", "description": "Max price filter (optional)"}
    }',
    '["query"]',
    'markdown'
);
```

## Multi-Statement Tools

`mcp_publish_tool` runs a single SQL statement. When you need to run several statements in sequence — set a session variable, create a temp table, then query it — use `mcp_publish_execution_tool` instead. It takes one extra argument, `bindings`, that declares how each tool argument maps to prepared-statement parameters per statement. The result of the last statement is what the tool returns.

### When to Use It

Reach for `mcp_publish_execution_tool` when your tool needs any of:

- `SET VARIABLE` followed by a query that reads it via `getvariable()`
- `CREATE TEMP TABLE` / `CREATE TEMP VIEW` followed by a query against it
- A sequence of DDL or pragma statements that prepare state for a final `SELECT`

For everything else, stick with `mcp_publish_tool` — it's simpler and the single-statement path has less ceremony.

### SET VARIABLE Example

```sql
PRAGMA mcp_publish_execution_tool(
    'high_value_sales',
    'List sales above a configurable threshold',
    'SET VARIABLE min_amount = $threshold;
     SELECT * FROM sales
      WHERE amount > getvariable(''min_amount'')
      ORDER BY amount DESC',
    '{"threshold": {"type": "number", "description": "Minimum sale amount"}}',
    '["threshold"]',
    '[{}, {"threshold": "number"}]'  -- first stmt needs no params, second binds $threshold
);
```

### Temp Table Staging Example

```sql
PRAGMA mcp_publish_execution_tool(
    'category_price_report',
    'Build and report on products in a category',
    'CREATE OR REPLACE TEMP TABLE staged AS
        SELECT * FROM products WHERE category = $cat;
     SELECT name, price,
            price - AVG(price) OVER () as price_vs_avg
       FROM staged
      ORDER BY price DESC',
    '{"cat": {"type": "string", "description": "Product category"}}',
    '["cat"]',
    '{"cat": "string"}',  -- object form: bind $cat in every statement that references it
    'markdown'
);
```

### Bindings: Object vs Array Form

The `bindings` argument tells the execution engine how to prepare each statement. Two shapes are supported:

- **Object form** `{"q": "string"}` — apply the same parameter types to every statement. Simplest; use when every statement references the same parameters.
- **Array form** `[{}, {"q": "string"}]` — one entry per statement, in order. Use when only some statements reference parameters (e.g., a `SET VARIABLE` followed by a query that uses `getvariable()` — the first statement binds `$threshold` directly, the second doesn't bind anything new).

The array length must match the number of semicolon-separated statements in your template.

!!! note "Parameter types"
    Binding type names (`"string"`, `"integer"`, `"number"`, `"boolean"`) are the DuckDB-side types used when preparing each statement. They should match the JSON Schema `type` values in your `properties` argument.

!!! tip "Falling back to interpolation"
    If a statement can't be prepared (for example, a macro invocation), the execution tool falls back to safe string interpolation for that statement. You generally don't need to think about this — but it means the function works with macro-based tools as well.

See [`mcp_publish_execution_tool` in the server reference](../reference/server.md#mcp_publish_execution_tool) for the complete argument list.

## Security Considerations

### Input Validation

Parameters are substituted safely (not concatenated), but validate at the SQL level:

```sql
PRAGMA mcp_publish_tool(
    'get_item',
    'Get item by ID',
    'SELECT * FROM items WHERE id = CAST($id AS INTEGER) LIMIT 1',
    '{"id": {"type": "string"}}',
    '["id"]'
);
```

### Limiting Results

Always include reasonable limits:

```sql
'SELECT * FROM large_table WHERE ... LIMIT LEAST($limit, 1000)'
```

### Read-Only Operations

Custom tools should generally be read-only. Avoid:

- `INSERT`, `UPDATE`, `DELETE` statements
- Calls to functions with side effects

If you need write operations, carefully consider who can call the tool.

## Testing Custom Tools

Use the memory transport to test tools:

```sql
-- Start test server
PRAGMA mcp_server_start('memory');

-- Publish tool
PRAGMA mcp_publish_tool('test_tool', 'Test', 'SELECT $x + $y as sum',
    '{"x": {"type": "integer"}, "y": {"type": "integer"}}', '["x", "y"]');

-- Initialize
SELECT mcp_server_send_request('{
    "jsonrpc": "2.0", "id": 1, "method": "initialize",
    "params": {"protocolVersion": "2024-11-05", "clientInfo": {"name": "test"}, "capabilities": {}}
}');

-- List tools (should include test_tool)
SELECT mcp_server_send_request('{"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}}');

-- Call the tool
SELECT mcp_server_send_request('{
    "jsonrpc": "2.0", "id": 3, "method": "tools/call",
    "params": {"name": "test_tool", "arguments": {"x": 2, "y": 3}}
}');
```

## Best Practices

1. **Use descriptive names** - AI assistants use names to understand what tools do

2. **Write clear descriptions** - Include what the tool returns and any constraints

3. **Document parameters** - Use the `description` field in the schema

4. **Use markdown output** for tools designed for AI assistants

5. **Include sensible defaults** for optional parameters using `COALESCE`

6. **Test with realistic inputs** before deploying

7. **Group related tools** by naming convention (e.g., `orders_list`, `orders_detail`, `orders_summary`)
