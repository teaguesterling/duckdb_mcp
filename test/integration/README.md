# Integration Tests

This directory contains comprehensive integration tests for the DuckDB MCP Extension.

## Overview

The integration tests validate end-to-end functionality including:
- HTTP transport with authentication
- Memory transport for server testing
- Custom tool publishing and execution
- Resource publishing
- Output format handling
- Error handling
- Server configuration options

## Running Tests

```bash
# Run all integration tests
./test/integration/run_integration_tests.sh

# Or from the project root
make integration_test
```

## Test Categories

### Extension Loading
- Verifies the extension loads successfully
- Tests basic function availability

### Memory Transport Tests
- Server startup with memory transport
- JSON-RPC protocol compliance
- Built-in tools (query, describe, list_tables, etc.)

### Stdio Transport Tests
- Initialize request/response via stdin/stdout
- tools/list via stdio
- Query execution via stdio
- Multiple sequential requests
- Describe tool via stdio

### HTTP Transport Tests
- Health endpoint
- MCP endpoints (/ and /mcp)
- CORS headers
- JSON-RPC over HTTP

### HTTP Authentication Tests
- 401 Unauthorized (no credentials)
- 403 Forbidden (wrong credentials)
- 200 OK (correct credentials)

### Custom Tool Publishing
- `mcp_publish_tool` function
- Parameter substitution
- Tool execution

### Resource Publishing
- `mcp_publish_table`
- `mcp_publish_query`
- `mcp_publish_resource`

### Output Formats
- JSON format
- Markdown format
- CSV format

### Server Configuration
- Tool enable/disable flags
- Default format configuration

### Error Handling
- Invalid SQL errors
- Invalid method errors
- Authentication errors

## Requirements

- Built DuckDB binary (`build/release/duckdb`)
- Built extension (`build/release/extension/duckdb_mcp/duckdb_mcp.duckdb_extension`)
- `curl` (for HTTP tests, optional)

## CI Integration

These tests are run automatically in CI after the extension is built. See `.github/workflows/MainDistributionPipeline.yml` for details.
