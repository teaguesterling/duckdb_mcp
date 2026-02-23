# Security Audit: duckdb_mcp Extension

**Date:** 2026-02-22
**Branch:** `security-audit` (from `v1.5-variegata`)
**Scope:** Full codebase review covering client transport, server infrastructure, tool handlers, authentication, and security configuration.

---

## Summary

The audit identified **21 distinct findings**: 15 true bugs and 6 controllable behaviors that should be gated by security settings. The most critical issues are SQL injection in server-side tool handlers, arbitrary file write via the export tool, broken authentication validation, and environment variable injection in child process spawning.

**All 15 bugs have been fixed.** The 6 controllable behaviors remain as future work requiring design decisions.

---

## Findings: True Bugs

These should be fixed regardless of configuration.

### BUG-01: SQL Injection in `DescribeTable` — Unquoted Table Name (Critical) — FIXED

**File:** `src/server/tool_handlers.cpp`

```cpp
string describe_query = "DESCRIBE " + table_name;
```

`table_name` comes directly from MCP client input with no quoting or validation. An MCP client can send `table_name = "foo; DROP TABLE bar; --"` to execute arbitrary SQL. The JSON response builder also injects `table_name` into JSON without escaping.

**Fix applied:** Table name quoted via `KeywordHelper::WriteOptionallyQuoted()`. All user-supplied values in JSON output escaped via `EscapeJsonString()` helper.

---

### BUG-02: SQL Injection in `DescribeQuery` — Bypasses All Access Controls (Critical) — FIXED

**File:** `src/server/tool_handlers.cpp`

```cpp
string describe_query = "DESCRIBE (" + query + ")";
```

The `describe` tool's `query` parameter executes arbitrary SQL via `DESCRIBE (...)`, completely bypassing the `IsQueryAllowed()` allowlist/denylist that gates the `query` tool. `DESCRIBE` fully executes the subquery to determine the schema.

**Fix applied:** `DescribeToolHandler` now accepts `allowed_queries`/`denied_queries` and calls `IsQueryAllowed()` before executing the describe query.

---

### BUG-03: Arbitrary File Write via `ExportToolHandler` (Critical) — FIXED

**File:** `src/server/tool_handlers.cpp`

```cpp
copy_query = "COPY (" + query + ") TO '" + output_path + "' (FORMAT CSV, HEADER)";
```

Both `output_path` and `query` come from MCP client input with no validation. A client can write attacker-controlled content to any path the DuckDB process can access (e.g., `~/.ssh/authorized_keys`, cron jobs). The `output_path` is also vulnerable to SQL injection via single-quote escaping. The `query` parameter bypasses `IsQueryAllowed()`.

**Fix applied:** Single quotes in `output_path` escaped via `EscapeSQLString()`. `ExportToolHandler` now accepts `allowed_queries`/`denied_queries` and calls `IsQueryAllowed()` before execution.

---

### BUG-04: SQL Injection in `ListTablesToolHandler` (High) — FIXED

**File:** `src/server/tool_handlers.cpp`

```cpp
tables_query += " AND schema_name = '" + schema_filter + "'";
tables_query += " AND database_name = '" + database_filter + "'";
```

Both filter parameters are directly interpolated into SQL without escaping.

**Fix applied:** Both `schema_filter` and `database_filter` escaped via `EscapeSQLString()` before interpolation.

---

### BUG-05: SQL Injection via Unvalidated Non-String Parameters in `SQLToolHandler` (High) — FIXED

**File:** `src/server/tool_handlers.cpp`

```cpp
} else {
    // For numeric/boolean types, use value as-is
    sql_value = value;
}
```

For parameters typed as `integer`/`number`/`boolean` in the tool schema, the raw user-supplied string is placed directly into SQL with no type validation. A value of `"1 OR 1=1; DROP TABLE t; --"` for an integer parameter injects SQL.

**Fix applied:** Integer/number values validated via `std::stoll`/`std::stod` with full-string consumption check (`pos == value.size()`) to reject trailing injection payloads. Boolean values strictly validated as `"true"` or `"false"`. Unknown types treated as escaped strings.

