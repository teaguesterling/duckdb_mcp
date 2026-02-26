#!/bin/bash
# Integration test runner for DuckDB MCP Extension
# Runs comprehensive tests for HTTP transport, memory transport, and client-server communication

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
DUCKDB="$PROJECT_DIR/build/release/duckdb"
EXTENSION="$PROJECT_DIR/build/release/extension/duckdb_mcp/duckdb_mcp.duckdb_extension"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TOTAL_TESTS=0
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

# Cleanup tracking
CLEANUP_PIDS=()

cleanup() {
    echo -e "\n${BLUE}Cleaning up...${NC}"
    for pid in "${CLEANUP_PIDS[@]}"; do
        kill $pid 2>/dev/null || true
        wait $pid 2>/dev/null || true
    done
}
trap cleanup EXIT

# Test helper functions
log_section() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
}

fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    if [ -n "$2" ]; then
        echo -e "  Expected: $2"
    fi
    if [ -n "$3" ]; then
        echo -e "  Got: $3"
    fi
    TESTS_FAILED=$((TESTS_FAILED + 1))
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
}

skip() {
    echo -e "${YELLOW}○ SKIP${NC}: $1 - $2"
    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
}

run_sql() {
    local sql="$1"
    echo "$sql" | "$DUCKDB" -unsigned 2>&1 || true
}

run_sql_with_extension() {
    local sql="$1"
    echo "LOAD '$EXTENSION'; $sql" | "$DUCKDB" -unsigned 2>&1 || true
}

# Start a DuckDB server in background and return its PID
start_server() {
    local port=$1
    local config=${2:-'{}'}
    local keep_alive=${3:-20}

    {
        echo "LOAD '$EXTENSION';"
        echo "SELECT mcp_server_start('http', 'localhost', $port, '$config');"
        for i in $(seq 1 $keep_alive); do sleep 1; echo ""; done
        echo ".exit"
    } | "$DUCKDB" -unsigned &
    local pid=$!
    CLEANUP_PIDS+=($pid)
    sleep 2
    echo $pid
}

# ==========================================
# Prerequisite checks
# ==========================================
log_section "Prerequisite Checks"

if [ ! -f "$DUCKDB" ]; then
    echo -e "${RED}Error: DuckDB binary not found at $DUCKDB${NC}"
    echo "Please run 'make' first to build the extension."
    exit 1
fi
pass "DuckDB binary exists"

if [ ! -f "$EXTENSION" ]; then
    echo -e "${RED}Error: Extension not found at $EXTENSION${NC}"
    echo "Please run 'make' first to build the extension."
    exit 1
fi
pass "Extension file exists"

if ! command -v curl &> /dev/null; then
    echo -e "${YELLOW}Warning: curl not installed, HTTP tests will be skipped${NC}"
    SKIP_HTTP=true
else
    pass "curl is available"
    SKIP_HTTP=false
fi

# ==========================================
# Extension Loading Tests
# ==========================================
log_section "Extension Loading Tests"

RESULT=$(run_sql_with_extension "SELECT 'loaded' as status;")
if echo "$RESULT" | grep -q "loaded"; then
    pass "Extension loads successfully"
else
    fail "Extension loading" "loaded" "$RESULT"
fi

# Check version
RESULT=$(run_sql_with_extension "SELECT mcp_server_status();")
if echo "$RESULT" | grep -q "stopped\|running"; then
    pass "mcp_server_status() works"
else
    fail "mcp_server_status()" "status output" "$RESULT"
fi

# ==========================================
# Memory Transport Tests (Server)
# ==========================================
log_section "Memory Transport Tests"

# Test server start with memory transport
RESULT=$(run_sql_with_extension "SELECT mcp_server_start('memory', 'localhost', 0, '{}');")
if echo "$RESULT" | grep -q "success.*true"; then
    pass "Memory transport server starts"
else
    fail "Memory transport start" "success: true" "$RESULT"
fi

# Test tools/list via memory transport
RESULT=$(run_sql_with_extension "
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{}}');
")
if echo "$RESULT" | grep -q '"name":"query"'; then
    pass "Memory transport: tools/list returns query tool"
else
    fail "Memory transport: tools/list" "query tool in response" "$RESULT"
fi

