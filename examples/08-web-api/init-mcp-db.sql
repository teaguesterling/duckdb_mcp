-- init-mcp-db.sql: Web API MCP server initialization
-- Loads extensions for HTTP, HTML parsing, JSON validation, and cloud access

-- ============================================
-- Load Core Extensions
-- ============================================

-- HTTP and remote file access
INSTALL httpfs;
LOAD httpfs;

-- JSON processing
INSTALL json;
LOAD json;

-- AWS S3 credentials
INSTALL aws;
LOAD aws;

-- Azure Blob Storage
INSTALL azure;
LOAD azure;

-- ============================================
-- Load Community Extensions
-- ============================================

-- HTTP client for requests
INSTALL http_client FROM community;
LOAD http_client;

-- HTTP/2 with connection pooling
INSTALL curl_httpfs FROM community;
LOAD curl_httpfs;

-- HTML/XML parsing with XPath
INSTALL webbed FROM community;
LOAD webbed;

-- JSON schema validation
INSTALL json_schema FROM community;
LOAD json_schema;

-- JSONata expression language
INSTALL jsonata FROM community;
LOAD jsonata;

-- URL/domain parsing
INSTALL netquack FROM community;
LOAD netquack;

-- Google Sheets access
INSTALL gsheets FROM community;
LOAD gsheets;

.print '[init-mcp-db] Web API extensions loaded'

-- ============================================
-- Helper Macros
-- ============================================

-- GraphQL query helper
CREATE OR REPLACE MACRO query_graphql(url, query, variables := '{}', headers := '{}') AS (
    SELECT
        response_body::JSON->'data' as data,
        response_body::JSON->'errors' as errors,
        response_status as status
    FROM http_post(
        url,
        body := json_object('query', query, 'variables', variables::JSON),
        headers := json_merge('{"Content-Type": "application/json"}', headers)
    )
);

-- JSON API reader helper
CREATE OR REPLACE MACRO read_api(url, headers := '{}') AS (
    SELECT
        response_body::JSON as data,
        response_status as status,
        response_time_ms as latency_ms
    FROM http_get(url, headers := headers)
);

.print '[init-mcp-db] Helper macros created'