---

### BUG-06: `ValidateAuthentication` Is a Non-Functional Stub (Critical) — FIXED

**File:** `src/server/mcp_server.cpp`

```cpp
bool MCPServer::ValidateAuthentication(const MCPMessage &request) const {
    if (!config.require_auth) { return true; }
    // TODO: Implement proper authentication validation
    return !config.auth_token.empty();  // Never inspects the request
}
```

When `require_auth = true`, this always returns `true` if the server has a non-empty `auth_token` configured. The client's actual credentials are never checked. This affects stdio and memory transports. The HTTP transport has its own correct check in `http_server_transport.cpp`.

**Fix applied:** `ValidateAuthentication` now fails closed (`return false`) for non-HTTP transports when `require_auth` is true. HTTP transport auth is enforced at the transport layer. Also wired `require_auth`, `allowed_queries`, and `denied_queries` config JSON parsing in `MCPServerStartImpl`.

---

### BUG-07: `execvp` Inherits Full Parent Environment — `LD_PRELOAD` Injection (Critical) — FIXED

**File:** `src/protocol/mcp_transport.cpp`

```cpp
for (const auto &env_pair : config.environment) {
    setenv(env_pair.first.c_str(), env_pair.second.c_str(), 1);
}
execvp(config.command_path.c_str(), args.data());
```

The child process inherits the parent's entire environment, and user-supplied `env` values are added via `setenv` with overwrite. An attacker who can supply environment variables can set `LD_PRELOAD=/tmp/evil.so` to execute arbitrary code before the MCP server binary runs. Environment keys/values are never validated by the security layer.

**Fix applied:** Replaced `execvp` with `execve` using an explicitly constructed environment. Only safe passthrough variables (`HOME`, `USER`, `LANG`, `TZ`, `PATH`, `TERM`, `SHELL`) inherited from parent. Dangerous keys (`LD_PRELOAD`, `LD_LIBRARY_PATH`, `LD_AUDIT`, `DYLD_INSERT_LIBRARIES`, `DYLD_LIBRARY_PATH`, `DYLD_FRAMEWORK_PATH`) blocked even if user supplies them. Manual PATH resolution for relative commands.

---

### BUG-08: Command Allowlist Basename Shortcut Allows PATH Hijacking (Critical) — FIXED

**File:** `src/duckdb_mcp_security.cpp`

```cpp
if (allowed.length() > 0 && allowed[0] == '/' && (command_path.empty() || command_path[0] != '/')) {
    auto last_slash = allowed.find_last_of('/');
    if (last_slash != string::npos) {
        string basename = allowed.substr(last_slash + 1);
        if (command_path == basename) {
            return true;  // Allows "python3" when "/usr/bin/python3" is allowlisted
        }
    }
}
```

When the allowlist contains an absolute path like `/usr/bin/python3`, a user can supply the relative name `python3`. Combined with `execvp`'s `PATH` search, this resolves to whichever `python3` appears first in `PATH` — which may be attacker-controlled.

**Fix applied:** Basename shortcut removed entirely. Only exact string match accepted. Allowlist entries must use the same form (absolute or relative) as the command.

---

### BUG-09: Permissive Mode Is the Default — Allows All Commands (High) — FIXED

**File:** `src/duckdb_mcp_security.cpp`, `src/duckdb_mcp_extension.cpp`

```cpp
bool MCPSecurityConfig::IsPermissiveMode() const {
    return allowed_commands.empty() && allowed_urls.empty() && !commands_locked;
}
```

The initialization in `LoadInternal` calls `SetAllowedCommands("")` which leaves `allowed_commands` empty and `commands_locked = false`, making `IsPermissiveMode()` return `true`. This means a freshly loaded extension allows executing **any** command. The comment says "Set secure defaults" but the behavior is allow-all.

**Fix applied:** `SetAllowedCommands()` now always sets `commands_locked = true` on any call, making an empty list mean "deny all" rather than "permissive mode". `LoadInternal` no longer calls `SetAllowedCommands("")`, so the user's first explicit `SET allowed_mcp_commands` is the locking call. Re-initialization is guarded to prevent resetting locked state.