# Test query execution via memory transport
RESULT=$(run_sql_with_extension "
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"query\",\"arguments\":{\"sql\":\"SELECT 42 as answer\"}}}');
")
# Note: JSON is escaped in the response text field, so we check for the escaped pattern
if echo "$RESULT" | grep -q 'answer.*42'; then
    pass "Memory transport: query execution works"
else
    fail "Memory transport: query" "answer: 42" "$RESULT"
fi

# Test describe tool via memory transport
RESULT=$(run_sql_with_extension "
CREATE TABLE test_describe (id INT, name VARCHAR);
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"describe\",\"arguments\":{\"table\":\"test_describe\"}}}');
")
if echo "$RESULT" | grep -q 'name.*id'; then
    pass "Memory transport: describe tool works"
else
    fail "Memory transport: describe" "column info" "$RESULT"
fi

# ==========================================
# Stdio Transport Tests
# ==========================================
log_section "Stdio Transport Tests"

# Test stdio transport with initialize request
# For stdio, we pipe JSON-RPC requests to the process and read responses
STDIO_RESULT=$(echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}' | \
    "$DUCKDB" -unsigned -cmd "LOAD '$EXTENSION';" -cmd "SELECT mcp_server_start('stdio', 'localhost', 0, '{}');" 2>&1 || true)
if echo "$STDIO_RESULT" | grep -q '"serverInfo"'; then
    pass "Stdio transport: Initialize returns serverInfo"
else
    fail "Stdio transport: Initialize" "serverInfo in response" "$STDIO_RESULT"
fi

# Test stdio transport with tools/list
STDIO_RESULT=$(echo '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' | \
    "$DUCKDB" -unsigned -cmd "LOAD '$EXTENSION';" -cmd "SELECT mcp_server_start('stdio', 'localhost', 0, '{}');" 2>&1 || true)
if echo "$STDIO_RESULT" | grep -q '"name":"query"'; then
    pass "Stdio transport: tools/list returns query tool"
else
    fail "Stdio transport: tools/list" "query tool" "$STDIO_RESULT"
fi

# Test stdio transport with query execution
STDIO_RESULT=$(echo '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"query","arguments":{"sql":"SELECT 99 as value"}}}' | \
    "$DUCKDB" -unsigned -cmd "LOAD '$EXTENSION';" -cmd "SELECT mcp_server_start('stdio', 'localhost', 0, '{}');" 2>&1 || true)
if echo "$STDIO_RESULT" | grep -q 'value.*99'; then
    pass "Stdio transport: Query execution works"
else
    fail "Stdio transport: Query" "value: 99" "$STDIO_RESULT"
fi

# Test stdio with multiple requests (batch)
STDIO_RESULT=$(printf '%s\n%s' \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}' \
    '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' | \
    "$DUCKDB" -unsigned -cmd "LOAD '$EXTENSION';" -cmd "SELECT mcp_server_start('stdio', 'localhost', 0, '{}');" 2>&1 || true)
if echo "$STDIO_RESULT" | grep -q '"serverInfo"' && echo "$STDIO_RESULT" | grep -q '"name":"query"'; then
    pass "Stdio transport: Multiple requests work"
else
    fail "Stdio transport: Multiple requests" "both responses" "$STDIO_RESULT"
fi

# Test stdio with describe tool
STDIO_RESULT=$(printf '%s\n%s' \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}' \
    '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"describe","arguments":{"query":"SELECT 1 as col"}}}' | \
    "$DUCKDB" -unsigned -cmd "LOAD '$EXTENSION';" -cmd "SELECT mcp_server_start('stdio', 'localhost', 0, '{}');" 2>&1 || true)
if echo "$STDIO_RESULT" | grep -q 'name.*col'; then
    pass "Stdio transport: Describe tool works"
else
    fail "Stdio transport: Describe" "column info" "$STDIO_RESULT"
fi

# ==========================================
# HTTP Transport Tests
# ==========================================
log_section "HTTP Transport Tests"

if [ "$SKIP_HTTP" = true ]; then
    skip "HTTP tests" "curl not available"
