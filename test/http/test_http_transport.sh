#!/bin/bash
# Test script for DuckDB MCP HTTP transport
# Tests HTTP server functionality, authentication, and MCP protocol over HTTP

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
DUCKDB="$PROJECT_DIR/build/release/duckdb"
EXTENSION="$PROJECT_DIR/build/release/extension/duckdb_mcp/duckdb_mcp.duckdb_extension"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0

# Cleanup function
cleanup() {
    if [ -n "$DUCKDB_PID" ]; then
        kill $DUCKDB_PID 2>/dev/null || true
        wait $DUCKDB_PID 2>/dev/null || true
    fi
}
trap cleanup EXIT

# Test helper functions
pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    echo "  Expected: $2"
    echo "  Got: $3"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

start_server() {
    local port=$1
    local config=${2:-'{}'}

    (
        trap '' PIPE
        echo "LOAD '$EXTENSION';"
        echo "SELECT mcp_server_start('http', 'localhost', $port, '$config');"
        for i in $(seq 1 30); do sleep 1; echo "" 2>/dev/null || true; done
        echo ".exit"
    ) 2>/dev/null | "$DUCKDB" -unsigned &
    DUCKDB_PID=$!
    sleep 2
}

stop_server() {
    if [ -n "$DUCKDB_PID" ]; then
        kill $DUCKDB_PID 2>/dev/null || true
        wait $DUCKDB_PID 2>/dev/null || true
        unset DUCKDB_PID
    fi
}

echo "=========================================="
echo "DuckDB MCP HTTP Transport Tests"
echo "=========================================="
echo ""

# Check prerequisites
if [ ! -f "$DUCKDB" ]; then
    echo -e "${RED}Error: DuckDB binary not found at $DUCKDB${NC}"
    exit 1
fi

if [ ! -f "$EXTENSION" ]; then
    echo -e "${RED}Error: Extension not found at $EXTENSION${NC}"
    exit 1
fi

if ! command -v curl &> /dev/null; then
    echo -e "${RED}Error: curl is required but not installed${NC}"
    exit 1
fi

# ==========================================
# Test 1: Basic HTTP server startup
# ==========================================
echo -e "${YELLOW}Test 1: Basic HTTP server startup${NC}"
PORT=18080
start_server $PORT

RESPONSE=$(curl -s http://localhost:$PORT/health 2>/dev/null || echo "CURL_ERROR")
if [ "$RESPONSE" = '{"status":"ok"}' ]; then
    pass "Health endpoint responds correctly"
else
    fail "Health endpoint" '{"status":"ok"}' "$RESPONSE"
fi

stop_server

# ==========================================
# Test 2: MCP Initialize request
# ==========================================
echo -e "${YELLOW}Test 2: MCP Initialize request${NC}"
PORT=18081
start_server $PORT

RESPONSE=$(curl -s -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}' \
    2>/dev/null || echo "CURL_ERROR")

if echo "$RESPONSE" | grep -q '"serverInfo"'; then
    pass "Initialize returns server info"
else
    fail "Initialize request" "Response containing serverInfo" "$RESPONSE"
fi

if echo "$RESPONSE" | grep -q '"version":"1.4.0"'; then
    pass "Server reports version 1.4.0"
else
    fail "Server version" "1.4.0" "$RESPONSE"
fi

stop_server

# ==========================================
# Test 3: Tools list
# ==========================================
echo -e "${YELLOW}Test 3: Tools list${NC}"
PORT=18082
start_server $PORT

RESPONSE=$(curl -s -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' \
    2>/dev/null || echo "CURL_ERROR")

if echo "$RESPONSE" | grep -q '"name":"query"'; then
    pass "Tools list includes query tool"
else
    fail "Tools list" "query tool in response" "$RESPONSE"
fi

if echo "$RESPONSE" | grep -q '"name":"describe"'; then
    pass "Tools list includes describe tool"
else
    fail "Tools list" "describe tool in response" "$RESPONSE"
fi

stop_server

# ==========================================
# Test 4: Query execution
# ==========================================
echo -e "${YELLOW}Test 4: Query execution${NC}"
PORT=18083
start_server $PORT

RESPONSE=$(curl -s -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"query","arguments":{"sql":"SELECT 42 as answer"}}}' \
    2>/dev/null || echo "CURL_ERROR")

if echo "$RESPONSE" | grep -q 'answer.*42'; then
    pass "Query returns correct result"
else
    fail "Query execution" 'response containing answer 42' "$RESPONSE"
fi

stop_server

# ==========================================
# Test 5: Authentication - No credentials (401)
# ==========================================
echo -e "${YELLOW}Test 5: Authentication - No credentials${NC}"
PORT=18084
start_server $PORT '{"auth_token":"secret123"}'

HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
    2>/dev/null || echo "000")

