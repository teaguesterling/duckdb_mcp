# Task: Fix CSV Output Quoting in ExportToolHandler

**GitHub Issue:** #23
**Finding:** CR-05
**Severity:** Medium
**File:** `src/server/tool_handlers.cpp`

## Problem

`ExportToolHandler::FormatData()` (lines 541-565) generates CSV without RFC 4180
quoting. Values containing commas, double quotes, or newlines produce malformed
output.

## Current Code (lines 541-565)

```cpp
} else if (format == "csv") {
    string csv;
    // Header
    for (idx_t col = 0; col < result.names.size(); col++) {
        if (col > 0) csv += ",";
        csv += result.names[col];     // BUG: unquoted
    }
    csv += "\n";
    // Data
    while (auto chunk = result.Fetch()) {
        for (idx_t i = 0; i < chunk->size(); i++) {
            for (idx_t col = 0; col < chunk->ColumnCount(); col++) {
                if (col > 0) csv += ",";
                auto value = chunk->GetValue(col, i);
                csv += value.ToString();              // BUG: unquoted
            }
            csv += "\n";
        }
    }
    return csv;
}
```

## Fix

Add a helper function to properly quote CSV fields per RFC 4180:

```cpp
static string QuoteCSVField(const string &field) {
    // Quote if field contains comma, double quote, newline, or carriage return
    bool needs_quoting = false;
    for (char c : field) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needs_quoting = true;
            break;
        }
    }
    if (!needs_quoting) {
        return field;
    }
    // Escape double quotes by doubling them, wrap in quotes
    string result = "\"";
    for (char c : field) {
        if (c == '"') {
            result += "\"\"";
        } else {
            result += c;
        }
    }
    result += "\"";
    return result;
}
```

Then apply to both headers and values:

```cpp
csv += QuoteCSVField(result.names[col]);   // header
csv += QuoteCSVField(value.ToString());    // value
```

NULL values should be emitted as empty (no quotes) since CSV has no null
literal — check `value.IsNull()` before calling `ToString()`.

## Verification

1. Run `make test`
2. Test with data containing commas: `SELECT 'hello, world' as val`
3. Test with data containing quotes: `SELECT 'say "hi"' as val`
4. Test with data containing newlines: `SELECT E'line1\nline2' as val`
5. Verify output parses correctly with a CSV parser

## Scope

Only `src/server/tool_handlers.cpp`. Add the `QuoteCSVField` helper as a static
function near the top (next to `EscapeJsonString` and `EscapeSQLString`), then
apply in `FormatData`.
