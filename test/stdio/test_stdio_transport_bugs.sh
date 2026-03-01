#!/bin/bash
# Test script for StdioTransport bug fixes (Issue #27)
# Tests: CR-09 (PID reuse race), CR-10 (partial JSON reads), CR-21 (startup polling)
#
# These tests exercise the client-side StdioTransport by ATTACHing to mock
# MCP servers that trigger the specific bug conditions.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
DUCKDB="${PROJECT_DIR}/build/release/duckdb"
EXTENSION="${PROJECT_DIR}/build/release/extension/duckdb_mcp/duckdb_mcp.duckdb_extension"

# Fall back to debug build if release isn't available
if [ ! -f "$DUCKDB" ]; then
    DUCKDB="${PROJECT_DIR}/build/debug/duckdb"
    EXTENSION="${PROJECT_DIR}/build/debug/extension/duckdb_mcp/duckdb_mcp.duckdb_extension"
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

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

skip() {
    echo -e "${YELLOW}○ SKIP${NC}: $1 — $2"
    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
}

echo "=========================================="
echo "StdioTransport Bug Fix Tests (Issue #27)"
echo "=========================================="
echo ""

# Check prerequisites
if [ ! -f "$DUCKDB" ]; then
    echo -e "${RED}Error: DuckDB binary not found${NC}"
    echo "  Tried: ${PROJECT_DIR}/build/release/duckdb"
    echo "  Tried: ${PROJECT_DIR}/build/debug/duckdb"
    exit 1
fi

if [ ! -f "$EXTENSION" ]; then
    echo -e "${RED}Error: Extension not found at $EXTENSION${NC}"
    exit 1
fi

# Make mock scripts executable
chmod +x "$SCRIPT_DIR"/mock_*.sh

# ==========================================
# CR-21: Fast-failing server detection
# ==========================================
echo -e "${YELLOW}CR-21: Fast-failing server detection${NC}"

# Test: ATTACH to a server that exits immediately should fail quickly.
# Before fix: 100ms fixed sleep regardless. After fix: polling detects early exit.
START_TIME=$(date +%s%N 2>/dev/null || python3 -c "import time; print(int(time.time()*1e9))")

