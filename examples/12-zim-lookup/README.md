# ZIM Article Lookup MCP Server

A DuckDB MCP server for searching and reading articles from ZIM files (e.g., Wikipedia).

## Overview

This example demonstrates using DuckDB with the `zim` extension to create an MCP server that can:

- **Search** articles by keyword with relevance scoring
- **Retrieve** full article text (HTML → Markdown conversion)
- **List** available articles in a ZIM file

## Setup

### Files

- `init-mcp-server.sql` — DuckDB init script (loads extensions, starts MCP)
- `mcp.json` — MCP client configuration
- `README.md` — This file

### Requirements

- DuckDB ≥ 1.5.5 (with `zim` extension available)
- A ZIM file (e.g., [Wikipedia Mini](https://download.kiwix.org/zim/wikipedia/en.wikipedia.org/wikipedia_en_mini_2024-01.zim))

## Quick Start

### As an MCP Server (for AI Assistants)

```bash
# Set the ZIM file path
export ZIM_FILES=/path/to/wikipedia_en_mini.zim

# Start the server (stdio transport, blocks until stdin closes)
duckdb -init init-mcp-server.sql
```

Add to Claude Desktop configuration:

```json
{
  "mcpServers": {
    "zim-lookup": {
      "command": "duckdb",
      "args": ["-init", "/path/to/init-mcp-server.sql"],
      "env": {
        "ZIM_FILES": "/path/to/wikipedia_en_mini.zim"
      }
    }
  }
}
```

### Testing Directly

```bash
# Search articles
ZIM_FILES=/path/to/file.zim duckdb -c "
LOAD duckdb_mcp; LOAD zim; LOAD webbed; LOAD duck_block_utils; LOAD markdown;
SELECT title, snippet FROM zim_search(getenv('ZIM_FILES'), 'quantum mechanics',
  ignore_errors := true, with_snippet := true,
  result_offset := CAST(0 AS BIGINT), max_results := CAST(10 AS BIGINT));
" --ascii

# Get full article as Markdown
ZIM_FILES=/path/to/file.zim duckdb -c "
LOAD duckdb_mcp; LOAD zim; LOAD webbed; LOAD duck_block_utils; LOAD markdown;
SELECT duck_blocks_to_md(html_to_duck_blocks(zim_get_text(getenv('ZIM_FILES'), 'Protein')));
" --markdown
```

## How It Works

### Architecture

```
┌─────────────┐     ┌──────────────────┐     ┌──────────────┐
│  MCP Client  │────▶│  DuckDB MCP      │────▶│  ZIM File    │
│  (Claude,    │     │  Server          │     │  (Wikipedia) │
│   OpenCode)  │◀────│  (stdio)         │◀────│              │
└─────────────┘     └──────────────────┘     └──────────────┘
                        │
                        ├── Built-in `query` tool (SQL with getenv())
                        └── Extensions: zim, webbed, duck_block_utils, markdown
```

### Key Patterns

1. **`getenv('ZIM_FILES')`** — The ZIM file path is passed via environment variable, not as a tool parameter. This avoids the `mcp_publish_tool` limitation with table function parameter binding.

2. **Built-in `query` tool** — Rather than publishing custom tools (which don't support table function parameters), we use the built-in `query` tool with SQL templates that reference `getenv('ZIM_FILES')`.

3. **HTML → Markdown pipeline** — ZIM article text is stored as HTML. The conversion chain is:
   ```sql
   duck_blocks_to_md(html_to_duck_blocks(zim_get_text(getenv('ZIM_FILES'), 'Article Name')))
   ```

## SQL Patterns

### Search Articles

```sql
SELECT title, snippet, score
FROM zim_search(getenv('ZIM_FILES'), 'search term',
  ignore_errors := true,
  with_snippet := true,
  result_offset := CAST(0 AS BIGINT),
  max_results := CAST(10 AS BIGINT));
```

### Get Full Article

```sql
SELECT duck_blocks_to_md(html_to_duck_blocks(
    zim_get_text(getenv('ZIM_FILES'), 'Article Name')));
```

### Search with Article Content

```sql
SELECT title, snippet
FROM zim_search(getenv('ZIM_FILES'), 'search term',
  ignore_errors := true,
  with_snippet := true,
  result_offset := CAST(0 AS BIGINT),
  max_results := CAST(10 AS BIGINT));
```

## Notes

- **Parameter binding limitation**: `mcp_publish_tool` does not bind `$param` placeholders for table functions (see issue #71). Use the built-in `query` tool with hardcoded SQL patterns instead.
- **Extension loading order**: Load `zim` before `webbed` and `duck_block_utils`.
- **Markdown conversion**: Requires `LOAD markdown` in addition to `webbed` and `duck_block_utils`.
- **Type casts**: `zim_search` requires `BIGINT` casts for `result_offset` and `max_results`.

## Related

- [duckdb_zim](https://github.com/duckdb/duckdb_zim) — ZIM file support for DuckDB
- [MCP Specification](https://modelcontextprotocol.io/)
- [DuckDB MCP Extension](https://github.com/duckdb/duckdb_mcp)
