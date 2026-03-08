# Changelog

All notable changes to the DuckDB MCP Extension.

---

## v2.0.0

### Security (15 bugs fixed from security audit)

- Fixed SQL injection in `DescribeTable`, `DescribeQuery`, `ListTables`, `ExportToolHandler`, and `SQLToolHandler` parameter substitution
- Fixed arbitrary file write via `ExportToolHandler` (path escaping + query allowlist enforcement)
- Fixed broken `ValidateAuthentication` stub (was always returning true)
- Fixed LD_PRELOAD injection: replaced `execvp` with `execve` + sanitized environment, blocked dangerous env vars
- Fixed command allowlist basename shortcut allowing PATH hijacking (removed entirely, exact match only)
- Fixed permissive mode default: `SetAllowedCommands` now locks on any call (deny-all by default)
- Fixed arbitrary config file read via `from_config_file` (validated against configured `mcp_server_file`)
- Fixed pipe FD leaks to child process (`O_CLOEXEC`)
- Fixed zombie processes from non-blocking `waitpid` (SIGTERM -> wait -> SIGKILL escalation)
- Fixed timing attack on HTTP auth token (constant-time comparison)
- Fixed unescaped data in JSON construction (`EscapeJsonString` helper)
- Fixed exception messages reflected to HTTP clients (generic error message)

### Security - Controllable Behaviors (6 items hardened)

- Query allowlist/denylist now uses statement type classification, not substring matching
- Execute tool DDL permissions split into fine-grained categories (`allow_ddl`, `allow_dml`, `allow_load`, `allow_attach`, `allow_set`)
- CORS defaults to disabled; configurable via `cors_origins` (empty/wildcard/comma-separated)
- URL prefix matching now enforces component boundary (prevents subdomain confusion)
- `/health` endpoint configurable: `enable_health_endpoint`, `auth_health_endpoint`
- `mcp_server_send_request` auto-disabled when `require_auth=true` (prevents auth bypass)

### Architecture

