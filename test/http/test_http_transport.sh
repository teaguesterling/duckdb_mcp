#!/bin/bash
# DuckDB v2.0 CLI switches to "agent mode" output when it sees AI_AGENT, CLAUDECODE
# and similar; these scripts parse the plain output, so force it off. v1.5 ignores it.
export DUCKDB_AGENT_MODE=0
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

if echo "$RESPONSE" | grep -q '"version":"2.0.0"'; then
    pass "Server reports version 2.0.0"
else
    fail "Server version" "2.0.0" "$RESPONSE"
fi

stop_server

# ==========================================
# Test 3: Tools list
# ==========================================
echo -e "${YELLOW}Test 3: Tools list${NC}"
PORT=18082
start_server $PORT

NOTIFICATION_BODY=$(mktemp)
NOTIFICATION_STATUS=$(curl -s -X POST http://localhost:$PORT/mcp \
    -o "$NOTIFICATION_BODY" -w "%{http_code}" \
    -H "Content-Type: application/json" \
    -d '{"jsonrpc":"2.0","method":"notifications/initialized"}' \
    2>/dev/null || echo "000")

if [ "$NOTIFICATION_STATUS" = "202" ] && [ ! -s "$NOTIFICATION_BODY" ]; then
    pass "Notification returns 202 with an empty body"
else
    fail "Notification response" "202 with an empty body" "$NOTIFICATION_STATUS $(cat "$NOTIFICATION_BODY")"
fi
rm -f "$NOTIFICATION_BODY"

TOOLS_BODY=$(mktemp)
TOOLS_STATUS=$(curl -s -X POST http://localhost:$PORT/mcp \
    -o "$TOOLS_BODY" -w "%{http_code}" \
    -H "Content-Type: application/json" \
    -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' \
    2>/dev/null || echo "CURL_ERROR")
RESPONSE=$(cat "$TOOLS_BODY")

if [ "$TOOLS_STATUS" = "200" ]; then
    pass "Normal request returns 200 OK"
else
    fail "Normal request status" "200" "$TOOLS_STATUS"
fi

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

rm -f "$TOOLS_BODY"
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
# Test 7b: Auth - shorter token rejected (403)
# ==========================================
echo -e "${YELLOW}Test 7b: Auth - shorter token rejected${NC}"
PORT=18186
start_server $PORT '{"auth_token":"secret123"}'

HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer s" \
    -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
    2>/dev/null || echo "000")

if [ "$HTTP_CODE" = "403" ]; then
    pass "Shorter token returns 403 Forbidden"
else
    fail "Shorter token status" "403" "$HTTP_CODE"
fi

stop_server

# ==========================================
# Test 7c: Auth - longer token rejected (403)
# ==========================================
echo -e "${YELLOW}Test 7c: Auth - longer token rejected${NC}"
PORT=18286
start_server $PORT '{"auth_token":"secret123"}'

HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer secret123extrabyteshere" \
    -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
    2>/dev/null || echo "000")

if [ "$HTTP_CODE" = "403" ]; then
    pass "Longer token returns 403 Forbidden"
else
    fail "Longer token status" "403" "$HTTP_CODE"
fi

stop_server

# ==========================================
# Test 7d: Auth - same-length wrong token rejected (403)
# ==========================================
echo -e "${YELLOW}Test 7d: Auth - same-length wrong token rejected${NC}"
PORT=18386
start_server $PORT '{"auth_token":"secret123"}'

# "secret123" is 9 chars; "wrongtokn" is also 9 chars
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer wrongtokn" \
    -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
    2>/dev/null || echo "000")

if [ "$HTTP_CODE" = "403" ]; then
    pass "Same-length wrong token returns 403 Forbidden"
else
    fail "Same-length wrong token status" "403" "$HTTP_CODE"
fi

stop_server

# ==========================================
# Test 7e: Auth - correct prefix, wrong suffix rejected (403)
# ==========================================
echo -e "${YELLOW}Test 7e: Auth - correct prefix wrong suffix rejected${NC}"
PORT=18486
start_server $PORT '{"auth_token":"secret123"}'

HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer secret124" \
    -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
    2>/dev/null || echo "000")

if [ "$HTTP_CODE" = "403" ]; then
    pass "Correct prefix wrong suffix returns 403 Forbidden"
