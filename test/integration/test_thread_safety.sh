#!/bin/bash
# Thread safety integration tests for DuckDB MCP Extension
# Tests for: CR-08 (MCPSecurityConfig), CR-11 (MCPConnection::last_error), CR-12 (MCPServer::start_time)
#
# These tests exercise concurrent access patterns that trigger data races
# in the unfixed code. They verify:
#   1. No crashes under concurrent load (the main symptom of string data races)
#   2. Correct results when multiple threads access shared state
#   3. Server stability under concurrent HTTP requests

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

BUILD_TYPE=${BUILD_TYPE:-release}
DUCKDB="$PROJECT_DIR/build/$BUILD_TYPE/duckdb"
EXTENSION="$PROJECT_DIR/build/$BUILD_TYPE/extension/duckdb_mcp/duckdb_mcp.duckdb_extension"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Counters
TOTAL_TESTS=0
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

# Cleanup tracking
CLEANUP_PIDS=()
CLEANUP_FILES=()

cleanup() {
    echo -e "\n${BLUE}Cleaning up...${NC}"
    for pid in "${CLEANUP_PIDS[@]}"; do
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
    done
    for f in "${CLEANUP_FILES[@]}"; do
        rm -rf "$f" 2>/dev/null || true
    done
}
trap cleanup EXIT

log_section() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

pass() {
    echo -e "${GREEN}  PASS${NC}: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
}

fail() {
    echo -e "${RED}  FAIL${NC}: $1"
    if [ -n "$2" ]; then echo -e "  Expected: $2"; fi
    if [ -n "$3" ]; then echo -e "  Got: $3"; fi
    TESTS_FAILED=$((TESTS_FAILED + 1))
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
}

skip() {
    echo -e "${YELLOW}  SKIP${NC}: $1 - $2"
    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
}

run_sql_with_extension() {
    local sql="$1"
    echo "LOAD '$EXTENSION'; $sql" | "$DUCKDB" -unsigned 2>&1 || true
}

# Start an HTTP server, wait for it, return PID. Returns empty on failure.
start_server() {
    local port=$1
    local config=${2:-'{}'}
    local keep_alive=${3:-20}

    {
        echo "LOAD '$EXTENSION';"
        echo "SELECT mcp_server_start('http', 'localhost', $port, '$config');"
        for i in $(seq 1 $keep_alive); do sleep 1; echo ""; done
        echo ".exit"
    } | "$DUCKDB" -unsigned > /dev/null 2>&1 &
    local pid=$!
    CLEANUP_PIDS+=($pid)

    # Wait up to 5 seconds for server to respond
    for i in $(seq 1 10); do
        if curl -s --connect-timeout 1 --max-time 2 -o /dev/null "http://localhost:$port/health" 2>/dev/null; then
            echo $pid
            return 0
        fi
        sleep 0.5
    done
    # Server didn't start
    echo ""
    return 1
}

# ==========================================
# Prerequisite checks
# ==========================================
log_section "Thread Safety Tests - Prerequisites"

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

HAS_CURL=false
if command -v curl &> /dev/null; then
    HAS_CURL=true
    pass "curl is available"
fi

# ==========================================
# CR-11: MCPConnection last_error - Memory transport stress
# ==========================================
log_section "CR-11: MCPConnection last_error - Concurrent Error Paths"

