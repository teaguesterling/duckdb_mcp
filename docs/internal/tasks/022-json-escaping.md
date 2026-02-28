# Task: Fix Missing JSON Escaping in Tool Handler Outputs

**GitHub Issue:** #22
**Findings:** CR-01, CR-02, CR-03
**Severity:** High
**File:** `src/server/tool_handlers.cpp`

## Problem

Three tool handlers manually build JSON strings but interpolate user-derived
values without escaping. The `EscapeJsonString()` function is defined at the top
of the same file (lines 13-46) and correctly used by `DescribeToolHandler`, but
is missing from three other handlers.

## Specific Locations

### CR-01: ListTablesToolHandler::Execute (lines 840-858)

Six values need escaping:

```
line 848: db_name.ToString()
line 849: schema_name.ToString()
line 850: table_name.ToString()
line 851: obj_type.ToString()
```

Pattern to apply:
```cpp
// Before:
json += "\"database\":\"" + db_name.ToString() + "\",";
// After:
json += "\"database\":\"" + EscapeJsonString(db_name.ToString()) + "\",";
```

### CR-02: DatabaseInfoToolHandler (5 methods, lines 933-1165)

#### GetDatabasesInfo() (lines 933-978)
- line 960: `chunk->GetValue(0, i).ToString()` (database name)
- line 966: `path.ToString()` (file path)
- line 969: `chunk->GetValue(2, i).ToString()` (type)

#### GetSchemasInfo() (lines 980-1013)
- line 1005: `chunk->GetValue(0, i).ToString()` (database)
- line 1006: `chunk->GetValue(1, i).ToString()` (schema name)

#### GetTablesInfo() (lines 1016-1061)
- line 1044: `chunk->GetValue(0, i).ToString()` (database)
- line 1045: `chunk->GetValue(1, i).ToString()` (schema)
- line 1046: `chunk->GetValue(2, i).ToString()` (table name)

#### GetViewsInfo() (lines 1064-1101)
- line 1091: `chunk->GetValue(0, i).ToString()` (database)
- line 1092: `chunk->GetValue(1, i).ToString()` (schema)
- line 1093: `chunk->GetValue(2, i).ToString()` (view name)

#### GetExtensionsInfo() (lines 1103-1165)
- line 1131: `chunk->GetValue(0, i).ToString()` (extension name)
- line 1157: `version.ToString()` (version string)
- lines 1142-1149: Replace the manual partial escape with `EscapeJsonString()`:

```cpp
// Before (incomplete escape):
string desc_str = desc.ToString();
string escaped;
for (char c : desc_str) {
    if (c == '"') escaped += "\\\"";
    else if (c == '\\') escaped += "\\\\";
    else escaped += c;
}
json += "\"description\":\"" + escaped + "\",";

// After:
json += "\"description\":\"" + EscapeJsonString(desc.ToString()) + "\",";
```

### CR-03: ExportToolHandler::FormatData JSON branch (lines 510-539)

- line 526: `result.names[col]` (column name)
- line 532: `value.ToString()` (cell value)

Pattern:
```cpp
// Before:
json += "\"" + result.names[col] + "\":";
json += "\"" + value.ToString() + "\"";
// After:
json += "\"" + EscapeJsonString(result.names[col]) + "\":";
json += "\"" + EscapeJsonString(value.ToString()) + "\"";
```

## Verification

After making changes:

1. Run `make test` to ensure existing tests pass
2. Create a table with special characters in its name:
   ```sql
   CREATE TABLE "test""quote" (id INTEGER);
   CREATE TABLE "test\nline" (id INTEGER);
   ```
3. Verify `list_tables` and `database_info` tools produce valid JSON
4. Verify `export` tool with format=json produces valid JSON

## Scope

Only `src/server/tool_handlers.cpp` needs changes. No header changes. No new
functions needed — the existing `EscapeJsonString()` at the top of the file
handles all cases including control characters.
