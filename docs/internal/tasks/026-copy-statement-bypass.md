# Task: Block COPY Statements in ExecuteToolHandler

**GitHub Issue:** #26
**Finding:** CR-07
**Severity:** Medium
**File:** `src/server/tool_handlers.cpp`

## Problem

`StatementType::COPY_STATEMENT` is not classified by any of the `Is*Statement()`
methods, so `IsAllowedStatement()` falls through to `return true`. COPY can
read/write arbitrary files, bypassing intended restrictions.

## Current Code (lines 1318-1355)

```cpp
bool ExecuteToolHandler::IsAllowedStatement(StatementType type) const {
    if (type == StatementType::SELECT_STATEMENT) return false;
    if (IsDMLStatement(type) && !allow_dml) return false;
    if (IsSafeDDLStatement(type) && !allow_ddl) return false;
    if (IsLoadStatement(type) && !allow_load) return false;
    if (IsAttachStatement(type) && !allow_attach) return false;
    if (IsSetStatement(type) && !allow_set) return false;

    switch (type) {
    case StatementType::EXPLAIN_STATEMENT:
    case StatementType::RELATION_STATEMENT:
    case StatementType::CALL_STATEMENT:
        return false;
    default:
        break;
    }
    return true;  // COPY_STATEMENT falls through here
}
```

## Recommended Fix

Block COPY by default. The simplest approach is to add it to the explicit block
list since the export tool already provides controlled file export:

```cpp
switch (type) {
case StatementType::EXPLAIN_STATEMENT:
case StatementType::RELATION_STATEMENT:
case StatementType::CALL_STATEMENT:
case StatementType::COPY_STATEMENT:    // ADD: block COPY (use export tool)
    return false;
default:
    break;
}
```

### Alternative: Gate Behind a Flag

If COPY should be optionally allowed, add a flag:

1. Add `bool allow_copy = false;` to `ExecuteToolHandler` and `MCPServerConfig`
2. Add `IsCopyStatement()` method
3. Gate: `if (IsCopyStatement(type) && !allow_copy) return false;`

### Also Consider

Check if there are other `StatementType` values that could fall through. Review
the DuckDB `StatementType` enum for any new types that should be explicitly
handled. A safer default would be to allowlist rather than denylist:

```cpp
// Safer: only allow what we explicitly understand
bool ExecuteToolHandler::IsAllowedStatement(StatementType type) const {
    if (IsDMLStatement(type)) return allow_dml;
    if (IsSafeDDLStatement(type)) return allow_ddl;
    if (IsLoadStatement(type)) return allow_load;
    if (IsAttachStatement(type)) return allow_attach;
    if (IsSetStatement(type)) return allow_set;
    return false;  // Unknown types blocked by default
}
```

This is a more significant refactor but prevents future bypass from new
statement types.

## Verification

1. `make test` passes
2. With execute tool enabled, verify:
   - `COPY (SELECT 1) TO '/tmp/test.csv'` is rejected
   - `INSERT INTO t VALUES (1)` still works (if DML allowed)
   - `CREATE TABLE t (id INT)` still works (if DDL allowed)

## Scope

- `src/server/tool_handlers.cpp` — `IsAllowedStatement()` method
- Optionally `src/include/server/tool_handlers.hpp` and
  `src/include/server/mcp_server.hpp` if adding a flag
