# Task: Fix StdioTransport PID Race, Partial Reads, and Startup

**GitHub Issue:** #27
**Findings:** CR-09, CR-10, CR-21
**Severity:** Medium
**File:** `src/protocol/mcp_transport.cpp`

## Problem 1: PID Reuse Race (CR-09)

### Current Code (lines 362-375)

```cpp
bool StdioTransport::IsProcessRunning() const {
    if (process_pid <= 0) return false;
    int status;
    int result = waitpid(process_pid, &status, WNOHANG);
    return result == 0;
}
```

`IsProcessRunning()` is called via `IsConnected()` on every `Send()`/`Receive()`.
When the child exits, `waitpid()` reaps it (returns pid > 0). The PID is now
available for OS reuse. Later `StopProcess()` calls `kill(process_pid, SIGTERM)`
which may target a different process.

### Fix

Track whether the child has been reaped:

```cpp
// In header (mcp_transport.hpp), add to StdioTransport:
bool process_reaped = false;

// In IsProcessRunning:
bool StdioTransport::IsProcessRunning() const {
    if (process_pid <= 0 || process_reaped) return false;
    int status;
    int result = waitpid(process_pid, &status, WNOHANG);
    if (result > 0) {
        // Child exited — mark as reaped to prevent kill on stale PID
        process_reaped = true;
        return false;
    }
    if (result < 0) {
        // Error (e.g., ECHILD) — already reaped or not our child
        process_reaped = true;
        return false;
    }
    return true;
}
```

Note: `process_reaped` is written from a `const` method, so it needs to be
`mutable`. Alternatively, restructure so only `StopProcess` reaps.

Update `StopProcess()` to skip kill/waitpid if already reaped:

```cpp
void StdioTransport::StopProcess() {
    if (process_pid > 0) {
        // Close FDs...
        if (!process_reaped) {
            kill(process_pid, SIGTERM);
            usleep(100000);
            int status;
            int wait_result = waitpid(process_pid, &status, WNOHANG);
            if (wait_result == 0) {
                kill(process_pid, SIGKILL);
                waitpid(process_pid, &status, 0);
            }
        }
        process_pid = -1;
        process_reaped = false;
    }
}
```

## Problem 2: Partial JSON Line Reads (CR-10)

### Current Code (lines 392-432)

After `WaitForData()` returns true, the read loop breaks on `EAGAIN`/`EWOULDBLOCK`
even when no complete line (`\n`) has been found. This returns partial data that
fails JSON parsing.

### Fix

When `EAGAIN` occurs without a complete line, loop back to wait for more data:

```cpp
string StdioTransport::ReadFromProcess() {
    if (stdout_fd < 0) {
        throw IOException("Process stdout not available");
    }

    char buffer[4096];
    string result;

    while (true) {
        if (!WaitForData()) {
            if (result.empty()) {
                throw IOException("Timeout waiting for process response");
            }
            // Partial data received but timeout on more — return what we have
            break;
        }

        ssize_t bytes_read = read(stdout_fd, buffer, sizeof(buffer) - 1);

        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            result += buffer;

            // Check if we have a complete line
            if (result.find('\n') != string::npos) {
                break;
            }
            // No complete line yet — loop to wait for more data
        } else if (bytes_read == 0) {
            // EOF
            break;
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                throw IOException("Error reading from process stdout");
            }
            // EAGAIN without complete line — wait for more data
            // (loop continues to WaitForData)
        }
    }

    StringUtil::RTrim(result);
    return result;
}
```

The key change: instead of `break` on `EAGAIN`, continue the outer loop which
re-enters `WaitForData()`.

## Problem 3: Fragile Sleep After Fork (CR-21)

### Current Code (line 312)

```cpp
usleep(100000); // 100ms
```

### Fix

Replace with a targeted check. After forking, poll the child's stdout for data
readiness or verify the process is alive:

```cpp
// Instead of fixed sleep, poll briefly to detect early failure
for (int i = 0; i < 10; i++) {
    if (!IsProcessRunning()) {
        // Child died immediately — startup failure
        close(stdin_fd); close(stdout_fd); close(stderr_fd);
        stdin_fd = stdout_fd = stderr_fd = -1;
        return false;
    }
    usleep(10000); // 10ms increments, up to 100ms total
}
```

This is still time-based but detects fast failures earlier. A more robust
approach would be to have the child signal readiness on stdout, but that
requires cooperation from the MCP server process which we don't control.

## Verification

1. `make test` passes
2. `make integration_test` passes — especially the stdio transport tests
3. Test with a slow-starting MCP server (e.g., Python server with startup delay)
4. Test with a fast-failing server (e.g., nonexistent command)
5. Verify no SIGTERM sent to wrong process by checking `StopProcess` behavior
   when child has already exited

## Scope

- `src/protocol/mcp_transport.cpp` — all three fixes in this file
- `src/include/protocol/mcp_transport.hpp` — add `process_reaped` member if
  using that approach
