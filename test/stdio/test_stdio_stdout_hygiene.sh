#!/bin/bash
# Regression test for issue #74 — stdout hygiene of the stdio transport.
#
# JSON-RPC over stdio requires fd 1 to carry framed protocol messages and NOTHING
# else. A DuckDB process has several writers that reach fd 1 without going through
# the transport:
#
#   1. The DuckDB CLI shell installs its own log storage as the global one and
#      prints every DUCKDB_LOG_WARNING to STDOUT wrapped in ANSI colour codes. A
#      warning raised while a tools/call is executing lands *inside* the protocol
#      stream and its trailing \033[00m reset prefixes the next response line, so
#      the response no longer starts with '{' and a strict client drops it.
#   2. `SET logging_storage='stdout'` installs DuckDB's own StdOutLogStorage,
#      which writes log rows straight to STREAM_STDOUT.
#   3. duckdb_mcp's own console logger (`SET mcp_console_logging=true`).
#
# The failure is silent: a *successful* response vanishes rather than an error
# being reported. So this test does not check that the happy path works — it
# deliberately provokes each stray writer and then asserts that stdout still
# contains nothing but well-formed JSON-RPC.
#
# IMPORTANT: each provocation is paired with a liveness check that the stray
# writer actually fired. Without that, the test would pass vacuously the day
# DuckDB stops emitting the warning we use as a trigger.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
# Both are overridable so the same script can be pointed at another build tree
# (useful when confirming that this test does fail against a build without the fix).
DUCKDB="${DUCKDB:-${PROJECT_DIR}/build/release/duckdb}"
EXTENSION="${EXTENSION:-${PROJECT_DIR}/build/release/extension/duckdb_mcp/duckdb_mcp.duckdb_extension}"

# Fall back to debug build if release isn't available
if [ ! -f "$DUCKDB" ]; then
    DUCKDB="${PROJECT_DIR}/build/debug/duckdb"
    EXTENSION="${PROJECT_DIR}/build/debug/extension/duckdb_mcp/duckdb_mcp.duckdb_extension"
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    if [ -n "${2:-}" ]; then
        echo "  Expected: $2"
    fi
    if [ -n "${3:-}" ]; then
        echo "  Got: $3"
    fi
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

skip() {
    echo -e "${YELLOW}○ SKIP${NC}: $1 — ${2:-}"
    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
}

echo "=========================================="
echo "stdio stdout hygiene (Issue #74)"
echo "=========================================="
echo ""

if [ ! -f "$DUCKDB" ]; then
    echo -e "${RED}Error: DuckDB binary not found${NC}"
    echo "  Tried: ${PROJECT_DIR}/build/release/duckdb"
    echo "  Tried: ${PROJECT_DIR}/build/debug/duckdb"
    exit 1
fi
if [ ! -f "$EXTENSION" ]; then
    echo -e "${RED}Error: extension not found at ${EXTENSION}${NC}"
    exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo -e "${RED}Error: python3 is required to validate the JSON-RPC stream${NC}"
    exit 1
fi

# The DuckDB CLI renders a "Success" header block for every statement in the init
# script, on stdout, before the server ever starts. That is the host's output, not the
# transport's, and a stdio launcher has to silence it — which is what these two dot
# commands do. Silencing it here is what lets the assertions below be absolute: once the
# setup phase is quiet, EVERY byte on stdout must be protocol traffic.
QUIET_SHELL='.mode list
.headers off'

TMPDIR_TEST="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_TEST"' EXIT

# Two requests: initialize, then a tools/call that runs the provoking SQL.
REQUESTS_FILE="${TMPDIR_TEST}/requests.ldjson"
cat > "$REQUESTS_FILE" <<'EOF'
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"hygiene","version":"1"}}}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"provoke","arguments":{}}}
EOF

#! Validates that every non-empty line on stdout is a JSON-RPC object, and that the
#! responses to both request ids arrived. Prints "OK" or a diagnostic.
VALIDATOR="${TMPDIR_TEST}/validate.py"
cat > "$VALIDATOR" <<'PYEOF'
import json, sys

path = sys.argv[1]
expected_ids = [int(a) for a in sys.argv[2:]] or [1, 2]
with open(path, "rb") as f:
    raw = f.read()