if [ "$HTTP_CODE" = "401" ]; then
    pass "No credentials returns 401 Unauthorized"
else
    fail "No credentials status" "401" "$HTTP_CODE"
fi

stop_server

# ==========================================
# Test 6: Authentication - Wrong credentials (403)
# ==========================================
echo -e "${YELLOW}Test 6: Authentication - Wrong credentials${NC}"
PORT=18085
start_server $PORT '{"auth_token":"secret123"}'

HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer wrongtoken" \
    -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
    2>/dev/null || echo "000")

if [ "$HTTP_CODE" = "403" ]; then
    pass "Wrong credentials returns 403 Forbidden"
else
    fail "Wrong credentials status" "403" "$HTTP_CODE"
fi

stop_server

# ==========================================
# Test 7: Authentication - Correct credentials (200)
# ==========================================
echo -e "${YELLOW}Test 7: Authentication - Correct credentials${NC}"
PORT=18086
start_server $PORT '{"auth_token":"secret123"}'

HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer secret123" \
    -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}' \
    2>/dev/null || echo "000")

if [ "$HTTP_CODE" = "200" ]; then
    pass "Correct credentials returns 200 OK"
else
    fail "Correct credentials status" "200" "$HTTP_CODE"
fi

stop_server

# ==========================================
# Test 8: CORS headers
# ==========================================
echo -e "${YELLOW}Test 8: CORS headers${NC}"
PORT=18087
start_server $PORT

CORS_HEADER=$(curl -s -I -X OPTIONS http://localhost:$PORT/mcp 2>/dev/null | grep -i "Access-Control-Allow-Origin" || echo "")

if echo "$CORS_HEADER" | grep -qi "Access-Control-Allow-Origin"; then
    pass "CORS headers are present"
else
    fail "CORS headers" "Access-Control-Allow-Origin header" "$CORS_HEADER"
fi

stop_server

# ==========================================
# Test 9: Alternative endpoint (/mcp)
# ==========================================
echo -e "${YELLOW}Test 9: Alternative endpoint (/mcp)${NC}"
PORT=18088
start_server $PORT

RESPONSE=$(curl -s -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"query","arguments":{"sql":"SELECT 1"}}}' \
    2>/dev/null || echo "CURL_ERROR")

if echo "$RESPONSE" | grep -q '"result"'; then
    pass "/mcp endpoint works"
else
    fail "/mcp endpoint" "valid response" "$RESPONSE"
fi

# Also test root endpoint
RESPONSE=$(curl -s -X POST http://localhost:$PORT/ \
    -H "Content-Type: application/json" \
    -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"query","arguments":{"sql":"SELECT 1"}}}' \
    2>/dev/null || echo "CURL_ERROR")

if echo "$RESPONSE" | grep -q '"result"'; then
    pass "/ endpoint works"
else
    fail "/ endpoint" "valid response" "$RESPONSE"
fi

stop_server

# ==========================================
# Summary
# ==========================================
echo ""
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
echo -e "${RED}Failed: $TESTS_FAILED${NC}"

if [ $TESTS_FAILED -gt 0 ]; then
    exit 1
fi

echo ""
echo -e "${GREEN}All tests passed!${NC}"
