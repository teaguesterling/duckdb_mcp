# Open Issues — Master Tracker

Last updated: 2026-03-02

## Overview

| Category | Open | Fixed | Total |
|----------|------|-------|-------|
| Code review (CR-01–23) | 11 | 12 | 23 |
| Other bugs/features | 1 | 1 | 2 |

---

## Tier 1 — Bugs with user-visible impact

| Issue | Title | Source | Effort |
|-------|-------|--------|--------|
| [#8](https://github.com/teaguesterling/duckdb_mcp/issues/8) | Format fallback: jsonl, table, box not implemented | User report | Medium |
| [#41](https://github.com/teaguesterling/duckdb_mcp/issues/41) | Non-string JSON array items silently dropped in config | CR-22 | Small |
| [#37](https://github.com/teaguesterling/duckdb_mcp/issues/37) | ExportToFile double-executes the query | CR-16 | Small |

**#8**: `query` and `export` tools accept `jsonl`, `table`, and `box` format
options but silently fall back to the default tab-separated output. Formats that
work: `json`, `markdown`, `csv`.

**#41**: Config arrays like `["--port", 8080]` silently drop non-string items.
Fix: convert numeric/boolean to string, or error on unsupported types.

**#37**: `Execute()` runs the query to check for errors, then `ExportToFile()`
runs it again via `COPY`. Remove the pre-check or restructure to reuse the
result.

## Tier 2 — Security hardening

| Issue | Title | Source | Effort |
|-------|-------|--------|--------|
| [#42](https://github.com/teaguesterling/duckdb_mcp/issues/42) | CORS defaults to wildcard origin | CR-23 | Small |
| [#39](https://github.com/teaguesterling/duckdb_mcp/issues/39) | Constant-time token comparison leaks token length | CR-18 | Small |

**#42**: `cors_origins = "*"` is inconsistent with security-by-default posture.
Default should be empty/restrictive with explicit opt-in.

**#39**: Dummy loop iterates `a.size()` not `max(a.size(), b.size())`. Trivial
fix.

## Tier 3 — Code quality / maintainability

| Issue | Title | Source | Effort |
|-------|-------|--------|--------|
| [#40](https://github.com/teaguesterling/duckdb_mcp/issues/40) | Duplicated HTTP server setup between Start() and RunHTTPLoop() | CR-20 | Medium |
| [#28](https://github.com/teaguesterling/duckdb_mcp/issues/28) | Umbrella: error leaks, silent failures, stubs | CR-06/14/15/19 | Medium |

**#40**: HTTP config construction and handler lambda are copy-pasted between two
methods. Extract a common helper.

**#28 remaining sub-items:**
- CR-06 — HTTP handler lambdas leak `e.what()` to clients (unescaped)
- CR-14 — `ListResources` client stub (always returns empty)
- CR-15 — `ListTools` client stub (always returns empty)
- CR-19 — `ApplyPendingRegistrations` swallows exceptions silently

## Tier 4 — Architectural / longer-term

| Issue | Title | Source | Effort |
|-------|-------|--------|--------|
| [#38](https://github.com/teaguesterling/duckdb_mcp/issues/38) | Global singletons share state across database instances | CR-17 | Large |

**#38**: `MCPSecurityConfig`, `MCPServerManager`, `MCPConfigManager`,
`MCPLogger`, `MCPTemplateManager` are global singletons. Correct fix requires
moving state into per-`DatabaseInstance` storage via DuckDB's `ObjectCache` or
attached state.

---

## Recently Fixed

| Issue | Title | Fixed by | Date |
|-------|-------|----------|------|
| [#34](https://github.com/teaguesterling/duckdb_mcp/issues/34) | Query tool does not enforce read-only | PR #45 | 2026-03-02 |
| [#36](https://github.com/teaguesterling/duckdb_mcp/issues/36) | ParseCapabilities hardcodes wrong defaults | PR #44 | 2026-03-02 |
| [#25](https://github.com/teaguesterling/duckdb_mcp/issues/25) | Thread safety issues across multiple components | PR #35 | 2026-03-02 |
| [#27](https://github.com/teaguesterling/duckdb_mcp/issues/27) | StdioTransport PID reuse race and partial reads | PR #33 | 2026-03-02 |
| [#26](https://github.com/teaguesterling/duckdb_mcp/issues/26) | COPY statement not blocked in ExecuteToolHandler | PR #32 | 2026-03-02 |
| [#22](https://github.com/teaguesterling/duckdb_mcp/issues/22) | Missing JSON escaping in tool handler outputs | PR #31 | 2026-02-28 |
| [#24](https://github.com/teaguesterling/duckdb_mcp/issues/24) | Registry dangling pointer after lock release | PR #30 | 2026-02-28 |
| [#23](https://github.com/teaguesterling/duckdb_mcp/issues/23) | CSV output lacks proper quoting | PR #29 | 2026-02-28 |
| [#43](https://github.com/teaguesterling/duckdb_mcp/issues/43) | Query format fallback (dup of #8) | Closed | 2026-03-02 |
| [#19](https://github.com/teaguesterling/duckdb_mcp/issues/19) | Optional tool parameters not substituted with NULL | PR #20 | 2026-02-27 |
| [#21](https://github.com/teaguesterling/duckdb_mcp/issues/21) | Document mcp_publish_tool function signatures | PR #20 | 2026-02-27 |