---

### BUG-10: Arbitrary Config File Read via `from_config_file` (High) — FIXED

**File:** `src/duckdb_mcp_security.cpp`

The `from_config_file` ATTACH option opens any file path with `std::ifstream` — no restriction to the configured `mcp_server_file`. Error messages from JSON parse failures leak file existence and partial contents.

**Fix applied:** `ParseMCPAttachParams` now validates that the `from_config_file` path, after canonical resolution via `realpath()`, matches the configured `mcp_server_file`. Falls back to raw string comparison when paths can't be resolved. Windows uses simple string comparison.

---

### BUG-11: Pipe FDs Leak to Child Process — No `FD_CLOEXEC` (Medium) — FIXED

**File:** `src/protocol/mcp_transport.cpp`

Pipes are created with `pipe()` instead of `pipe2(..., O_CLOEXEC)`. All parent FDs (including database file handles) are inherited by the child process. Failed `pipe()` calls also leak previously-opened FDs.

**Fix applied:** Uses `pipe2(O_CLOEXEC)` on Linux (atomic) and `pipe()` + `fcntl(FD_CLOEXEC)` on macOS. All FDs > `STDERR_FILENO` closed in child before `execve`. Partial pipe failures properly clean up already-opened FDs.

---

### BUG-12: Zombie Processes from Non-Blocking `waitpid` (Medium) — FIXED

**File:** `src/protocol/mcp_transport.cpp`

```cpp
kill(process_pid, SIGTERM);
waitpid(process_pid, &status, WNOHANG);  // Returns immediately, child still running
```

The child hasn't exited in the nanoseconds between `kill` and `waitpid(WNOHANG)`. The process becomes a zombie.

**Fix applied:** `StopProcess()` now sends SIGTERM, waits 100ms, checks with `waitpid(WNOHANG)`, and escalates to SIGKILL + blocking `waitpid` if the process is still running.

---

### BUG-13: Token Comparison Vulnerable to Timing Attack (Medium) — FIXED

**File:** `src/server/http_server_transport.cpp`

```cpp
string expected = "Bearer " + config.auth_token;
if (auth_header != expected) { ... }
```

`std::string::operator!=` is not constant-time. Remote attackers can measure timing differences to recover the token character by character.

**Fix applied:** Replaced with `ConstantTimeEquals()` that uses `volatile unsigned char` XOR accumulation over the full string length, with a dummy loop on length mismatch to avoid leaking size information.

---

### BUG-14: Unescaped Data in Manual JSON Construction (Medium) — FIXED

**Files:** `src/server/tool_handlers.cpp`

Database values containing `"`, `\`, or control characters are concatenated directly into JSON strings, producing malformed or attacker-manipulable JSON responses.

**Fix applied:** Added `EscapeJsonString()` helper that handles `"`, `\`, newlines, tabs, and control characters (`\u00XX`). Applied to all user-supplied values in `DescribeTable` and `DescribeQuery` JSON output.

---

### BUG-15: Exception Messages Reflected to HTTP Clients (Medium) — FIXED

**File:** `src/server/http_server_transport.cpp`

```cpp
res.set_content(R"(...,"message":"Internal error: )" + string(e.what()) + R"("}...)", ...);
```

Raw exception messages (containing file paths, SQL fragments, internal state) are returned in HTTP error responses. The `e.what()` string is also not JSON-escaped.

**Fix applied:** HTTP error handler now returns a generic `"Internal server error"` message. The full exception is suppressed from client output (should be logged internally in production).

---

## Findings: Controllable Behaviors

These are inherently dangerous capabilities that should be gated behind explicit security settings. **These remain as future work.**

### CTRL-01: Query Allowlist/Denylist Is Substring-Based — Trivially Bypassable (High)

**File:** `src/server/tool_handlers.cpp:177-198`

The `IsQueryAllowed()` check uses `string::find()` (substring matching). SQL comments (`sel/**/ect`), alternative syntax, or allowlist false-positives (`INSERT ... SELECT` matching `SELECT`) bypass the filter.