else
    fail "Correct prefix wrong suffix status" "403" "$HTTP_CODE"
fi

stop_server

# ==========================================
# Test 8: CORS disabled by default
# ==========================================
echo -e "${YELLOW}Test 8: CORS disabled by default (no cors_origins config)${NC}"
PORT=18087
start_server $PORT

# OPTIONS request should NOT have CORS headers when cors_origins is not configured
CORS_HEADER=$(curl -s -I -X OPTIONS http://localhost:$PORT/mcp -H "Origin: https://example.com" 2>/dev/null | grep -i "Access-Control-Allow-Origin" || echo "")

if [ -z "$CORS_HEADER" ]; then
    pass "No CORS headers when cors_origins not configured"
else
    fail "Default CORS" "No Access-Control-Allow-Origin header" "$CORS_HEADER"
fi

# POST request should also NOT have CORS headers
POST_CORS=$(curl -s -D - -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -H "Origin: https://evil.example.com" \
    -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' \
    2>/dev/null | grep -i "Access-Control-Allow-Origin" || echo "")

if [ -z "$POST_CORS" ]; then
    pass "No CORS headers on POST when cors_origins not configured"
else
    fail "Default CORS POST" "No Access-Control-Allow-Origin header" "$POST_CORS"
fi

stop_server

# ==========================================
# Test 9: CORS wildcard allows all origins
# ==========================================
echo -e "${YELLOW}Test 9: CORS wildcard allows all origins${NC}"
PORT=18088
start_server $PORT '{"cors_origins": "*"}'

# OPTIONS preflight should return CORS headers
OPTIONS_HEADERS=$(curl -s -I -X OPTIONS http://localhost:$PORT/mcp \
    -H "Origin: https://example.com" \
    2>/dev/null)

CORS_ORIGIN=$(echo "$OPTIONS_HEADERS" | grep -i "Access-Control-Allow-Origin" || echo "")
if echo "$CORS_ORIGIN" | grep -q '\*'; then
    pass "Wildcard CORS returns Access-Control-Allow-Origin: *"
else
    fail "Wildcard CORS origin" "Access-Control-Allow-Origin: *" "$CORS_ORIGIN"
fi

CORS_METHODS=$(echo "$OPTIONS_HEADERS" | grep -i "Access-Control-Allow-Methods" || echo "")
if echo "$CORS_METHODS" | grep -qi "POST"; then
    pass "Wildcard CORS returns Allow-Methods including POST"
else
    fail "Wildcard CORS methods" "Access-Control-Allow-Methods containing POST" "$CORS_METHODS"
fi

CORS_HEADERS_HDR=$(echo "$OPTIONS_HEADERS" | grep -i "Access-Control-Allow-Headers" || echo "")
if echo "$CORS_HEADERS_HDR" | grep -qi "Content-Type"; then
    pass "Wildcard CORS returns Allow-Headers including Content-Type"
else
    fail "Wildcard CORS headers" "Access-Control-Allow-Headers containing Content-Type" "$CORS_HEADERS_HDR"
fi

# Wildcard should NOT have Vary: Origin
VARY_HEADER=$(echo "$OPTIONS_HEADERS" | grep -i "^Vary:" | grep -i "Origin" || echo "")
if [ -z "$VARY_HEADER" ]; then
    pass "Wildcard CORS does not set Vary: Origin"
else
    fail "Wildcard Vary" "No Vary: Origin header" "$VARY_HEADER"
fi

# POST request should also have CORS headers
POST_CORS=$(curl -s -D - -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -H "Origin: https://any-origin.example.com" \
    -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' \
    2>/dev/null | grep -i "Access-Control-Allow-Origin" || echo "")

if echo "$POST_CORS" | grep -q '\*'; then
    pass "Wildcard CORS headers present on POST response"
else
    fail "Wildcard CORS POST" "Access-Control-Allow-Origin: *" "$POST_CORS"
fi

stop_server

# ==========================================
# Test 10: CORS specific origin - matching
# ==========================================
echo -e "${YELLOW}Test 10: CORS specific origin - matching origin${NC}"
PORT=18089
start_server $PORT '{"cors_origins": "https://allowed.example.com, https://also-allowed.example.com"}'

# Request from allowed origin should get CORS headers echoing that origin
OPTIONS_HEADERS=$(curl -s -I -X OPTIONS http://localhost:$PORT/mcp \
    -H "Origin: https://allowed.example.com" \
    2>/dev/null)

CORS_ORIGIN=$(echo "$OPTIONS_HEADERS" | grep -i "Access-Control-Allow-Origin" || echo "")
if echo "$CORS_ORIGIN" | grep -q "https://allowed.example.com"; then
    pass "Specific CORS echoes matching origin"
else
    fail "Specific CORS origin" "Access-Control-Allow-Origin: https://allowed.example.com" "$CORS_ORIGIN"
fi

# Should have Vary: Origin for specific origins
VARY_HEADER=$(echo "$OPTIONS_HEADERS" | grep -i "^Vary:" | grep -i "Origin" || echo "")
if [ -n "$VARY_HEADER" ]; then
    pass "Specific CORS sets Vary: Origin"
else
    fail "Specific CORS Vary" "Vary: Origin header" "$VARY_HEADER"
fi

# Second allowed origin should also work
CORS_ORIGIN2=$(curl -s -I -X OPTIONS http://localhost:$PORT/mcp \
    -H "Origin: https://also-allowed.example.com" \
    2>/dev/null | grep -i "Access-Control-Allow-Origin" || echo "")

if echo "$CORS_ORIGIN2" | grep -q "https://also-allowed.example.com"; then
    pass "Specific CORS echoes second allowed origin"
else
    fail "Specific CORS second origin" "Access-Control-Allow-Origin: https://also-allowed.example.com" "$CORS_ORIGIN2"
fi

stop_server

# ==========================================
# Test 11: CORS specific origin - non-matching
# ==========================================
echo -e "${YELLOW}Test 11: CORS specific origin - non-matching origin${NC}"
PORT=18090
start_server $PORT '{"cors_origins": "https://allowed.example.com"}'

# Request from disallowed origin should NOT get CORS headers
OPTIONS_HEADERS=$(curl -s -I -X OPTIONS http://localhost:$PORT/mcp \
    -H "Origin: https://evil.example.com" \
    2>/dev/null)

CORS_ORIGIN=$(echo "$OPTIONS_HEADERS" | grep -i "Access-Control-Allow-Origin" || echo "")
if [ -z "$CORS_ORIGIN" ]; then
    pass "Non-matching origin gets no CORS headers"
else
    fail "Non-matching CORS" "No Access-Control-Allow-Origin header" "$CORS_ORIGIN"
fi

# POST from disallowed origin should also not get CORS headers
POST_CORS=$(curl -s -D - -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -H "Origin: https://evil.example.com" \
    -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' \
    2>/dev/null | grep -i "Access-Control-Allow-Origin" || echo "")

if [ -z "$POST_CORS" ]; then
    pass "Non-matching origin gets no CORS headers on POST"
else
    fail "Non-matching CORS POST" "No Access-Control-Allow-Origin header" "$POST_CORS"
fi

stop_server

# Status code of one request. Usage: http_status METHOD PATH [curl args...]
http_status() {
    local method=$1 path=$2; shift 2
    curl -s -o /dev/null -w "%{http_code}" -X "$method" "http://localhost:$PORT$path" "$@" 2>/dev/null || echo "000"
}
# Expect one status. Usage: expect_status NAME WANT METHOD PATH [curl args...]
expect_status() {
    local name=$1 want=$2; shift 2
    local got
    got=$(http_status "$@")
    if [ "$got" = "$want" ]; then pass "$name ($want)"; else fail "$name" "$want" "$got"; fi
}
INIT_BODY='{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'

# ==========================================
# Test 11b: MCP-Protocol-Version header (#107)
# ==========================================
# 2025-06-18+: an invalid or unsupported version header MUST get 400. A missing
# header is accepted: 2024-11-05 clients never send one.
echo -e "${YELLOW}Test 11b: MCP-Protocol-Version header${NC}"
PORT=18093
start_server $PORT

expect_status "No version header accepted" 200 POST /mcp -H "Content-Type: application/json" -d "$INIT_BODY"
expect_status "Version 2025-11-25 accepted" 200 POST /mcp -H "Content-Type: application/json" -H "MCP-Protocol-Version: 2025-11-25" -d "$INIT_BODY"
expect_status "Version 2024-11-05 accepted" 200 POST /mcp -H "Content-Type: application/json" -H "MCP-Protocol-Version: 2024-11-05" -d "$INIT_BODY"
expect_status "Version 2025-03-26 rejected (not offered)" 400 POST /mcp -H "Content-Type: application/json" -H "MCP-Protocol-Version: 2025-03-26" -d "$INIT_BODY"
expect_status "Garbage version rejected" 400 POST /mcp -H "Content-Type: application/json" -H "MCP-Protocol-Version: banana" -d "$INIT_BODY"

stop_server

# ==========================================
# Test 11c: Origin validation (#107)
# ==========================================
# 2025-11-25: Origin present and invalid MUST get 403, on all incoming
# connections (DNS rebinding). origin_policy decides what an Origin that is not
# in cors_origins gets: reject (default) | loopback | allow.
echo -e "${YELLOW}Test 11c: Origin validation${NC}"

PORT=18094
start_server $PORT
expect_status "Default: no Origin passes" 200 POST /mcp -H "Content-Type: application/json" -d "$INIT_BODY"
expect_status "Default: foreign Origin on POST" 403 POST /mcp -H "Content-Type: application/json" -H "Origin: https://evil.example.com" -d "$INIT_BODY"
expect_status "Default: localhost Origin on POST" 403 POST /mcp -H "Content-Type: application/json" -H "Origin: http://localhost:3000" -d "$INIT_BODY"
expect_status "Default: foreign Origin on /health" 403 GET /health -H "Origin: https://evil.example.com"
expect_status "Default: foreign Origin on GET /mcp" 403 GET /mcp -H "Origin: https://evil.example.com"
expect_status "Default: no Origin on /health" 200 GET /health
stop_server

PORT=18095
start_server $PORT '{"cors_origins": "https://allowed.example.com"}'
expect_status "Listed Origin passes" 200 POST /mcp -H "Content-Type: application/json" -H "Origin: https://allowed.example.com" -d "$INIT_BODY"
expect_status "Unlisted Origin rejected" 403 POST /mcp -H "Content-Type: application/json" -H "Origin: https://evil.example.com" -d "$INIT_BODY"
expect_status "Unlisted Origin preflight rejected" 403 OPTIONS /mcp -H "Origin: https://evil.example.com"
stop_server

PORT=18096
start_server $PORT '{"origin_policy": "loopback"}'
expect_status "Loopback: localhost passes" 200 POST /mcp -H "Content-Type: application/json" -H "Origin: http://localhost:3000" -d "$INIT_BODY"
expect_status "Loopback: 127.0.0.1 passes" 200 POST /mcp -H "Content-Type: application/json" -H "Origin: http://127.0.0.1:8080" -d "$INIT_BODY"
expect_status "Loopback: [::1] passes" 200 POST /mcp -H "Content-Type: application/json" -H "Origin: http://[::1]:8080" -d "$INIT_BODY"
expect_status "Loopback: lookalike host rejected" 403 POST /mcp -H "Content-Type: application/json" -H "Origin: http://localhost.evil.com" -d "$INIT_BODY"
expect_status "Loopback: foreign Origin rejected" 403 POST /mcp -H "Content-Type: application/json" -H "Origin: https://evil.example.com" -d "$INIT_BODY"
stop_server

PORT=18097
start_server $PORT '{"origin_policy": "allow"}'
expect_status "Allow: foreign Origin passes" 200 POST /mcp -H "Content-Type: application/json" -H "Origin: https://evil.example.com" -d "$INIT_BODY"
stop_server

PORT=18098
start_server $PORT '{"auth_token":"secret123"}'
# Origin runs before auth: a rebinding page gets 403 without reaching the token check
expect_status "Origin checked before auth" 403 POST /mcp -H "Content-Type: application/json" -H "Origin: https://evil.example.com" -d "$INIT_BODY"
stop_server

# ==========================================
# Test 12: Alternative endpoint (/mcp)
# ==========================================
echo -e "${YELLOW}Test 12: Alternative endpoint (/mcp)${NC}"
PORT=18091
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
