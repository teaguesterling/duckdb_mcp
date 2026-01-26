# Changelog

All notable changes to the DuckDB MCP Extension.

---

## v1.4.0

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
