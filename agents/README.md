# DuckDB MCP Agent Definitions

This folder contains Claude Code subagent definitions for working with the DuckDB MCP extension.

## Available Agents

| Agent | Description |
|-------|-------------|
| [duckdb-mcp-analyst](./duckdb-mcp-analyst.md) | Data analyst with direct DuckDB access through MCP |

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

## Contributing

To add or update agent definitions:

1. Edit the agent file in this folder
2. Test with Claude Code to verify it works
3. Consider contributing back to awesome-claude-code-subagents