problems = []
seen_ids = set()
for lineno, line in enumerate(raw.split(b"\n"), start=1):
    if not line.strip():
        continue
    try:
        text = line.decode("utf-8")
    except UnicodeDecodeError:
        problems.append("line %d is not valid UTF-8: %r" % (lineno, line[:60]))
        continue
    try:
        msg = json.loads(text)
    except ValueError:
        problems.append("line %d is not JSON: %r" % (lineno, text[:80]))
        continue
    if not isinstance(msg, dict) or msg.get("jsonrpc") != "2.0":
        problems.append("line %d is not a JSON-RPC 2.0 message: %r" % (lineno, text[:80]))
        continue
    if "id" in msg:
        seen_ids.add(msg["id"])

for want in expected_ids:
    if want not in seen_ids:
        problems.append("no response with id=%r on stdout" % (want,))

if problems:
    print("STRAY: " + " | ".join(problems))
    sys.exit(1)
print("OK")
PYEOF

#! run_case <name> <init-sql-file>
#! Runs the stdio server with the given init script, feeding the request file on
#! stdin. Captures stdout and stderr into separate files.
run_case() {
    local init_sql="$1"
    local out="$2"
    local err="$3"
    local requests="${4:-$REQUESTS_FILE}"
    timeout 30 "$DUCKDB" -unsigned -init "$init_sql" \
        < "$requests" > "$out" 2> "$err"
    return 0
}

# ---------------------------------------------------------------------------
# Case 1: a DuckDB warning raised by the tool's own SQL (the reported repro).
#
# The deprecated `->` lambda makes the binder emit DUCKDB_LOG_WARNING while the
# tools/call is executing. The CLI shell's log storage prints it to STDOUT.
# ---------------------------------------------------------------------------
INIT1="${TMPDIR_TEST}/init_warning.sql"
cat > "$INIT1" <<EOF
$QUIET_SHELL
LOAD '${EXTENSION}';
PRAGMA mcp_publish_tool(
  'provoke',
  'Runs SQL that makes DuckDB log a warning.',
  'SELECT unnest(list_transform([''a'',''b''], x -> x || ''!'')) AS v',
  '{}', '[]', 'markdown');
PRAGMA mcp_server_start('stdio');
EOF

OUT1="${TMPDIR_TEST}/case1.out"
ERR1="${TMPDIR_TEST}/case1.err"
run_case "$INIT1" "$OUT1" "$ERR1"

# Liveness: the provocation must actually have produced a warning somewhere,
# otherwise the hygiene assertion below proves nothing.
if grep -q "Deprecated lambda arrow" "$OUT1" "$ERR1" 2>/dev/null; then
    pass "Case 1 liveness: the deprecated-lambda warning was emitted"

    RESULT=$(python3 "$VALIDATOR" "$OUT1" 2>&1)
    if [ "$RESULT" = "OK" ]; then
        pass "Case 1: DuckDB warning during tools/call does not reach stdout"
    else
        fail "Case 1: DuckDB warning during tools/call leaked onto stdout" \
             "only JSON-RPC on stdout" "$RESULT"
    fi

    # Nothing may be silently dropped either: the warning must still be visible.
    if grep -q "Deprecated lambda arrow" "$ERR1"; then
        pass "Case 1: the warning is preserved on stderr"
    else
        fail "Case 1: the warning was lost" "warning text on stderr" \
             "$(head -c 200 "$ERR1")"
    fi
else
    skip "Case 1" "this DuckDB build does not warn about the deprecated '->' lambda"
    skip "Case 1 hygiene" "trigger unavailable"
    skip "Case 1 preservation" "trigger unavailable"
fi

# ---------------------------------------------------------------------------
# Case 2: DuckDB's own stdout log storage, switched on from inside the session.
#
# Independent of the CLI shell: `SET logging_storage='stdout'` installs DuckDB's
# StdOutLogStorage, which writes log rows via Printer::RawPrint(STREAM_STDOUT).
# This trigger does not depend on any particular deprecation surviving.
#
# Logging is enabled through the `execute` tool rather than in the init script, so
# that every log row is produced strictly WHILE the transport owns stdout. Anything
# the init script logs happens before the server starts and is the launcher's
# problem, not the transport's (see the stdio note in docs/guides/server-usage.md).
# ---------------------------------------------------------------------------
INIT2="${TMPDIR_TEST}/init_logstorage.sql"
cat > "$INIT2" <<EOF
$QUIET_SHELL
LOAD '${EXTENSION}';
PRAGMA mcp_server_start('stdio', '{"enable_execute_tool": true, "execute_allow_set": true}');
EOF

