# Subagent Task Files

Task files for addressing findings from the February 2026 code review
(`docs/internal/CODE_REVIEW_2026_02.md`). Each file maps to a GitHub issue and
contains enough context for an agent to implement the fix independently.

## Task Index

| File | Issue | Severity | Summary |
|------|-------|----------|---------|
| [022-json-escaping.md](022-json-escaping.md) | #22 | High | Apply `EscapeJsonString()` to ListTables, DatabaseInfo, Export handlers |
| [023-csv-escaping.md](023-csv-escaping.md) | #23 | Medium | Add RFC 4180 quoting to CSV output in ExportToolHandler |
| [024-registry-use-after-free.md](024-registry-use-after-free.md) | #24 | High | Switch registries to `shared_ptr` to prevent dangling pointers |
| [025-thread-safety.md](025-thread-safety.md) | #25 | Medium | Add mutexes to MCPSecurityConfig, MCPConnection, MCPServer |
| [026-copy-statement-bypass.md](026-copy-statement-bypass.md) | #26 | Medium | Block COPY statements in ExecuteToolHandler |
| [027-transport-bugs.md](027-transport-bugs.md) | #27 | Medium | Fix PID reuse race, partial reads, and fork startup in StdioTransport |
| [028-code-quality.md](028-code-quality.md) | #28 | Low | Error leaks, silent failures, stubs, timing, CORS default |

## Recommended Order

1. **022** — JSON escaping (straightforward, high impact, single file)
2. **023** — CSV escaping (straightforward, single file)
3. **026** — COPY bypass (small change, security impact)
4. **025** — Thread safety (moderate scope, important for correctness)
5. **027** — Transport bugs (moderate scope, affects reliability)
6. **024** — Registry use-after-free (larger refactor, touches multiple files)
7. **028** — Code quality (collection of independent items, incremental)
