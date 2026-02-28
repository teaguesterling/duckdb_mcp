# Task: Fix Thread Safety Issues

**GitHub Issue:** #25
**Findings:** CR-08, CR-11, CR-12
**Severity:** Medium
**Files:** `src/include/duckdb_mcp_security.hpp`, `src/duckdb_mcp_security.cpp`,
`src/include/protocol/mcp_connection.hpp`, `src/protocol/mcp_connection.cpp`,
`src/include/server/mcp_server.hpp`

## Problem

Three components have data races where members are accessed from multiple
threads without synchronization.

## Fix 1: MCPSecurityConfig (CR-08)

### File: `src/include/duckdb_mcp_security.hpp`

Add a mutex to protect all member access:

```cpp
private:
    mutable mutex config_mutex;  // ADD THIS
    vector<string> allowed_commands;
    vector<string> allowed_urls;
    string server_file;
    bool servers_locked;
    bool commands_locked;
    bool serving_disabled;
```

### File: `src/duckdb_mcp_security.cpp`

Add `lock_guard<mutex> lock(config_mutex);` at the start of every public method:

- `SetAllowedCommands()` — lock before checking `servers_locked`
- `SetAllowedUrls()` — lock before checking `servers_locked`
- `SetServerFile()` — lock before checking `servers_locked`
- `LockServers()` — lock before write
- `SetServingDisabled()` — lock before write
- `IsCommandAllowed()` — lock before reading `allowed_commands`
- `IsUrlAllowed()` — lock before reading `allowed_urls`
- `IsPermissiveMode()` — lock before reading members
- `ValidateAttachSecurity()` — lock before reading `servers_locked` and
  `allowed_commands`

Also add locks to the `const` accessor methods in the header (already declared
`const`, so the mutex must be `mutable`):

- `AreServersLocked()`
- `AreCommandsLocked()`
- `IsServingDisabled()`
- `GetServerFile()`

## Fix 2: MCPConnection::last_error (CR-11)

### File: `src/include/protocol/mcp_connection.hpp`

`last_error` is already alongside `connection_mutex`. The fix is to ensure all
access goes through the existing mutex.

### File: `src/protocol/mcp_connection.cpp`

- `SetError()` (line 280): Acquire `connection_mutex` before writing `last_error`
- `GetLastError()` (header inline): Change to a method that acquires
  `connection_mutex` before reading

Note: Check that `SetError` is not called while `connection_mutex` is already
held (e.g., from within `Connect()` which holds the lock). If so, either use a
recursive mutex or restructure to avoid nested locking.

Looking at the code:
- `Connect()` holds `connection_mutex` and calls `SetError()` — so `SetError`
  must NOT acquire `connection_mutex`
- `SendRequestWithRetry()` does NOT hold `connection_mutex` and calls
  `SetError()`
- `HandleTransportError()` does NOT hold `connection_mutex` and calls
  `SetError()`

**Best approach:** Use a separate `mutable mutex error_mutex` for `last_error`
and `is_recoverable_error` only:

```cpp
mutable mutex error_mutex;
string last_error;
```

Then `SetError()` and `GetLastError()` lock `error_mutex`.

## Fix 3: MCPServer::start_time (CR-12)

### File: `src/include/server/mcp_server.hpp`

Change:
```cpp
time_t start_time;
```
to:
```cpp
atomic<time_t> start_time;
```

No other changes needed — it's already read/written with simple loads/stores.

## Verification

1. `make test` passes
2. `make integration_test` passes
3. No deadlocks in the HTTP transport tests (which exercise concurrent access)

## Scope

- `src/include/duckdb_mcp_security.hpp` — add mutex, make accessors use it
- `src/duckdb_mcp_security.cpp` — add lock_guard to every public method
- `src/include/protocol/mcp_connection.hpp` — add error_mutex, fix GetLastError
- `src/protocol/mcp_connection.cpp` — lock error_mutex in SetError/ClearError
- `src/include/server/mcp_server.hpp` — change start_time to atomic