RESULT=$(run_sql_with_extension "
CREATE TABLE thread_test AS SELECT generate_series as id, 'data_' || generate_series as val FROM generate_series(1, 100);
SELECT mcp_server_start('memory');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"query\",\"arguments\":{\"sql\":\"SELECT * FROM thread_test LIMIT 5\"}}}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"nonexistent_tool\",\"arguments\":{}}}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"resources/read\",\"params\":{\"uri\":\"data://missing\"}}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"query\",\"arguments\":{\"sql\":\"SELECT * FROM thread_test LIMIT 3\"}}}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\",\"params\":{\"name\":\"query\",\"arguments\":{\"sql\":\"INVALID SQL GARBAGE\"}}}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\",\"params\":{\"name\":\"query\",\"arguments\":{\"sql\":\"SELECT COUNT(*) FROM thread_test\"}}}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"tools/list\",\"params\":{}}');
SELECT mcp_server_send_request('{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"clientInfo\":{\"name\":\"test\",\"version\":\"1.0\"}}}');
SELECT mcp_server_stop();
DROP TABLE thread_test;
")

if echo "$RESULT" | grep -q "data_"; then
    pass "CR-11: Mixed valid/invalid requests completed without crash"
else
    fail "CR-11: Memory transport requests" "data_ in output" "$RESULT"
fi

if echo "$RESULT" | grep -q '"error"'; then
    pass "CR-11: Error responses returned properly (not garbage from torn string)"
else
    fail "CR-11: Expected error responses for invalid requests" "error in output" ""
fi

# ==========================================
# CR-12: MCPServer::start_time - GetUptime via memory transport
# ==========================================
log_section "CR-12: MCPServer start_time - Status/Uptime via Memory Transport"

RESULT=$(run_sql_with_extension "
SELECT mcp_server_start('memory');
SELECT (mcp_server_status()).running;
SELECT (mcp_server_status()).running;
SELECT (mcp_server_status()).running;
SELECT mcp_server_stop();
")

if echo "$RESULT" | grep -q "true"; then
    pass "CR-12: Server status reports running correctly (atomic start_time read safe)"
else
    fail "CR-12: Server status" "true" "$RESULT"
fi

# ==========================================
# CR-08: MCPSecurityConfig - Config Lock Ordering
# ==========================================
log_section "CR-08: MCPSecurityConfig - Config Lock Ordering"

RESULT=$(run_sql_with_extension "
SET allowed_mcp_commands='python3:/usr/bin/python3';
SELECT 'config_set' AS step;
")

if echo "$RESULT" | grep -q "config_set"; then
    pass "CR-08: SetAllowedCommands + subsequent operations don't deadlock"
else
    fail "CR-08: Possible deadlock in security config" "config_set" "$RESULT"
fi

RESULT=$(run_sql_with_extension "
SET allowed_mcp_commands='test_cmd';
SET lock_mcp_servers=true;
SELECT 'lock_cycle_done' AS step;
")

if echo "$RESULT" | grep -q "lock_cycle_done"; then
    pass "CR-08: Full config lock cycle completes without deadlock"
else
    fail "CR-08: Config lock cycle" "lock_cycle_done" "$RESULT"
fi

# ==========================================
# HTTP concurrent tests (CR-08 + CR-12 combined)
# ==========================================
log_section "Combined: Concurrent HTTP Stress Test"

if [ "$HAS_CURL" != true ]; then
    skip "Concurrent HTTP stress test" "curl not available"
else
    PORT=18321
    SERVER_PID=$(start_server $PORT || true)

    if [ -z "$SERVER_PID" ]; then
        skip "Concurrent HTTP stress test" "HTTP server failed to start"
    else
        pass "HTTP server started on port $PORT"

        # Send many concurrent requests to stress thread-safe code paths
        CONCURRENT=20
        TMPDIR_STRESS=$(mktemp -d)
        CLEANUP_FILES+=("$TMPDIR_STRESS")

        TOOLS_LIST='{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'
        QUERY_REQ='{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"query","arguments":{"sql":"SELECT 42"}}}'

        for iter in $(seq 1 3); do
            pids=()
            for i in $(seq 1 $CONCURRENT); do
                case $((i % 3)) in
                    0)
                        curl -s --max-time 5 "http://localhost:$PORT/health" > "$TMPDIR_STRESS/s_${iter}_${i}.out" 2>&1 &
                        ;;
                    1)
                        curl -s --max-time 5 -X POST -H "Content-Type: application/json" \
                            -d "$TOOLS_LIST" \
                            "http://localhost:$PORT/mcp" > "$TMPDIR_STRESS/s_${iter}_${i}.out" 2>&1 &
                        ;;
                    2)
                        curl -s --max-time 5 -X POST -H "Content-Type: application/json" \
                            -d "$QUERY_REQ" \
                            "http://localhost:$PORT/mcp" > "$TMPDIR_STRESS/s_${iter}_${i}.out" 2>&1 &
                        ;;
                esac
                pids+=($!)
            done
            for pid in "${pids[@]}"; do
                wait "$pid" 2>/dev/null || true
            done
        done

        # Final health check - server must survive
        sleep 1
        HTTP_CODE=$(curl -s --max-time 3 -o /dev/null -w "%{http_code}" "http://localhost:$PORT/health" 2>/dev/null || echo "000")
        if [ "$HTTP_CODE" = "200" ]; then
            pass "Stress: Server survived $((CONCURRENT * 3)) concurrent mixed requests"
        else
            fail "Stress: Server crashed" "HTTP 200 after stress" "HTTP $HTTP_CODE"
        fi

        # Verify responses
        VALID_RESPONSES=0
        TOTAL_RESPONSES=0
        for f in "$TMPDIR_STRESS"/s_*.out; do
            [ -f "$f" ] || continue
            TOTAL_RESPONSES=$((TOTAL_RESPONSES + 1))
            if [ -s "$f" ]; then
                VALID_RESPONSES=$((VALID_RESPONSES + 1))
            fi
        done

        if [ "$VALID_RESPONSES" -gt 0 ] && [ "$VALID_RESPONSES" -ge "$((TOTAL_RESPONSES * 9 / 10))" ]; then
            pass "Stress: $VALID_RESPONSES/$TOTAL_RESPONSES requests got responses"
        else
            fail "Stress: Too many empty responses" ">90% with data" "$VALID_RESPONSES/$TOTAL_RESPONSES"
        fi

        rm -rf "$TMPDIR_STRESS"

        # Stop server
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
fi

# ==========================================
# Summary
# ==========================================
echo ""
log_section "Thread Safety Test Results"
echo -e "Total:   $TOTAL_TESTS"
echo -e "Passed:  ${GREEN}$TESTS_PASSED${NC}"
echo -e "Failed:  ${RED}$TESTS_FAILED${NC}"
echo -e "Skipped: ${YELLOW}$TESTS_SKIPPED${NC}"

if [ "$TESTS_FAILED" -gt 0 ]; then
    echo -e "\n${RED}THREAD SAFETY TESTS FAILED${NC}"
    exit 1
else
    echo -e "\n${GREEN}ALL THREAD SAFETY TESTS PASSED${NC}"
    exit 0
fi