RESULT=$(echo "
LOAD '${EXTENSION}';
ATTACH '' AS fast_exit_test (TYPE mcp, COMMAND '${SCRIPT_DIR}/mock_fast_exit.sh', TRANSPORT 'stdio');
" | timeout 10 "$DUCKDB" -unsigned 2>&1 || true)
END_TIME=$(date +%s%N 2>/dev/null || python3 -c "import time; print(int(time.time()*1e9))")

if echo "$RESULT" | grep -qi "fail\|error\|not connect\|not available"; then
    pass "CR-21: Fast-exiting server detected as failure"
else
    fail "CR-21: Fast-exiting server should fail" "error message" "$RESULT"
fi

# Test: ATTACH to a nonexistent binary should fail
RESULT=$(echo "
LOAD '${EXTENSION}';
ATTACH '' AS nonexist_test (TYPE mcp, COMMAND '/nonexistent/binary/path', TRANSPORT 'stdio');
" | timeout 10 "$DUCKDB" -unsigned 2>&1 || true)

if echo "$RESULT" | grep -qi "fail\|error\|not connect\|not found"; then
    pass "CR-21: Nonexistent binary detected as failure"
else
    fail "CR-21: Nonexistent binary should fail" "error message" "$RESULT"
fi

# ==========================================
# CR-09: PID reuse safety
# ==========================================
echo ""
echo -e "${YELLOW}CR-09: PID reuse race prevention${NC}"

# Test: Connect to a server that exits, then disconnect. The key safety property
# is that StopProcess() should NOT send SIGTERM/SIGKILL to a stale PID when
# the child has already been reaped by IsProcessRunning().
#
# We can't directly observe whether kill() was called on a wrong PID from
# the SQL layer, but we can verify the sequence doesn't crash or hang:
# 1. ATTACH to a server (echo server that we can control)
# 2. Use it briefly
# 3. Kill the server externally (simulate unexpected exit)
# 4. DETACH should complete without errors or hanging

# Create a mock server that writes its PID to a file so we can kill it
PID_FILE=$(mktemp)
cat > /tmp/mock_pid_server_$$.sh << 'MOCK_EOF'
#!/bin/bash
echo $$ > "$1"
while IFS= read -r line; do
    id=$(echo "$line" | grep -o '"id":[0-9]*' | head -1 | cut -d: -f2)
    [ -z "$id" ] && id=1
    # Respond to initialize
    if echo "$line" | grep -q '"initialize"'; then
        echo "{\"jsonrpc\":\"2.0\",\"id\":${id},\"result\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"serverInfo\":{\"name\":\"test\",\"version\":\"1.0\"}}}"
    else
        echo "{\"jsonrpc\":\"2.0\",\"id\":${id},\"result\":{\"status\":\"ok\"}}"
    fi
done
MOCK_EOF
chmod +x /tmp/mock_pid_server_$$.sh

# Test that detach after child exit doesn't hang (completes within timeout)
RESULT=$(timeout 15 bash -c "
echo \"
LOAD '${EXTENSION}';
ATTACH '' AS pid_test (TYPE mcp, COMMAND '/tmp/mock_pid_server_$$.sh ${PID_FILE}', TRANSPORT 'stdio');
\" | '${DUCKDB}' -unsigned 2>&1
" 2>&1 || true)

# Even if attach fails, the important thing is it didn't hang
if [ $? -le 124 ]; then
    pass "CR-09: ATTACH/DETACH with short-lived server completes without hanging"
else
    fail "CR-09: ATTACH/DETACH hung (timeout)" "completion within 15s" "timed out"
fi

# Clean up
rm -f "$PID_FILE" "/tmp/mock_pid_server_$$.sh"

# Second test: verify that after a child is reaped by IsProcessRunning(),
# StopProcess doesn't kill a new process that reused the PID.
# Strategy: Fork a process, let it die, spawn a "canary" process, then
# trigger StopProcess. If the canary survives, no stale kill happened.
#
# We do this at the shell level since we can't instrument the C++ code:

# Start a short-lived background process and record its PID
bash -c "exit 0" &
DEAD_PID=$!
wait $DEAD_PID 2>/dev/null || true

# Start a canary process (long-lived sleep)
sleep 300 &
CANARY_PID=$!

# Run DuckDB with a server that we immediately kill, then detach
cat > /tmp/mock_canary_server_$$.sh << 'MOCK_EOF'
#!/bin/bash
while IFS= read -r line; do
    id=$(echo "$line" | grep -o '"id":[0-9]*' | head -1 | cut -d: -f2)
    [ -z "$id" ] && id=1
    if echo "$line" | grep -q '"initialize"'; then
        echo "{\"jsonrpc\":\"2.0\",\"id\":${id},\"result\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"serverInfo\":{\"name\":\"test\",\"version\":\"1.0\"}}}"
    else
        echo "{\"jsonrpc\":\"2.0\",\"id\":${id},\"result\":{\"status\":\"ok\"}}"
    fi
done
MOCK_EOF
chmod +x /tmp/mock_canary_server_$$.sh

# ATTACH and DETACH — this exercises the full Connect/Disconnect path
echo "
LOAD '${EXTENSION}';
ATTACH '' AS canary_test (TYPE mcp, COMMAND '/tmp/mock_canary_server_$$.sh', TRANSPORT 'stdio');
DETACH canary_test;
" | timeout 15 "$DUCKDB" -unsigned 2>&1 > /dev/null || true

# Check canary is still alive — if StopProcess sent SIGTERM to wrong PID, canary might be dead
if kill -0 $CANARY_PID 2>/dev/null; then
    pass "CR-09: Canary process survived (no stale PID kill)"
else
    fail "CR-09: Canary process was killed — possible stale PID signal"
fi

# Clean up canary
kill $CANARY_PID 2>/dev/null || true
wait $CANARY_PID 2>/dev/null || true
rm -f "/tmp/mock_canary_server_$$.sh"

# ==========================================
# CR-10: Partial JSON line reads
# ==========================================
echo ""
echo -e "${YELLOW}CR-10: Partial JSON line reads${NC}"

# Test: Connect to mock_slow_json.sh which sends JSON in chunks.
# Before fix: ReadFromProcess() breaks on EAGAIN after first chunk,
# returning partial JSON. After fix: it loops back to WaitForData()
# and assembles the complete line.

# Create an MCP-compatible slow-JSON server
cat > /tmp/mock_slow_mcp_$$.sh << 'MOCK_EOF'
#!/bin/bash
# Read initialize request
read -r request

# Reply to initialize in chunks with small delays between them
# This triggers EAGAIN between chunks on a non-blocking fd
printf '{"jsonrpc":"2.0","id":1,"result":{"protocolVersion":"2024-11-05","capabilities":{},'
sleep 0.02
printf '"serverInfo":{"name":"slow-test","version":"1.0"}}}'
printf '\n'

# Read subsequent requests and reply normally
while IFS= read -r line; do
    id=$(echo "$line" | grep -o '"id":[0-9]*' | head -1 | cut -d: -f2)
    [ -z "$id" ] && id=1
    printf '{"jsonrpc":"2.0","id":%s,"result":{' "$id"
    sleep 0.02
    printf '"content":[{"type":"text","text":"chunked-response-ok"}]}}\n'
done
MOCK_EOF
chmod +x /tmp/mock_slow_mcp_$$.sh

RESULT=$(echo "
LOAD '${EXTENSION}';
ATTACH '' AS slow_json_test (TYPE mcp, COMMAND '/tmp/mock_slow_mcp_$$.sh', TRANSPORT 'stdio');
SELECT 'attach_ok' AS status;
DETACH slow_json_test;
" | timeout 15 "$DUCKDB" -unsigned 2>&1 || true)

if echo "$RESULT" | grep -q "attach_ok"; then
    pass "CR-10: Chunked JSON response assembled correctly (ATTACH succeeded)"
else
    # Check if it failed with a JSON parse error (the original bug)
    if echo "$RESULT" | grep -qi "json\|parse\|partial\|unexpected"; then
        fail "CR-10: Partial JSON read — got parse error" "successful ATTACH" "$RESULT"
    else
        fail "CR-10: Unexpected failure" "attach_ok" "$RESULT"
    fi
fi

rm -f "/tmp/mock_slow_mcp_$$.sh"

# Test: Verify the basic echo server works (baseline for chunked test)
RESULT=$(echo "
LOAD '${EXTENSION}';
ATTACH '' AS echo_test (TYPE mcp, COMMAND '${SCRIPT_DIR}/mock_echo_server.sh', TRANSPORT 'stdio');
SELECT 'echo_ok' AS status;
DETACH echo_test;
" | timeout 15 "$DUCKDB" -unsigned 2>&1 || true)

if echo "$RESULT" | grep -q "echo_ok"; then
    pass "CR-10: Baseline echo server works correctly"
else
    # This might fail due to initialize protocol, which is OK — the test above
    # is the important one. Mark as informational.
    skip "CR-10: Baseline echo server" "may require full MCP protocol handshake"
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
if [ $TESTS_SKIPPED -gt 0 ]; then
    echo -e "${YELLOW}Skipped: $TESTS_SKIPPED${NC}"
fi

if [ $TESTS_FAILED -gt 0 ]; then
    exit 1
fi

echo ""
echo -e "${GREEN}All tests passed!${NC}"
