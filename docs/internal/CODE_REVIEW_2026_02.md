# Code Review — February 2026

Rigorous review of the DuckDB MCP extension codebase covering security,
concurrency, logic, and code quality. Findings are numbered CR-01 through CR-23
and grouped by severity.

---

## High Severity

### CR-01: Missing JSON escaping in ListTablesToolHandler output ✓ Fixed (PR #31, Issue #22)

**File:** `src/server/tool_handlers.cpp:840-858`

Database, schema, and table names are interpolated into JSON without
`EscapeJsonString()`:

```cpp
json += "\"database\":\"" + db_name.ToString() + "\",";
json += "\"schema\":\"" + schema_name.ToString() + "\",";
json += "\"name\":\"" + table_name.ToString() + "\",";
json += "\"type\":\"" + obj_type.ToString() + "\",";
```

A table named `tab"le` produces malformed JSON. `EscapeJsonString()` is defined
in the same file and used correctly in `DescribeToolHandler` — it must be
applied here too.

**Impact:** Malformed JSON output; potential injection if consumed by downstream
JSON parsers.

---

### CR-02: Missing JSON escaping in DatabaseInfoToolHandler output ✓ Fixed (PR #31, Issue #22)

**File:** `src/server/tool_handlers.cpp:950-1165`

All five sub-methods emit unescaped values into JSON:

- `GetDatabasesInfo()` — database name (line 960), path (line 966), type (line 969)
- `GetSchemasInfo()` — database (line 1005), name (line 1006)
- `GetTablesInfo()` — database (line 1044), schema (line 1045), name (line 1046)
- `GetViewsInfo()` — database (line 1091), schema (line 1092), name (line 1093)
- `GetExtensionsInfo()` — name (line 1131), version (line 1157)

`GetExtensionsInfo` at line 1142 partially escapes the description field
(handles `"` and `\\`) but misses control characters. `EscapeJsonString()`
handles all cases and should be used instead.

**Impact:** Same as CR-01.

---

### CR-03: Missing JSON escaping in ExportToolHandler::FormatData ✓ Fixed (PR #31, Issue #22)

**File:** `src/server/tool_handlers.cpp:522-538`

Column names and string values are directly interpolated into JSON:

```cpp
json += "\"" + result.names[col] + "\":";
json += "\"" + value.ToString() + "\"";
```

**Impact:** Same as CR-01.

---

### CR-04: Use-after-free in ResourceRegistry/ToolRegistry pointer access ✓ Fixed (PR #30, Issue #24)

**File:** `src/server/mcp_server.cpp:45-49` and `79-83`

```cpp
ResourceProvider *ResourceRegistry::GetResource(const string &uri) const {
    lock_guard<mutex> lock(registry_mutex);
    auto it = resources.find(uri);
    return it != resources.end() ? it->second.get() : nullptr;
}
```

The mutex is released when the function returns. The raw pointer is then used by
callers (`HandleResourcesRead`, `HandleToolsCall`, `HandleToolsList`). If
another thread calls `UnregisterResource()` between return and dereference, the
pointer is dangling. Same pattern exists in `ToolRegistry::GetTool()`.

**Impact:** Use-after-free / crash in concurrent access scenarios.

---

## Medium Severity

### CR-05: ExportToolHandler::FormatData CSV output lacks quoting ✓ Fixed (PR #29, Issue #23)

**File:** `src/server/tool_handlers.cpp:541-565`

Values containing commas, quotes, or newlines are not quoted or escaped:

```cpp
csv += value.ToString();
```

Headers (column names) are also unquoted.

**Impact:** Malformed CSV for any data containing `,`, `"`, or `\n`.

---

### CR-06: Error messages in HTTP handler lambda leak internals — Open (tracked in Issue #28)

**File:** `src/server/mcp_server.cpp:176` and `355`

```cpp
return R"({"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error: )"
       + string(e.what()) + R"("},"id":null})";
```

