# Example 08: Web API MCP Server

A DuckDB MCP server configured for web data workflows with extensions for HTTP requests, HTML parsing, JSON validation, and cloud storage access.

## Features

- **HTTP Client**: Make REST/GraphQL requests via `http_client`
- **HTML/XML Parsing**: XPath extraction via `webbed`
- **JSON Validation**: Schema validation via `json_schema`
- **JSON Transformation**: JSONata expressions via `jsonata`
- **URL Parsing**: Domain/path analysis via `netquack`
- **Cloud Storage**: S3, Azure, Google Sheets access

## Extensions Used

| Extension | Purpose |
|-----------|---------|
| httpfs | HTTP/S3 file access |
| http_client | HTTP requests (GET, POST, etc.) |
| curl_httpfs | HTTP/2 with connection pooling |
| webbed | HTML/XML parsing with XPath |
| json_schema | JSON schema validation |
| jsonata | JSONata expression language |
| netquack | URL/domain parsing |
| gsheets | Google Sheets read/write |
| aws | AWS S3 credentials |
| azure | Azure Blob Storage |

## Quick Start

```bash
# Set the DuckDB binary path (optional)
export DUCKDB=/path/to/duckdb

# Run the MCP server
./launch-mcp.sh
```

## Example Queries

### Remote Data Access
```sql
-- Read CSV from URL
SELECT * FROM read_csv('https://example.com/data.csv');

-- Read JSON API response
SELECT * FROM read_json('https://api.github.com/repos/duckdb/duckdb/releases');

-- Read from S3
SELECT * FROM read_parquet('s3://bucket/path/data.parquet');
```

### HTTP Requests
```sql
-- GET request
SELECT * FROM http_get('https://api.example.com/users');

-- POST with JSON body
SELECT * FROM http_post(
    'https://api.example.com/data',
    body := '{"key": "value"}',
    headers := {'Content-Type': 'application/json'}
);
```

### HTML Parsing
```sql
-- Extract with XPath
SELECT xpath_extract(content, '//h1/text()') as title
FROM read_html('https://example.com');

-- Parse HTML tables
SELECT * FROM read_html_table('https://example.com/data.html', 0);
```

### JSON Schema Validation
```sql
-- Validate API response
SELECT json_valid_schema(response, '{
    "type": "object",
    "required": ["id", "name"]
}') as is_valid
FROM api_responses;
```

### JSONata Transformation
```sql
-- Transform JSON with JSONata
SELECT jsonata('Account.Order.Product.Price', data) as prices
FROM orders;
```

### URL Analysis
```sql
-- Parse URLs
SELECT url_host(url), url_path(url), url_query(url)
FROM web_logs;

-- Domain analysis
SELECT domain_registered('www.example.co.uk') as domain;
```

## Configuration

The server is configured with:
- All read-only tools enabled
- HTTP/cloud extensions loaded
- Execute tool disabled for safety

## Use with Claude Desktop

Add to your MCP configuration:
```json
{
  "mcpServers": {
    "duckdb-web": {
      "command": "/path/to/examples/08-web-api/launch-mcp.sh"
    }
  }
}
```

## Environment Variables

For cloud access, set credentials:
```bash
# AWS
export AWS_ACCESS_KEY_ID=your_key
export AWS_SECRET_ACCESS_KEY=your_secret

# Azure
export AZURE_STORAGE_ACCOUNT=your_account
export AZURE_STORAGE_KEY=your_key
```
