# DuckDB MCP Agent Definitions

This folder contains Claude Code subagent definitions for working with the DuckDB MCP extension.

## Available Agents

| Agent | Description | Use Case |
|-------|-------------|----------|
| [duckdb-mcp-analyst](./duckdb-mcp-analyst.md) | Data analyst with direct DuckDB access through MCP | SQL analytics, reporting, insights |
| [duckdb-mcp-explorer](./duckdb-mcp-explorer.md) | Data discovery and profiling specialist | File discovery, schema inference, data cataloging |

## Agent Capabilities

### duckdb-mcp-analyst
- Execute analytical SQL queries
- Database and table discovery
- Data profiling and quality checks
- Export to CSV, JSON, Parquet
- Window functions, CTEs, pivots
- Remote data source access

### duckdb-mcp-explorer
- File system exploration with `glob()`
- Path parsing (`parse_path`, `parse_dirname`, `parse_filename`)
- Schema inference for CSV, Parquet, JSON
- Data quality quick checks
- Remote source testing (HTTP, S3, databases)
- Data catalog generation

## Usage with Claude Code

### Option 1: Copy to Claude Code commands folder

Copy the agent definition to your project's `.claude/commands/` folder:

```bash
cp agents/duckdb-mcp-analyst.md /path/to/project/.claude/commands/
```

Then invoke with `/duckdb-mcp-analyst` in Claude Code.

### Option 2: Reference in CLAUDE.md

Add to your project's `CLAUDE.md` or global `~/.claude/CLAUDE.md`:

```markdown
## Data Analysis

When analyzing data, use the patterns from the duckdb-mcp-analyst agent:
- Always start with `database_info` to understand the database
- Use `describe` before writing queries
- Format results appropriately for the use case
```

### Option 3: Use with awesome-claude-code-subagents

These agents are also published to the [awesome-claude-code-subagents](https://github.com/VoltAgent/awesome-claude-code-subagents) repository under `categories/05-data-ai/`.

## MCP Server Setup

Before using these agents, ensure your DuckDB MCP server is configured. See the [examples](../examples/) folder for configuration templates.

### Quick Start

1. Build the extension:
   ```bash
   make
   ```

2. Configure Claude Desktop (`~/.config/claude/mcp.json`):
   ```json
   {
     "mcpServers": {
       "duckdb": {
         "command": "/path/to/duckdb_mcp/examples/01-simple/launch-mcp.sh"
       }
     }
   }
   ```

3. The agent will have access to tools like:
   - `mcp__duckdb__query` - Execute SQL queries
   - `mcp__duckdb__describe` - Get table/query schemas
   - `mcp__duckdb__list_tables` - Discover available tables
   - `mcp__duckdb__database_info` - Database overview
   - `mcp__duckdb__export` - Export query results

## Key DuckDB Functions for Agents

### File System Functions
| Function | Purpose |
|----------|---------|
| `glob(pattern)` | List files matching a glob pattern |
| `read_text(source)` | Read file content as text |
| `read_blob(source)` | Read file content as binary |

### Path Manipulation
| Function | Purpose |
|----------|---------|
| `parse_path(path)` | Split path into list of components |
| `parse_dirname(path)` | Extract directory name (last folder) |
| `parse_dirpath(path)` | Extract directory path (without filename) |
| `parse_filename(path, trim_ext)` | Extract filename, optionally without extension |

### File Readers
| Function | Formats |
|----------|---------|
| `read_csv()` | CSV, TSV, delimited files |
| `read_parquet()` | Parquet columnar format |
| `read_json()` | JSON, JSON Lines |
| `read_csv_auto()` | CSV with auto-detection |

### Remote Sources
| Protocol | Example |
|----------|---------|
| HTTP/HTTPS | `'https://example.com/data.csv'` |
| S3 | `'s3://bucket/path/file.parquet'` |
| Azure | `'azure://container/path/file.csv'` |
| GCS | `'gcs://bucket/path/file.json'` |

## Contributing

To add or update agent definitions:

1. Edit the agent file in this folder
2. Test with Claude Code to verify it works
3. Consider contributing back to awesome-claude-code-subagents
