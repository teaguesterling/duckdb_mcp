#!/bin/bash
# Test script for HTTP server example

HOST="localhost"
PORT="8080"
TOKEN="demo-token"

echo "=== DuckDB MCP HTTP Server Tests ==="
echo ""

echo "1. Health check (no auth required)"
curl -s http://$HOST:$PORT/health
echo ""
echo ""

echo "2. Initialize (with auth)"
curl -s -X POST http://$HOST:$PORT/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d '{
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "protocolVersion": "2024-11-05",
            "clientInfo": {"name": "test-script", "version": "1.0"},
            "capabilities": {}
        }
    }'
echo ""
echo ""

echo "3. List tools"
curl -s -X POST http://$HOST:$PORT/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' | python3 -m json.tool 2>/dev/null || cat
echo ""

echo "4. Query all products"
curl -s -X POST http://$HOST:$PORT/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"query","arguments":{"sql":"SELECT * FROM products","format":"markdown"}}}'
echo ""
echo ""

echo "5. Use custom search tool"
curl -s -X POST http://$HOST:$PORT/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $TOKEN" \
    -d '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"search_products","arguments":{"query":"Electronics"}}}'
echo ""
echo ""

echo "6. Test authentication - no token (expect 401)"
curl -s -w " [HTTP %{http_code}]" -X POST http://$HOST:$PORT/mcp \
    -H "Content-Type: application/json" \
    -d '{"jsonrpc":"2.0","id":5,"method":"tools/list","params":{}}'
echo ""
echo ""

echo "7. Test authentication - wrong token (expect 403)"
curl -s -w " [HTTP %{http_code}]" -X POST http://$HOST:$PORT/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer wrong-token" \
    -d '{"jsonrpc":"2.0","id":6,"method":"tools/list","params":{}}'
echo ""
echo ""

echo "=== Tests Complete ==="
