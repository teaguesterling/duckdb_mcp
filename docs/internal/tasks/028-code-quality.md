# Task: Code Quality Fixes

**GitHub Issue:** #28
**Findings:** CR-06, CR-13, CR-14, CR-15, CR-16, CR-17, CR-18, CR-19, CR-20,
CR-22, CR-23
**Severity:** Low / Design
**Files:** Multiple

This task bundles lower-severity issues. They can be addressed incrementally.
Each section is independent.

---

## 1. Fix HTTP Error Handler Leaks (CR-06, CR-20)

**Files:** `src/server/mcp_server.cpp`

### Problem

Two identical lambda handlers at lines 170-178 and 349-357 inline `e.what()`
into JSON responses without escaping. This (a) leaks internal details and
(b) produces malformed JSON if the exception message contains `"`.

### Fix

Extract a shared helper and return a generic error:

```cpp
// Add as a private static method or free function:
static string MakeParseErrorResponse() {
    return R"({"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error"},"id":null})";
}
```

Replace both lambda catch blocks:
```cpp
} catch (const std::exception &e) {
    return MakeParseErrorResponse();
}
```

This also resolves CR-20 (code duplication) — extract the shared lambda and
HTTPServerConfig construction into a helper method to eliminate the copy-paste
between `Start()` and `RunHTTPLoop()`.

---

## 2. Log Errors in ApplyPendingRegistrations (CR-19)

**File:** `src/server/mcp_server.cpp:888-896` and `916-918`

### Problem

The catch blocks say "Log error but continue" but log nothing.

### Fix

Use the existing MCPLogger:

```cpp
#include "duckdb_mcp_logging.hpp"

} catch (const std::exception &e) {
    MCP_LOG_ERROR("SERVER", "Failed to register pending tool '%s': %s",
                  reg.name.c_str(), e.what());
}
```

Same pattern for the resource registration catch blocks.

---

## 3. Fix ConstantTimeEquals Length Leak (CR-18)

**File:** `src/server/http_server_transport.cpp:14-30`

### Problem

When lengths differ, the dummy loop iterates `a.size()` times instead of
`max(a.size(), b.size())`.

### Fix

```cpp
static bool ConstantTimeEquals(const string &a, const string &b) {
    size_t max_len = std::max(a.size(), b.size());
    volatile unsigned char result = (a.size() != b.size()) ? 1 : 0;
    for (size_t i = 0; i < max_len; i++) {
        unsigned char ca = (i < a.size()) ? static_cast<unsigned char>(a[i]) : 0;
        unsigned char cb = (i < b.size()) ? static_cast<unsigned char>(b[i]) : 0;
        result |= ca ^ cb;
    }
    return result == 0;
}
```

---

## 4. Fix Silent JSON Array Item Drop (CR-22)

**File:** `src/duckdb_mcp_security.cpp:300-304` and `342`

### Problem

Non-string items in ARGS JSON arrays are silently ignored. `["--port", 8080]`
loses the `8080`.

### Fix

Coerce non-string values to strings:

```cpp
yyjson_arr_foreach(root, idx, max, arg) {
    if (yyjson_is_str(arg)) {
        params.args.push_back(yyjson_get_str(arg));
    } else if (yyjson_is_int(arg)) {
        params.args.push_back(std::to_string(yyjson_get_sint(arg)));
    } else if (yyjson_is_real(arg)) {
        params.args.push_back(std::to_string(yyjson_get_real(arg)));
    } else if (yyjson_is_bool(arg)) {
        params.args.push_back(yyjson_get_bool(arg) ? "true" : "false");
    }
    // null and object/array items are still skipped (no reasonable coercion)
}
```

Same pattern for ENV values at line 342 (values should all be strings, but
coercing numbers is reasonable).

---

## 5. Change CORS Default (CR-23)

**File:** `src/include/server/mcp_server.hpp:54`

### Problem

CORS defaults to `"*"` (allow all origins). Inconsistent with security-by-default.

### Fix

Change default to empty string (disabled):

```cpp
string cors_origins;  // Empty = CORS disabled by default
```

Update documentation in `docs/guides/server-usage.md` and
`docs/reference/configuration.md` to note that CORS must be explicitly enabled.

Ensure examples that need CORS (e.g., `examples/08-web-api`) explicitly set it.

---

## 6. Fix ParseCapabilities Stub (CR-13)

**File:** `src/protocol/mcp_connection.cpp:269-278`

### Problem

Hardcodes `supports_tools = false` regardless of server response.

### Fix

Parse the actual capabilities from the server's initialize response:

```cpp
void MCPConnection::ParseCapabilities(const Value &server_info) {
    // Default everything to false
    capabilities = MCPCapabilities();

    if (server_info.IsNull() || server_info.type().id() != LogicalTypeId::STRUCT) {
        return;
    }

    // Parse from the result struct — look for "capabilities" key
    // The initialize response has: { protocolVersion, serverInfo, capabilities }
    // capabilities contains: { resources: {...}, tools: {...}, prompts: {...} }
    auto &children = StructValue::GetChildren(server_info);
    for (size_t i = 0; i < children.size(); i++) {
        auto &key = StructType::GetChildName(server_info.type(), i);
        if (key == "capabilities" && !children[i].IsNull()) {
            auto &cap_children = StructValue::GetChildren(children[i]);
            for (size_t j = 0; j < cap_children.size(); j++) {
                auto &cap_key = StructType::GetChildName(children[i].type(), j);
                if (cap_key == "resources") capabilities.supports_resources = true;
                if (cap_key == "tools") capabilities.supports_tools = true;
                if (cap_key == "prompts") capabilities.supports_prompts = true;
            }
        }
    }
}
```

Note: The actual format depends on how `MCPMessage::FromJSON` deserializes the
response. The value may be a JSON VARCHAR rather than a STRUCT, in which case
parse with yyjson instead.

---

## 7. Note on Stub List Methods (CR-14, CR-15)

**File:** `src/protocol/mcp_connection.cpp:104-126` and `169-184`

`ListResources()` and `ListTools()` receive server responses but don't parse
them. These are client-side methods used when DuckDB ATTACHes an external MCP
server. Completing them requires:

1. Parsing the JSON-RPC response result
2. Extracting the `resources`/`tools` arrays
3. Building the return vectors

This is a larger effort and may warrant its own issue if client functionality
needs to be production-ready. For now, adding a `// TODO` with the issue
reference is acceptable.

---

## 8. Note on Singletons (CR-17)

The singleton architecture (`MCPSecurityConfig`, `MCPServerManager`,
`MCPConfigManager`, `MCPLogger`, `MCPTemplateManager`) shares state across
database instances. This is a design-level issue that would require significant
refactoring to address (moving to per-instance state via DuckDB's
`DatabaseInstance` extension data).

This is documented but not actionable in this task. It should be considered if
the extension needs to support multi-database scenarios.

---

## 9. Note on Double Query Execution (CR-16)

`ExportToolHandler::Execute()` runs the query once for validation, then
`ExportToFile` runs it again via COPY. To fix: skip the initial `conn.Query()`
when `output_path` is non-empty and let COPY handle errors. Or use the result
of the first query directly for inline returns and only use COPY for file
exports.

---

## Verification

After each fix, run `make test` and `make integration_test`. These are
independent changes and can be committed separately.