REQUESTS2="${TMPDIR_TEST}/requests_logstorage.ldjson"
cat > "$REQUESTS2" <<'EOF'
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"hygiene","version":"1"}}}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"execute","arguments":{"statement":"SET GLOBAL logging_level = 'info'"}}}
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"execute","arguments":{"statement":"SET GLOBAL logging_storage = 'stdout'"}}}
{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"query","arguments":{"sql":"SELECT 42 AS logged_value"}}}
EOF

OUT2="${TMPDIR_TEST}/case2.out"
ERR2="${TMPDIR_TEST}/case2.err"
run_case "$INIT2" "$OUT2" "$ERR2" "$REQUESTS2"

# Liveness: DuckDB must actually have logged the query we ran, otherwise nothing
# was provoked and the assertion below is vacuous.
if grep -q "QueryLog" "$OUT2" "$ERR2" 2>/dev/null; then
    pass "Case 2 liveness: DuckDB's stdout log storage produced entries in-session"
else
    fail "Case 2 liveness: DuckDB logged nothing" "QueryLog entries" "(none)"
fi

RESULT=$(python3 "$VALIDATOR" "$OUT2" 1 2 3 4 2>&1)
if [ "$RESULT" = "OK" ]; then
    pass "Case 2: DuckDB 'stdout' log storage does not reach the protocol stream"
else
    fail "Case 2: DuckDB 'stdout' log storage leaked onto stdout" \
         "only JSON-RPC on stdout" "$RESULT"
fi

# ---------------------------------------------------------------------------
# Case 3: duckdb_mcp's own console logger.
# ---------------------------------------------------------------------------
INIT3="${TMPDIR_TEST}/init_mcplog.sql"
cat > "$INIT3" <<EOF
$QUIET_SHELL
LOAD '${EXTENSION}';
SET mcp_log_level = 'debug';
SET mcp_console_logging = true;
PRAGMA mcp_publish_tool(
  'provoke',
  'Runs a query while MCP console logging is on.',
  'SELECT 42 AS v',
  '{}', '[]', 'markdown');
PRAGMA mcp_server_start('stdio');
EOF

OUT3="${TMPDIR_TEST}/case3.out"
ERR3="${TMPDIR_TEST}/case3.err"
run_case "$INIT3" "$OUT3" "$ERR3"

# Liveness: the transport logs at DEBUG on every send/receive, so console logging
# must have produced lines somewhere. If it did not, the assertion is vacuous.
if grep -q "\[stdio\]" "$OUT3" "$ERR3" 2>/dev/null; then
    pass "Case 3 liveness: MCP console logging produced output"

    RESULT=$(python3 "$VALIDATOR" "$OUT3" 2>&1)
    if [ "$RESULT" = "OK" ]; then
        pass "Case 3: MCP console logging does not reach the protocol stream"
    else
        fail "Case 3: MCP console logging leaked onto stdout" \
             "only JSON-RPC on stdout" "$RESULT"
    fi
else
    fail "Case 3 liveness: MCP console logging produced nothing" \
         "[stdio] diagnostics on stdout or stderr" "(none)"
fi

# ---------------------------------------------------------------------------
# Case 4: stdout must be restored once the session ends, so a host that keeps
# using the connection afterwards is not left writing into the void.
# ---------------------------------------------------------------------------
INIT4="${TMPDIR_TEST}/init_restore.sql"
cat > "$INIT4" <<EOF
$QUIET_SHELL
LOAD '${EXTENSION}';
PRAGMA mcp_publish_tool('provoke', 'noop', 'SELECT 42 AS v', '{}', '[]', 'markdown');
PRAGMA mcp_server_start('stdio');
SELECT 'stdout-restored-marker' AS marker;
EOF

OUT4="${TMPDIR_TEST}/case4.out"
ERR4="${TMPDIR_TEST}/case4.err"
run_case "$INIT4" "$OUT4" "$ERR4"

if grep -q "stdout-restored-marker" "$OUT4"; then
    pass "Case 4: stdout is restored to the host after the session ends"
else
    fail "Case 4: stdout was not restored after the session" \
         "marker on stdout" "$(head -c 200 "$OUT4")"
fi

echo ""
echo "=========================================="
echo -e "Passed:  ${GREEN}${TESTS_PASSED}${NC}"
echo -e "Failed:  ${RED}${TESTS_FAILED}${NC}"
echo -e "Skipped: ${YELLOW}${TESTS_SKIPPED}${NC}"
echo "=========================================="

if [ "$TESTS_FAILED" -gt 0 ]; then
    exit 1
fi
exit 0
