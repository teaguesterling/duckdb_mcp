# Specialized DuckDB MCP Agents

These agents extend DuckDB MCP capabilities with domain-specific expertise using community extensions.

## Available Specialized Agents

| Agent | Domain | Key Extensions |
|-------|--------|----------------|
| [duckdb-mcp-devops](./duckdb-mcp-devops.md) | CI/CD, Infrastructure | duck_tails, duck_hunt, yaml |
| [duckdb-mcp-web](./duckdb-mcp-web.md) | APIs, Web Scraping | webbed, http_client, netquack |
| [duckdb-mcp-docs](./duckdb-mcp-docs.md) | Documentation | markdown, yaml, fts |
| [duckdb-mcp-developer](./duckdb-mcp-developer.md) | Code Analysis | sitting_duck, duck_tails, duck_hunt |

## Extension Dependencies

These agents require community and core extensions. Install them with:

```sql
-- Enable community extensions
SET allow_community_extensions = true;

-- ============================================
-- DevOps Extensions
-- ============================================
INSTALL duck_tails FROM community;  -- Git history via git:// protocol
INSTALL duck_hunt FROM community;   -- Test/build result parsing (44+ formats)
INSTALL yaml FROM community;        -- YAML config parsing
INSTALL otlp FROM community;        -- OpenTelemetry metrics/logs/traces
INSTALL hostfs FROM community;      -- File system navigation
INSTALL shellfs FROM community;     -- Shell command I/O
INSTALL sshfs FROM community;       -- SSH file access
INSTALL zipfs FROM community;       -- Read from zip archives
INSTALL inet;                       -- IP address functions (core)

-- ============================================
-- Web Extensions
-- ============================================
INSTALL webbed FROM community;      -- HTML/XML parsing with XPath
INSTALL http_client FROM community; -- HTTP requests
INSTALL curl_httpfs FROM community; -- HTTP/2 with connection pooling
INSTALL json_schema FROM community; -- JSON schema validation
INSTALL jsonata FROM community;     -- JSONata expression language
INSTALL netquack FROM community;    -- URL/domain parsing
INSTALL gsheets FROM community;     -- Google Sheets read/write
INSTALL httpfs;                     -- HTTP/S3 file access (core)
INSTALL aws;                        -- AWS S3 credentials (core)
INSTALL azure;                      -- Azure Blob Storage (core)

-- ============================================
-- Documentation Extensions
-- ============================================
INSTALL markdown FROM community;    -- Hierarchical markdown parsing
INSTALL fts FROM community;         -- Full-text search
INSTALL rapidfuzz FROM community;   -- Fuzzy string matching
INSTALL inflector FROM community;   -- String case transformations
INSTALL textplot FROM community;    -- Text-based visualizations
INSTALL minijinja FROM community;   -- Jinja-style templating
INSTALL excel FROM community;       -- Excel file support

-- ============================================
-- Developer Extensions
-- ============================================
INSTALL sitting_duck FROM community; -- AST parsing (12+ languages)
INSTALL parser_tools FROM community; -- SQL parsing and table extraction
INSTALL poached FROM community;      -- SQL introspection for IDEs
INSTALL prql FROM community;         -- PRQL alternative syntax
INSTALL hashfuncs FROM community;    -- Fast hashing (xxHash, etc.)
INSTALL crypto FROM community;       -- Cryptographic hashes
INSTALL fakeit FROM community;       -- Realistic test data generation
```

## Use Cases

### duckdb-mcp-devops
- Parse CI/CD workflow logs (GitHub Actions, GitLab CI, Jenkins)
- Analyze test results across 44+ frameworks (pytest, Jest, Go test, Cargo, JUnit, etc.)
- Query git history via `git://` protocol and compare file versions
- Process Kubernetes, Docker, and Helm configurations (YAML)
- Track build performance and failure patterns
- Read OpenTelemetry metrics, logs, and traces (otlp)
- Analyze IP addresses and network data (inet)
- Access remote files via SSH (sshfs) and execute shell commands (shellfs)

### duckdb-mcp-web
- Query REST and GraphQL APIs from SQL
- Scrape and parse HTML pages with XPath (webbed)
- Validate API responses against JSON schemas (json_schema)
- Transform JSON with JSONata expressions (jsonata)
- Parse and analyze URLs/domains (netquack)
- Read/write Google Sheets (gsheets)
- Access S3, Azure, and HTTP data sources
- HTTP/2 with connection pooling (curl_httpfs)

### duckdb-mcp-docs
- Parse markdown documentation with hierarchy (markdown)
- Extract and query YAML frontmatter
- Build full-text searchable content indexes (fts)
- Fuzzy search across documentation (rapidfuzz)
- String case transformations (inflector)
- Generate text-based visualizations (textplot)
- Create reports from templates (minijinja)
- Read Excel files (excel)

### duckdb-mcp-developer
- Parse source code AST across 12+ languages (sitting_duck)
- Analyze code complexity and structure
- Map dependencies and imports
- Correlate code changes with test failures
- Parse SQL queries for table dependencies (parser_tools)
- Use PRQL alternative syntax (prql)
- Generate file checksums with xxHash (hashfuncs)
- Create cryptographic hashes (crypto)
- Generate realistic test data (fakeit)

## Agent Collaboration

These agents are designed to work together:

```
┌─────────────────────────────────────────────────────────────────┐
│                     MCP Server (DuckDB)                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐     │
│  │   developer  │───▶│    devops    │───▶│   analyst    │     │
│  │  (code AST)  │    │  (CI/tests)  │    │  (metrics)   │     │
│  └──────────────┘    └──────────────┘    └──────────────┘     │
│         │                   │                   │              │
│         ▼                   ▼                   ▼              │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐     │
│  │     docs     │◀───│     web      │◀───│   explorer   │     │
│  │  (markdown)  │    │   (APIs)     │    │   (files)    │     │
│  └──────────────┘    └──────────────┘    └──────────────┘     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**Example workflow:**
1. `developer` analyzes code changes in a PR
2. `devops` parses the test results from CI
3. `docs` checks if documentation needs updating
4. `analyst` generates a summary report

## Extension Sources

Many extensions referenced here are from:
- [teaguesterling/sitting_duck](https://github.com/teaguesterling/sitting_duck) - AST parsing
- [teaguesterling/duck_tails](https://github.com/teaguesterling/duck_tails) - Git history
- [teaguesterling/duck_hunt](https://github.com/teaguesterling/duck_hunt) - Test results
- [teaguesterling/duckdb_markdown](https://github.com/teaguesterling/duckdb_markdown) - Markdown
- [teaguesterling/duckdb_yaml](https://github.com/teaguesterling/duckdb_yaml) - YAML
- [teaguesterling/duckdb_webbed](https://github.com/teaguesterling/duckdb_webbed) - HTML/XML

See the [DuckDB Community Extensions](https://duckdb.org/community_extensions/list_of_extensions) for a complete list.