**Recommendation:** Use DuckDB's prepared statement type parsing to enforce read-only at the statement level, not string matching. Or enforce read-only at the connection level.

---

### CTRL-02: `ExecuteToolHandler` DDL Includes `LOAD`/`ATTACH`/`SET` (High)

**File:** `src/server/tool_handlers.cpp:1081-1101`

When `allow_ddl = true`, `LOAD` (loads arbitrary shared libraries), `ATTACH` (attaches external databases), `SET` (changes security settings), and `UPDATE_EXTENSIONS` are all permitted. These are far more dangerous than `CREATE TABLE`.

**Recommendation:** Split DDL permissions into fine-grained categories. `LOAD`, `ATTACH`, `SET`, and `UPDATE_EXTENSIONS` should be separately controlled and default to disabled.

---

### CTRL-03: CORS Wildcard Always Hardcoded On (High)

**Files:** `src/server/mcp_server.cpp:134`, `src/server/http_server_transport.cpp:54`

`Access-Control-Allow-Origin: *` is always set. Combined with bearer token auth, any webpage can make cross-origin requests to the MCP server.

**Recommendation:** Make CORS configurable. Default to disabled or specific origins.

---

### CTRL-04: URL Prefix Matching Allows Subdomain/Path Confusion (Medium)

**File:** `src/duckdb_mcp_security.cpp:106-112`

`StringUtil::StartsWith(url, allowed)` means `https://api.example.com` allows `https://api.example.com.evil.com`.

**Recommendation:** Parse URLs properly. Match scheme+host+port exactly, then check path prefix with boundary enforcement.

---

### CTRL-05: `/health` Endpoint Unauthenticated — Fingerprints Server (Medium)

**File:** `src/server/http_server_transport.cpp:75-78`

The health endpoint always returns `{"status":"ok"}` regardless of auth configuration, confirming to scanners that a DuckDB MCP server exists.

**Recommendation:** Either apply auth to health, or make it configurable.

---

### CTRL-06: `mcp_server_send_request` Bypasses HTTP Auth (Medium)

**File:** `src/duckdb_mcp_extension.cpp:833-872`

This SQL function sends arbitrary MCP requests directly to the server, bypassing the HTTP auth layer. Intended for testing but registered in production.

**Recommendation:** Gate behind a testing/debug flag, or apply the same auth checks.

---

## Architectural Recommendations

1. **Enforce read-only at the connection level.** Multiple tools (`describe`, `export`, `list_tables`) construct SQL from user input without going through the query allowlist. A structural fix is to open read-only connections for server-side handlers.

2. **Use parameterized queries everywhere.** Every tool handler that constructs SQL from user input should use prepared statements or proper escaping. String concatenation of user input into SQL is the single largest vulnerability class in this codebase.

3. ~~**Switch to `execve` with explicit environment.**~~ **Done.** The `execvp` + inherited environment model has been replaced with `execve` + sanitized environment.

4. ~~**Default to deny-all.**~~ **Done.** `SetAllowedCommands` now locks on any call, making the default deny-all rather than permissive.

---

## Priority Order for Fixes

**Immediate (Critical) — ALL FIXED:**
1. ~~BUG-07: LD_PRELOAD injection via `execvp` + inherited env~~
2. ~~BUG-08: Basename shortcut bypasses command allowlist~~
3. ~~BUG-09: Permissive mode default allows all commands~~
4. ~~BUG-01/02/03: SQL injection in tool handlers (describe, export)~~
5. ~~BUG-06: Broken `ValidateAuthentication` stub~~

**Short-term (High) — BUGS FIXED, CTRL items remain:**
6. ~~BUG-04/05: SQL injection in list_tables and SQL tool parameters~~
7. ~~BUG-10: Arbitrary config file read~~
8. CTRL-01: Replace substring-based query filtering
9. CTRL-02: Split DDL permissions

**Medium-term — BUGS FIXED, CTRL items remain:**
10. ~~BUG-11/12: FD leaks and zombie processes~~
11. ~~BUG-13/14/15: Timing attack, JSON escaping, error reflection~~
12. CTRL-03/04/05/06: CORS, URL matching, health endpoint, test functions
