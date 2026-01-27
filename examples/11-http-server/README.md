# Example 11: HTTP Server

This example demonstrates running DuckDB MCP as an HTTP server, enabling web-based clients and HTTP tools to connect.

## Overview

The HTTP transport provides:
- RESTful HTTP endpoint for MCP JSON-RPC
- Bearer token authentication
- CORS support for browser clients
- Health check endpoint

## Files

- `init-http-server.sql` - Server initialization with sample data
- `test-http.sh` - Shell script to test HTTP endpoints

## Quick Start

### Start the Server

```bash
# Start DuckDB with HTTP server
duckdb -init init-http-server.sql
```

The server will start on `http://localhost:8080` with authentication enabled.

### Test with curl

```bash
# Run the test script
./test-http.sh

# Or manually:

# Health check (no auth required)
curl http://localhost:8080/health

# List tools (requires auth)
curl -X POST http://localhost:8080/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer demo-token" \
    -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'

# Query data
curl -X POST http://localhost:8080/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer demo-token" \
    -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"query","arguments":{"sql":"SELECT * FROM products"}}}'
```

## Authentication

Without the correct token, requests will fail:

```bash
# No token - returns 401 Unauthorized
curl -X POST http://localhost:8080/mcp \
    -H "Content-Type: application/json" \
    -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'

# Wrong token - returns 403 Forbidden
curl -X POST http://localhost:8080/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer wrong-token" \
    -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'
```

## Configuration Options

The server config supports:

| Option | Description |
|--------|-------------|
| `auth_token` | Bearer token for authentication |
| `ssl_cert_path` | Path to SSL certificate (for HTTPS) |
| `ssl_key_path` | Path to SSL private key (for HTTPS) |

## Use Cases

- **Web Applications**: Browser-based database interfaces
- **API Integration**: Backend services querying DuckDB
- **Monitoring**: Health check endpoints for load balancers
- **Development**: Easy testing with curl or Postman