else
    PORT=19080

    # Start HTTP server
    start_server $PORT '{}' 30

    # Test health endpoint
    RESPONSE=$(curl -s http://localhost:$PORT/health 2>/dev/null || echo "CURL_ERROR")
    if [ "$RESPONSE" = '{"status":"ok"}' ]; then
        pass "HTTP: Health endpoint responds"
    else
        fail "HTTP: Health endpoint" '{"status":"ok"}' "$RESPONSE"
    fi

    # Test initialize
    RESPONSE=$(curl -s -X POST http://localhost:$PORT/mcp \
        -H "Content-Type: application/json" \
        -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}' \
        2>/dev/null || echo "CURL_ERROR")
    if echo "$RESPONSE" | grep -q '"serverInfo"'; then
        pass "HTTP: Initialize returns serverInfo"
    else
        fail "HTTP: Initialize" "serverInfo in response" "$RESPONSE"
    fi

    # Test tools/list
    RESPONSE=$(curl -s -X POST http://localhost:$PORT/mcp \
        -H "Content-Type: application/json" \
        -d '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' \
        2>/dev/null || echo "CURL_ERROR")
    if echo "$RESPONSE" | grep -q '"name":"query"'; then
        pass "HTTP: tools/list returns tools"
    else
        fail "HTTP: tools/list" "query tool" "$RESPONSE"
    fi

    # Test query
    RESPONSE=$(curl -s -X POST http://localhost:$PORT/mcp \
        -H "Content-Type: application/json" \
        -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"query","arguments":{"sql":"SELECT 123 as num"}}}' \
        2>/dev/null || echo "CURL_ERROR")
    if echo "$RESPONSE" | grep -q 'num.*123'; then
        pass "HTTP: Query execution works"
    else
        fail "HTTP: Query" "num: 123" "$RESPONSE"
    fi

    # Kill first server
    kill ${CLEANUP_PIDS[-1]} 2>/dev/null || true
    sleep 1
fi

# ==========================================
# HTTP Authentication Tests
# ==========================================
log_section "HTTP Authentication Tests"

if [ "$SKIP_HTTP" = true ]; then
    skip "HTTP auth tests" "curl not available"
else
    PORT=19081

    # Start server with auth
    start_server $PORT '{"auth_token":"test-secret-token"}' 30

    # Test without auth (should get 401)
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:$PORT/mcp \
        -H "Content-Type: application/json" \
        -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
        2>/dev/null || echo "000")
    if [ "$HTTP_CODE" = "401" ]; then
        pass "HTTP Auth: No credentials returns 401"
    else
        fail "HTTP Auth: No credentials" "401" "$HTTP_CODE"
    fi

    # Test with wrong auth (should get 403)
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:$PORT/mcp \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer wrong-token" \
        -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
        2>/dev/null || echo "000")
    if [ "$HTTP_CODE" = "403" ]; then
        pass "HTTP Auth: Wrong credentials returns 403"
    else
        fail "HTTP Auth: Wrong credentials" "403" "$HTTP_CODE"
    fi

    # Test with correct auth (should get 200)
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:$PORT/mcp \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer test-secret-token" \
        -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}' \
        2>/dev/null || echo "000")
    if [ "$HTTP_CODE" = "200" ]; then
        pass "HTTP Auth: Correct credentials returns 200"
    else
        fail "HTTP Auth: Correct credentials" "200" "$HTTP_CODE"
    fi

    # Kill server
    kill ${CLEANUP_PIDS[-1]} 2>/dev/null || true
    sleep 1
fi

# ==========================================
# Custom Tool Publishing Tests
# ==========================================
log_section "Custom Tool Publishing Tests"

# Test mcp_publish_tool
RESULT=$(run_sql_with_extension "
SELECT mcp_publish_tool(
    'test_search',
    'Test search tool',
    'SELECT * FROM (VALUES (1, ''a''), (2, ''b'')) AS t(id, name) WHERE name = \$query',
    '{\"query\": {\"type\": \"string\", \"description\": \"Search term\"}}',
    '[\"query\"]'
);
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{}}');
")
if echo "$RESULT" | grep -q '"name":"test_search"'; then
    pass "Custom tool publishing works"
else
    fail "Custom tool publishing" "test_search in tools list" "$RESULT"
fi

