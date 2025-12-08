# Example 10: Developer MCP Server

A DuckDB MCP server optimized for code analysis, AST parsing, test data generation, and developer tooling workflows.

## Features

- **AST Parsing**: Parse SQL and other languages into queryable syntax trees
- **Git Integration**: Query git history and file versions
- **Test Result Parsing**: Parse 44+ test/build output formats
- **PRQL Support**: Write queries in PRQL syntax
- **Cryptographic Functions**: Hash, HMAC, and encryption utilities
- **Test Data Generation**: Generate realistic fake data
- **Code Metrics**: Analyze source code structure

## Extensions Used

| Extension | Type | Purpose |
|-----------|------|---------|
| sitting_duck | Community | SQL/language AST parsing |
| duck_tails | Community | Git history via git:// protocol |
| duck_hunt | Community | Test result parsing (44+ formats) |
| parser_tools | Community | Additional parsing utilities |
| prql | Community | PRQL query language support |
| hashfuncs | Community | SHA, BLAKE3, xxHash functions |
| crypto | Community | Encryption/HMAC functions |
| fakeit | Community | Fake data generation |
| json | Core | JSON processing |

## Quick Start

```bash
cd examples/10-developer
chmod +x launch-mcp.sh
./launch-mcp.sh
```

## Example Queries

### SQL AST Parsing
```sql
-- Parse a SQL query into AST
SELECT * FROM parse_sql('SELECT id, name FROM users WHERE active = true');

-- Extract table references
SELECT table_name
FROM parse_sql('SELECT * FROM orders o JOIN customers c ON o.customer_id = c.id')
WHERE node_type = 'table_ref';

-- Find all column references
SELECT column_name, table_alias
FROM parse_sql('SELECT u.name, o.total FROM users u JOIN orders o ON u.id = o.user_id')
WHERE node_type = 'column_ref';
```

### Git History Analysis
```sql
-- Query file at specific commit
SELECT * FROM 'git://src/main.py@abc123';

-- Get commit history
SELECT * FROM git_log('.')
ORDER BY commit_time DESC
LIMIT 20;

-- Compare file versions
SELECT * FROM 'git://config.yaml@HEAD~5'
EXCEPT
SELECT * FROM 'git://config.yaml@HEAD';
```

### Test Result Parsing
```sql
-- Parse JUnit XML results
SELECT * FROM duck_hunt('test-results.xml', 'junit');

-- Parse pytest output
SELECT * FROM duck_hunt('pytest-output.txt', 'pytest');

-- Aggregate test failures
SELECT
    suite_name,
    count(*) FILTER (WHERE status = 'failed') as failures,
    count(*) FILTER (WHERE status = 'passed') as passed
FROM duck_hunt('results/', 'junit')
GROUP BY suite_name;
```

### PRQL Queries
```sql
-- Use PRQL syntax
SELECT * FROM prql('
    from employees
    filter department == "Engineering"
    derive {
        annual_salary = salary * 12,
        tenure = today() - start_date
    }
    sort {-tenure}
    take 10
');
```

### Cryptographic Functions
```sql
-- Generate hashes
SELECT
    sha256('secret data') as sha256_hash,
    blake3('secret data') as blake3_hash,
    xxhash64('secret data') as xxhash;

-- HMAC for API signatures
SELECT hmac_sha256('message', 'secret_key') as signature;

-- Generate random bytes
SELECT encode(random_bytes(32), 'hex') as random_hex;
```

### Test Data Generation
```sql
-- Generate fake user data
SELECT
    fake_first_name() as first_name,
    fake_last_name() as last_name,
    fake_email() as email,
    fake_phone() as phone,
    fake_address() as address
FROM range(100);

-- Generate realistic datasets
CREATE TABLE test_orders AS
SELECT
    fake_uuid() as order_id,
    fake_company() as company,
    fake_credit_card() as payment_method,
    round(random() * 1000, 2) as amount,
    fake_date_between('2024-01-01', '2024-12-31') as order_date
FROM range(1000);
```

### Code Analysis Patterns
```sql
-- Find complex SQL queries (by AST depth)
SELECT
    file_path,
    query_text,
    max(depth) as max_depth
FROM (
    SELECT file_path, query_text, ast_depth(node) as depth
    FROM sql_files, LATERAL parse_sql(query_text)
)
GROUP BY file_path, query_text
HAVING max_depth > 5
ORDER BY max_depth DESC;

-- Find unused variables (cross-reference AST)
WITH declarations AS (
    SELECT var_name FROM parse_sql(code) WHERE node_type = 'declaration'
),
references AS (
    SELECT var_name FROM parse_sql(code) WHERE node_type = 'reference'
)
SELECT var_name as unused_variable
FROM declarations
WHERE var_name NOT IN (SELECT var_name FROM references);
```

## Use Cases

1. **SQL Linting**: Parse and validate SQL queries against style rules
2. **Code Migration**: Extract and transform SQL patterns across codebase
3. **Test Analysis**: Aggregate test results from CI/CD pipelines
4. **API Testing**: Generate test data and verify cryptographic signatures
5. **Git Archaeology**: Query historical versions of files
6. **Dependency Analysis**: Parse code to find import/require patterns

## Configuration

The server enables these MCP tools:
- `query`: Run SQL queries with developer extensions
- `describe`: Get table/view schema information
- `list_tables`: List available tables
- `database_info`: Get database metadata
- `export`: Export query results to files

Execute tool is disabled for safety.
