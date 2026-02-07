#!/bin/bash
# Test script for HTTP server with background: false
# Verifies that mcp_server_start blocks when background: false is set

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
    if [ -n "$DUCKDB_PID" ] && ps -p $DUCKDB_PID > /dev/null 2>&1; then
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
    if [ -n "$2" ]; then
        echo "  Expected: $2"
        echo "  Got: $3"
    fi
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

echo "=========================================="
echo "DuckDB MCP HTTP Foreground Mode Tests"
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
# Test 1: background: false should block
# ==========================================
echo -e "${YELLOW}Test 1: HTTP server with background: false should block${NC}"
PORT=19098

# Start DuckDB with foreground HTTP server in background
# If background:false works, DuckDB should NOT exit immediately
timeout 10s "$DUCKDB" -unsigned -c "
    LOAD '$EXTENSION';
    SELECT mcp_server_start('http', 'localhost', $PORT, '{\"background\": false}');
    SELECT 'SHOULD_NOT_REACH';
" &
DUCKDB_PID=$!

# Wait for server to start
sleep 2

# Test 1a: Verify the process is still running (blocked, not returned)
if ps -p $DUCKDB_PID > /dev/null 2>&1; then
    pass "DuckDB process is still running (blocking as expected)"
else
    fail "DuckDB process exited immediately (not blocking)"
fi

# Test 1b: Verify server is accessible
HEALTH=$(curl -s http://localhost:$PORT/health 2>/dev/null || echo "CURL_ERROR")
if [ "$HEALTH" = '{"status":"ok"}' ]; then
    pass "Server is accessible and responding"
else
    fail "Server not accessible" '{"status":"ok"}' "$HEALTH"
fi

# Test 1c: Verify server can handle MCP requests
RESPONSE=$(curl -s -X POST http://localhost:$PORT/mcp \
    -H "Content-Type: application/json" \
    -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"query","arguments":{"sql":"SELECT 42 as answer"}}}' \
    2>/dev/null || echo "CURL_ERROR")

if echo "$RESPONSE" | grep -q 'answer.*42'; then
    pass "Server can process MCP requests"
else
    fail "Server MCP request failed" "response containing answer 42" "$RESPONSE"
fi

# Cleanup
kill $DUCKDB_PID 2>/dev/null || true
wait $DUCKDB_PID 2>/dev/null || true
unset DUCKDB_PID

# ==========================================
# Test 2: Verify background: true still works (control test)
# ==========================================
echo -e "${YELLOW}Test 2: HTTP server with background: true should return immediately${NC}"
PORT=19099

# Capture output to check if command after mcp_server_start runs
OUTPUT=$(timeout 5s "$DUCKDB" -unsigned -c "
    LOAD '$EXTENSION';
    SELECT mcp_server_start('http', 'localhost', $PORT, '{\"background\": true}');
    SELECT 'AFTER_SERVER_START' as marker;
    SELECT mcp_server_stop();
" 2>&1 || true)

if echo "$OUTPUT" | grep -q "AFTER_SERVER_START"; then
    pass "background: true returns immediately (subsequent SQL executed)"
else
    fail "background: true did not return" "AFTER_SERVER_START in output" "$OUTPUT"
fi

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