Two problems:
1. `e.what()` is not JSON-escaped — if it contains `"` or `\`, the response is
   malformed JSON.
2. Internal error details are leaked to clients. The HTTP transport layer at
   `http_server_transport.cpp:128` correctly returns a generic message, but
   these lambda error paths bypass that.

**Impact:** Information disclosure; malformed JSON responses.

---

### CR-07: COPY_STATEMENT not gated by ExecuteToolHandler ✓ Fixed (PR #32, Issue #26)

**File:** `src/server/tool_handlers.cpp:1318-1355`

`StatementType::COPY_STATEMENT` is not classified by any of the `Is*Statement()`
methods, so `IsAllowedStatement()` falls through to `return true`. COPY can
read/write arbitrary files. Given how carefully the export tool controls file
access, this appears to be an oversight.

**Impact:** When execute tool is enabled, COPY TO can write arbitrary files and
COPY FROM can read arbitrary files, bypassing intended restrictions.

---

### CR-08: MCPSecurityConfig is not thread-safe ✓ Fixed (Issue #25)

**File:** `src/include/duckdb_mcp_security.hpp:73-78`

`allowed_commands`, `allowed_urls`, `servers_locked`, `commands_locked`,
`server_file` are plain members with no synchronization. `SetAllowedCommands()`
writes while `IsCommandAllowed()` reads concurrently — a data race on the
vector.

**Impact:** Undefined behavior under concurrent access to security configuration.

---

### CR-09: PID reuse race in StdioTransport process management ✓ Fixed (PR #33, Issue #27)

**File:** `src/protocol/mcp_transport.cpp:362-375`

```cpp
bool StdioTransport::IsProcessRunning() const {
    int status;
    int result = waitpid(process_pid, &status, WNOHANG);
    return result == 0;
}
```

`IsProcessRunning()` is called via `IsConnected()` on every `Send()`/`Receive()`.
When the child exits, `waitpid()` reaps it and the PID becomes available for
reuse by the OS. Later, `StopProcess()` calls `kill(process_pid, SIGTERM)` —
potentially targeting a different process.

**Impact:** In edge cases, sends SIGTERM/SIGKILL to the wrong process.

---

### CR-10: ReadFromProcess can return partial JSON lines ✓ Fixed (PR #33, Issue #27)

**File:** `src/protocol/mcp_transport.cpp:407-428`

After `WaitForData()` succeeds, the non-blocking read loop breaks on
`EAGAIN`/`EWOULDBLOCK` even if no `\n` has been found:

```cpp
if (errno != EAGAIN && errno != EWOULDBLOCK) {
    throw IOException("Error reading from process stdout");
}
break;  // Returns partial data without \n
```

If data arrives in multiple chunks, the first chunk (without `\n`) is returned
as a partial line. This causes JSON parse failures on the caller side.

Should re-enter `WaitForData()` instead of breaking.

**Impact:** Intermittent parse failures in communication with MCP server
processes.

---

### CR-11: `last_error` string is not thread-safe ✓ Fixed (Issue #25)

**File:** `src/include/protocol/mcp_connection.hpp:99`

Written by `SetError()` and read by `GetLastError()` from different threads.
`std::string` is not safe for concurrent read/write.

**Impact:** Data race / potential crash.

---

### CR-12: `start_time` is not atomic ✓ Fixed (Issue #25)

**File:** `src/include/server/mcp_server.hpp:183`

Written in `Start()`/`StartForeground()`, read in `GetUptime()`/`GetStatus()`.
Plain `time_t` while all other counters on the same object are `atomic`.

**Impact:** Data race under concurrent access.

---

## Low Severity / Design

### CR-13: ParseCapabilities hardcodes wrong defaults ✓ Fixed (PR #44, Issue #36)

**File:** `src/protocol/mcp_connection.cpp:269-278`

```cpp
capabilities.supports_resources = true;
capabilities.supports_tools = false;  // Wrong — server supports tools
```

Ignores the actual server response. `supports_tools = false` despite tools being
a core feature. `ListTools()`/`CallTool()` work anyway because they don't check
capabilities, making the field misleading.

**Impact:** Incorrect capability reporting; future code checking capabilities
will break.

---

### CR-14: ListResources returns empty vector (stub) — Open (tracked in Issue #28)

**File:** `src/protocol/mcp_connection.cpp:104-126`

The response is received from the server but never parsed — the function always
returns an empty vector with a "Placeholder implementation" comment.

**Impact:** `mcp_list_resources()` returns empty results for client connections.

---

### CR-15: ListTools returns empty vector (stub) — Open (tracked in Issue #28)

**File:** `src/protocol/mcp_connection.cpp:169-184`

Same as CR-14 but for tools.

**Impact:** `mcp_list_tools()` returns empty results for client connections.

---

### CR-16: ExportToFile double-executes the query — Open (Issue #37)

**File:** `src/server/tool_handlers.cpp:447` and `481-500`

`Execute()` runs `conn.Query(query)` to check for errors, then `ExportToFile()`
runs the same query again via `COPY (query) TO ...`.

**Impact:** Unnecessary double execution. Wastes resources.

---

### CR-17: Global singletons share state across database instances — Open (Issue #38)

**Files:** `MCPSecurityConfig`, `MCPServerManager`, `MCPConfigManager`,
`MCPLogger`, `MCPTemplateManager`

DuckDB supports multiple `DatabaseInstance` objects per process. These singletons
mean:
- Security settings from one database affect all others
- Only one MCP server can run across all databases
- Tool/resource registrations can cross database boundaries

**Impact:** Incorrect behavior in multi-database / embedded scenarios.

---

### CR-18: Constant-time comparison still leaks token length — Open (Issue #39)

**File:** `src/server/http_server_transport.cpp:14-30`

When lengths differ, the dummy loop iterates `a.size()` times, not
`max(a.size(), b.size())`. Timing still correlates with expected token length.

**Impact:** Minor timing side-channel on auth token length.

---

### CR-19: Silently swallowed exceptions in ApplyPendingRegistrations — Open (tracked in Issue #28)

**File:** `src/server/mcp_server.cpp:888-896`

```cpp
} catch (const std::exception &e) {
    // Log error but continue with other registrations
}
```

Comment says "Log error" but nothing is logged. User gets no feedback if a
pending registration fails.

**Impact:** Silent failures in tool/resource registration.

---

### CR-20: Duplicated HTTP server setup code — Open (Issue #40)

**File:** `src/server/mcp_server.cpp:151-187` vs `322-368`

`HTTPServerConfig` construction and handler lambda are copy-pasted between
`Start()` and `RunHTTPLoop()`. Both share the exception leakage issue (CR-06).

**Impact:** Maintenance burden; bugs must be fixed in two places.

---

### CR-21: Fragile sleep after fork ✓ Fixed (PR #33, Issue #27)

**File:** `src/protocol/mcp_transport.cpp:312`

`usleep(100000)` after fork is a timing assumption. On slow/loaded systems it
may not suffice; on fast systems it wastes 100ms of startup latency.

**Impact:** Unreliable process startup detection.

---

### CR-22: Non-string JSON array items silently dropped — Open (Issue #41)

**File:** `src/duckdb_mcp_security.cpp:300-304`

When parsing ARGS arrays, non-string items are silently ignored:

```cpp
yyjson_arr_foreach(root, idx, max, arg) {
    if (yyjson_is_str(arg)) {
        params.args.push_back(yyjson_get_str(arg));
    }
}
```

`["--port", 8080]` loses `8080`. Same for ENV values at line 342.

**Impact:** Silent data loss in configuration parsing.

---

### CR-23: CORS defaults to wildcard — Open (Issue #42)

**File:** `src/include/server/mcp_server.hpp:54`

```cpp
string cors_origins = "*";
```

Defaults to allowing all origins. Inconsistent with the security-by-default
posture of the rest of the extension. Already noted in `SECURITY_AUDIT.md` as a
future item.

**Impact:** Overly permissive default for production deployments.

---

## Summary

| Severity | Total | Fixed | Open | Key Areas |
|----------|-------|-------|------|-----------|
| High     | 4     | 4     | 0    | JSON escaping (3), use-after-free in registries (1) |
| Medium   | 8     | 6     | 2    | ✓ CSV, COPY, PID race/partial reads, thread safety (3) — ○ error leaks (CR-06) |
| Low      | 11    | 2     | 9    | ✓ Fragile sleep, ParseCapabilities — ○ stubs, singletons, duplicated code, silent errors, timing leaks, defaults |

### Status as of 2026-03-02

**Fixed (12 of 23):** CR-01/CR-02/CR-03 (PR #31, Issue #22), CR-04 (PR #30, Issue #24),
CR-05 (PR #29, Issue #23), CR-07 (PR #32, Issue #26), CR-08/CR-11/CR-12 (PR #35, Issue #25),
CR-09/CR-10/CR-21 (PR #33, Issue #27), CR-13 (PR #44, Issue #36)

**Tracked in open issues (4):** CR-06/CR-14/CR-15/CR-19 (Issue #28)

**Separate open issues (6):** CR-16 (Issue #37), CR-17 (Issue #38),
CR-18 (Issue #39), CR-20 (Issue #40), CR-22 (Issue #41), CR-23 (Issue #42)