# Test custom tool execution
RESULT=$(run_sql_with_extension "
SELECT mcp_publish_tool(
    'add_numbers',
    'Add two numbers',
    'SELECT \$a + \$b as result',
    '{\"a\": {\"type\": \"integer\"}, \"b\": {\"type\": \"integer\"}}',
    '[\"a\", \"b\"]'
);
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"add_numbers\",\"arguments\":{\"a\":5,\"b\":3}}}');
")
if echo "$RESULT" | grep -q 'result.*8'; then
    pass "Custom tool execution with parameters works"
else
    fail "Custom tool execution" "result: 8" "$RESULT"
fi

# ==========================================
# Resource Publishing Tests
# ==========================================
log_section "Resource Publishing Tests"

# Test mcp_publish_table
RESULT=$(run_sql_with_extension "
CREATE TABLE test_publish (id INT, value VARCHAR);
INSERT INTO test_publish VALUES (1, 'one'), (2, 'two');
SELECT mcp_publish_table('test_publish', 'data://test/table', 'json');
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"resources/list\",\"params\":{}}');
")
if echo "$RESULT" | grep -q 'data://test/table'; then
    pass "Table publishing works"
else
    fail "Table publishing" "data://test/table in resources" "$RESULT"
fi

# Test mcp_publish_resource (static content)
RESULT=$(run_sql_with_extension "
SELECT mcp_publish_resource('config://app', '{\"setting\": true}', 'application/json', 'App config');
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"resources/list\",\"params\":{}}');
")
if echo "$RESULT" | grep -q 'config://app'; then
    pass "Static resource publishing works"
else
    fail "Static resource publishing" "config://app in resources" "$RESULT"
fi

# ==========================================
# Output Format Tests
# ==========================================
log_section "Output Format Tests"

# Test JSON format
RESULT=$(run_sql_with_extension "
CREATE TABLE format_test (id INT, name VARCHAR);
INSERT INTO format_test VALUES (1, 'Alice'), (2, 'Bob');
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"query\",\"arguments\":{\"sql\":\"SELECT * FROM format_test\",\"format\":\"json\"}}}');
")
# Check for JSON array pattern with escaped quotes
if echo "$RESULT" | grep -q 'id.*1.*name.*Alice'; then
    pass "JSON output format works"
else
    fail "JSON format" "JSON array" "$RESULT"
fi

# Test Markdown format
RESULT=$(run_sql_with_extension "
CREATE TABLE format_test2 (id INT, name VARCHAR);
INSERT INTO format_test2 VALUES (1, 'Alice');
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"query\",\"arguments\":{\"sql\":\"SELECT * FROM format_test2\",\"format\":\"markdown\"}}}');
")
if echo "$RESULT" | grep -q '| id | name |'; then
    pass "Markdown output format works"
else
    fail "Markdown format" "markdown table" "$RESULT"
fi

# ==========================================
# Server Configuration Tests
# ==========================================
log_section "Server Configuration Tests"

# Test disabling tools
RESULT=$(run_sql_with_extension "
SELECT mcp_server_start('memory', 'localhost', 0, '{\"enable_query_tool\": false}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{}}');
")
if echo "$RESULT" | grep -q '"name":"query"'; then
    fail "Tool disabling" "query tool should not appear" "$RESULT"
else
    pass "Tool disabling works (query tool hidden)"
fi

# Test default format configuration
RESULT=$(run_sql_with_extension "
CREATE TABLE config_test (x INT);
INSERT INTO config_test VALUES (1);
SELECT mcp_server_start('memory', 'localhost', 0, '{\"default_result_format\": \"markdown\"}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"query\",\"arguments\":{\"sql\":\"SELECT * FROM config_test\"}}}');
")
if echo "$RESULT" | grep -q '| x |'; then
    pass "Default format configuration works"
else
    fail "Default format" "markdown table" "$RESULT"
fi

# ==========================================
# Error Handling Tests
# ==========================================
log_section "Error Handling Tests"

# Test invalid SQL
RESULT=$(run_sql_with_extension "
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"query\",\"arguments\":{\"sql\":\"SELECT * FROM nonexistent_table\"}}}');
")
if echo "$RESULT" | grep -q '"error"'; then
    pass "Invalid SQL returns error"
else
    fail "Invalid SQL error" "error in response" "$RESULT"
fi

# Test invalid method
RESULT=$(run_sql_with_extension "
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"invalid/method\",\"params\":{}}');
")
if echo "$RESULT" | grep -q '"error"\|Method not found'; then
    pass "Invalid method returns error"
else
    fail "Invalid method error" "error in response" "$RESULT"
fi

# ==========================================
# WebMCP Transport Tests (Native)
# ==========================================
log_section "WebMCP Transport Tests (Native)"

# Test 1: Starting webmcp transport fails gracefully in native build
RESULT=$(run_sql_with_extension "SELECT mcp_server_start('webmcp');")
if echo "$RESULT" | grep -q "success.*false"; then
    pass "WebMCP: start('webmcp') fails gracefully in native"
else
    fail "WebMCP: start('webmcp') native failure" "success: false" "$RESULT"
fi

# Test 2: Server not left running after failed webmcp start
RESULT=$(run_sql_with_extension "
SELECT mcp_server_start('webmcp');
SELECT (mcp_server_status()).running;
")
if echo "$RESULT" | grep -q "false"; then
    pass "WebMCP: server not left running after failed start"
else
    fail "WebMCP: server state after failed start" "running: false" "$RESULT"
fi

# Test 3: Memory transport still works after failed webmcp start
RESULT=$(run_sql_with_extension "
SELECT mcp_server_start('webmcp');
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
")
if echo "$RESULT" | grep -q "success.*true"; then
    pass "WebMCP: memory transport works after failed webmcp start"
else
    fail "WebMCP: memory after webmcp" "success: true" "$RESULT"
fi

# Test 4: WASM-only functions produce Catalog Error in native
RESULT=$(run_sql_with_extension "SELECT mcp_webmcp_sync();")
if echo "$RESULT" | grep -q "Catalog Error"; then
    pass "WebMCP: mcp_webmcp_sync() produces Catalog Error in native"
else
    fail "WebMCP: mcp_webmcp_sync() native" "Catalog Error" "$RESULT"
fi

RESULT=$(run_sql_with_extension "SELECT webmcp_list_page_tools();")
if echo "$RESULT" | grep -q "Catalog Error"; then
    pass "WebMCP: webmcp_list_page_tools() produces Catalog Error in native"
else
    fail "WebMCP: webmcp_list_page_tools() native" "Catalog Error" "$RESULT"
fi

# Test 5: tools/call via ProcessRequest — query tool (same path as HandleToolCall)
RESULT=$(run_sql_with_extension "
CREATE TABLE webmcp_test (id INTEGER, value VARCHAR);
INSERT INTO webmcp_test VALUES (1, 'alpha'), (2, 'beta');
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"query\",\"arguments\":{\"sql\":\"SELECT * FROM webmcp_test ORDER BY id\"}}}');
")
if echo "$RESULT" | grep -q "alpha" && echo "$RESULT" | grep -q "beta"; then
    pass "WebMCP path: query tool via ProcessRequest"
else
    fail "WebMCP path: query tool" "alpha and beta in response" "$RESULT"
fi

# Test 6: tools/call — describe tool
RESULT=$(run_sql_with_extension "
CREATE TABLE webmcp_desc (col_a INTEGER, col_b VARCHAR);
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"describe\",\"arguments\":{\"table\":\"webmcp_desc\"}}}');
")
if echo "$RESULT" | grep -q "col_a" && echo "$RESULT" | grep -q "col_b"; then
    pass "WebMCP path: describe tool via ProcessRequest"
else
    fail "WebMCP path: describe tool" "col_a and col_b in response" "$RESULT"
fi

# Test 7: tools/call — list_tables tool
RESULT=$(run_sql_with_extension "
CREATE TABLE webmcp_lt_test (x INT);
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"list_tables\",\"arguments\":{}}}');
")
if echo "$RESULT" | grep -q "webmcp_lt_test"; then
    pass "WebMCP path: list_tables tool via ProcessRequest"
else
    fail "WebMCP path: list_tables tool" "webmcp_lt_test in response" "$RESULT"
fi

# Test 8: Resource publishing + resources/read path
RESULT=$(run_sql_with_extension "
SELECT mcp_publish_resource('info://webmcp-test', 'WebMCP integration test content', 'text/plain', 'Test resource');
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"resources/read\",\"params\":{\"uri\":\"info://webmcp-test\"}}');
")
if echo "$RESULT" | grep -q "WebMCP integration test content"; then
    pass "WebMCP path: resources/read returns published content"
else
    fail "WebMCP path: resources/read" "WebMCP integration test content" "$RESULT"
fi

# Verify resource appears in resources/list
RESULT=$(run_sql_with_extension "
SELECT mcp_publish_resource('info://webmcp-list-test', 'list test', 'text/plain', 'For listing');
SELECT mcp_server_start('memory', 'localhost', 0, '{}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"resources/list\",\"params\":{}}');
")
if echo "$RESULT" | grep -q "info://webmcp-list-test"; then
    pass "WebMCP path: resources/list includes published resource"
else
    fail "WebMCP path: resources/list" "info://webmcp-list-test in response" "$RESULT"
fi

# Test 9: Prompt template registration + rendering
RESULT=$(run_sql_with_extension "
SELECT mcp_register_prompt_template('webmcp_greeting', 'A greeting prompt', 'Hello {name}, welcome to {place}!');
SELECT mcp_render_prompt_template('webmcp_greeting', '{\"name\": \"Alice\", \"place\": \"WebMCP\"}');
")
if echo "$RESULT" | grep -q "Hello Alice, welcome to WebMCP!"; then
    pass "WebMCP path: prompt template registration and rendering"
else
    fail "WebMCP path: prompt rendering" "Hello Alice, welcome to WebMCP!" "$RESULT"
fi

# ==========================================
# Summary
# ==========================================
log_section "Test Summary"

echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
echo -e "${RED}Failed: $TESTS_FAILED${NC}"
echo -e "${YELLOW}Skipped: $TESTS_SKIPPED${NC}"
echo -e "Total: $TOTAL_TESTS"

if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "\n${RED}Some tests failed!${NC}"
    exit 1
fi

echo -e "\n${GREEN}All tests passed!${NC}"
exit 0