- **Per-instance state (`MCPInstanceState`)**: Replaced global singletons with per-`DatabaseInstance` state via `ObjectCache`. Multiple DuckDB instances no longer share server/config/connection state. (closes #38)
- **Thread-safe server access**: `MCPServerManager` now provides locked accessor methods (`PublishResource`, `RegisterTool`, `AllowsDirectRequests`, `GetServerStats`) eliminating TOCTOU races from raw `GetServer()` pointer access
- **Atomic `process_pid`**: `StdioTransport` `process_pid` changed to `std::atomic<int>` to fix data race between `IsProcessRunning` and `StopProcess`

### New Features

- **Prepared statement binding for `mcp_publish_tool`**: Tool parameters are now bound using DuckDB's prepared statement API with proper type conversion (`string` → VARCHAR, `integer` → BIGINT, `number` → DOUBLE, `boolean` → BOOLEAN). Falls back to string interpolation for non-preparable templates (e.g., macros). Safer and more efficient than string interpolation alone.
- **Multi-statement execution tools (`mcp_publish_execution_tool`)**: New function for publishing tools that execute multiple semicolon-separated SQL statements. Supports global or per-statement parameter binding specs. Returns the result of the last statement.
- **Type-aware JSON serialization**: JSON and JSONL output formats now emit numbers unquoted and booleans as `true`/`false` instead of quoting all values as strings. Produces spec-compliant JSON that clients can parse without type coercion. NaN and Infinity values are serialized as `null`.
- **Prompts capability**: Server now advertises `prompts` capability with `listChanged: true`. `prompts/list` and `prompts/get` MCP methods are fully implemented, backed by the template manager.
- **Text output format**: New `text` format for query results (plain text, tab-separated)
- **JSONL output format**: New `jsonl` format (one JSON object per line)
- **Markdown pipe escaping**: Pipe characters in cell values properly escaped in markdown output (closes #54)
- **Configurable CORS**: `cors_origins` server config option
- **Configurable health endpoint**: `enable_health_endpoint` and `auth_health_endpoint` options
- **Direct request gating**: `allow_direct_requests` and `allow_direct_requests_explicit` config options

### Bug Fixes

- Fixed double query execution in `ExportToolHandler` (#51)
- Fixed constant-time token comparison still leaking token length (#50, #39)
- Fixed JSON output format not escaping double quotes (#52)
- Fixed non-string JSON values silently dropped in config parsing (#47)
- Fixed CORS disabled test to include Origin header (#42)
- Fixed format fallback failure (#43)
- Fixed CSV output to use RFC 4180 quoting (#29)
- Fixed use-after-free in registry pointer access (#24)
- Fixed partial write handling in `WriteToProcess` (pipe I/O)
- Fixed double connection registration in `MCPStorageAttach`
- Server capability parsing from initialize response (#44)
- Deduplicated HTTP server setup (`StartHTTPServer` helper, #40)
- Error leak fixes, client stubs, registration failure surfacing (#28)

### Breaking Changes

- **Type-aware JSON output**: JSON and JSONL formats now emit numeric values unquoted (`{"id":1}` instead of `{"id":"1"}`) and booleans as `true`/`false`. Clients that rely on string comparison of JSON output or expect all values to be strings will need updating.
- Per-instance state means global state is no longer shared across database instances
- Command allowlist behavior changed: empty allowlist now means "deny all" (was "allow all")
- Basename shortcuts removed from command allowlist (exact match only)

---

## v1.5.2

### New Features

**PRAGMA Functions**

- All server management and publishing functions now have `PRAGMA` versions that produce no output
- `PRAGMA mcp_server_start('transport')`, `PRAGMA mcp_server_stop`, etc.
- `PRAGMA mcp_publish_tool(...)`, `PRAGMA mcp_publish_table(...)`, `PRAGMA mcp_publish_query(...)`, `PRAGMA mcp_publish_resource(...)`, `PRAGMA mcp_register_prompt_template(...)`
- PRAGMA versions support the same queuing behavior as SELECT versions (publish before server starts)
- Recommended for init scripts where return values are not needed

**Config Mode**

- `PRAGMA mcp_config_begin` / `PRAGMA mcp_config_end` to suppress output from SELECT versions of MCP functions
- Useful for backward compatibility with existing `SELECT`-based init scripts
- Side effects still execute; only the result output is suppressed

**COMMAND Option in ATTACH**

- New `COMMAND` option for the `ATTACH` statement: `ATTACH '' AS srv (TYPE mcp, COMMAND 'python3', ARGS '["server.py"]')`
- Provides an explicit alternative to overloading the ATTACH path slot
- When both `COMMAND` and a path are provided, `COMMAND` takes priority

### Internal Changes

- Refactored server functions to extract `*Core()` implementations (taking `ClientContext &`) from scalar wrappers
- PRAGMA wrappers and scalar wrappers both delegate to the same core functions
- Config mode flag stored in `MCPConfiguration` struct, accessed via `MCPConfigManager::IsConfigMode()`

---

## v1.4.0

### New Features

**HTTP/HTTPS Transport**

- New `http` and `https` transport types for the MCP server
- Enables browser-based clients and HTTP-compatible tools to connect
- Built on cpp-httplib (bundled with DuckDB)

**HTTP Authentication**

- Bearer token authentication support
- Configurable via `auth_token` in server config
- Proper HTTP status codes:
    - `401 Unauthorized` when no credentials provided
    - `403 Forbidden` when invalid credentials provided
- `WWW-Authenticate: Bearer` header for authentication challenges

**HTTP Endpoints**

- `/health` - Health check endpoint returning `{"status":"ok"}`
- `/` and `/mcp` - MCP JSON-RPC endpoints (POST)
- Full CORS support for browser clients

**HTTPS Support**

- SSL/TLS support when OpenSSL is available
- Configure with `ssl_cert_path` and `ssl_key_path`

### Configuration

New config options for HTTP transport:

```sql
SELECT mcp_server_start('http', 'localhost', 8080, '{
    "auth_token": "your-secret-token",
    "ssl_cert_path": "/path/to/cert.pem",
    "ssl_key_path": "/path/to/key.pem"
}');
```

### Testing

- Added HTTP transport test suite (`test/http/test_http_transport.sh`)
- Tests cover health endpoint, authentication, MCP protocol, and CORS

---

## v1.3.2

### New Features

**`mcp_publish_resource` Function**

- New SQL function for publishing static content as MCP resources
- Parameters: `(uri, content, mime_type, description)`
- Useful for configuration files, documentation, or any static data

**Publish Before Server Starts (Queuing)**

- All publish functions (`mcp_publish_tool`, `mcp_publish_table`, `mcp_publish_query`, `mcp_publish_resource`) can now be called before the server starts
- Registrations are queued and automatically applied when the server starts
- Enables cleaner initialization scripts

**Improved Parameter Handling**

- SQL tool parameters now correctly handle all JSON types (integer, boolean, string)
- Previously, integer parameters like `{"id": 1}` were not substituted correctly

### Bug Fixes

- Fixed SQL parameter substitution for non-string JSON values in custom tools

---

## v1.3.1

### Bug Fixes

- **MCP notifications no longer cause "Method not found" errors** (Issue #9)
    - Server now properly handles JSON-RPC notifications (messages without `id` field)
    - `notifications/initialized`, `notifications/cancelled`, `notifications/progress` are now silently accepted
    - Unknown notifications are ignored per MCP spec (no error response sent)

---

## v1.3.0

### New Features

**Memory Transport for Testing**

- Added `mcp_server_start('memory')` transport for in-process testing
- New `mcp_server_send_request()` function for sending JSON-RPC requests directly to a running server
- Enables comprehensive unit testing without external processes

**Markdown Output Format**

- Query tool now supports `format` parameter with options: `json` (default), `markdown`, `csv`
- Markdown format is more token-efficient for LLM contexts - column names appear once in headers rather than repeated per row
- Server configuration option `default_result_format` to set default format

**Server Configuration Options**

- New JSON config parameter for `mcp_server_start()`:
    - `enable_query_tool`, `enable_describe_tool`, `enable_list_tables_tool`
    - `enable_database_info_tool`, `enable_export_tool`, `enable_execute_tool`
    - `default_result_format`: "json" | "markdown" | "csv"
- Fine-grained control over which tools are exposed to MCP clients

**`mcp_publish_tool` Function**

- New SQL function for publishing custom parameterized tools
- 5-param version: `(name, description, sql_template, properties_json, required_json)`
- 6-param version: adds optional format (json, csv, markdown)
- Parameters in SQL template use `$name` syntax

**Server Statistics**

- `mcp_server_status()` now reports `requests_received`, `responses_sent`, and `errors_returned` counters

**Shared `ResultFormatter` Utility**

- Consolidated JSON/CSV/Markdown formatting code used by both tool handlers and resource providers

### Bug Fixes

- **MCP protocol compliance for Claude Code compatibility** (critical)
    - Initialize response now includes `protocolVersion` and `serverInfo` at top level
    - Capabilities changed from booleans to objects with `listChanged`/`subscribe` sub-fields per MCP spec
    - `tools/list` inputSchema is now a nested object instead of escaped string
    - `tools/list` properties follow JSON Schema compliance (object with property names as keys)
    - JSON-typed values are now parsed and inlined instead of escaped as strings

- **Format validation returns errors instead of silent fallback** (Issue #8)
    - Unsupported formats (jsonl, table, box, parquet for inline) now return clear error messages
    - Previously would silently fall back to confusing tab-separated output

- **Fixed describe tool for query schema inspection**
    - The describe tool's `query` argument was broken (used `PREPARE` + `DESCRIBE stmt` which DuckDB doesn't support)
    - Changed to use `DESCRIBE ()` syntax

- **Examples work without icu extension**
    - Replaced `CURRENT_DATE` with `NOW()::TIMESTAMP::DATE` in all examples

### Examples & Documentation

- Simplified examples (01-10) to use direct DuckDB invocation instead of wrapper scripts
- Added specialized agent examples (07-devops, 08-web-api, 09-docs, 10-developer)
- Added Claude Code agent definitions for IDE integration
- Updated README with server tools documentation
- Improved `05-custom-tools` example with macro tables, `mcp_publish_tool` usage, and JSON-RPC examples

### Testing

- Added comprehensive memory transport tests (`mcp_memory_transport.test`)
- Added server configuration tests (`mcp_server_config.test`)
- Added resource publishing tests (`mcp_resource_publishing.test`)
- Added MCP spec compliance tests for initialize and tools/list responses
- Test coverage increased from ~223 to 324+ assertions

### Internal Changes

- Centralized version constant (`DUCKDB_MCP_VERSION`)
- Standardized on JSON for MCP protocol internally
- Simplified stdio transport by extracting common server logic
- Refactored `tool_handlers.cpp` and `resource_providers.cpp` to use shared `ResultFormatter`

---

## v1.2.0

Initial stable release with core MCP functionality.

### Features

- MCP client support with `ATTACH` statement
- MCP server support with `mcp_server_start()`
- Resource listing and reading (`mcp_list_resources`, `mcp_get_resource`)
- Tool listing and execution (`mcp_list_tools`, `mcp_call_tool`)
- Prompt support (`mcp_list_prompts`, `mcp_get_prompt`)
- Local prompt templates
- Table and query publishing (`mcp_publish_table`, `mcp_publish_query`)
- Cursor-based pagination for large result sets
- Multiple transport support (stdio, TCP, WebSocket)
- Built-in server tools (query, describe, list_tables, database_info, export, execute)

---

## v1.0.0

Initial release.
