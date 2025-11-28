# Example 07: DevOps MCP Server

A DuckDB MCP server configured for DevOps workflows with extensions for git history, test results, YAML configs, and observability data.

## Features

- **Git History**: Query commits, branches, tags via `duck_tails`
- **Test Results**: Parse 44+ test/build formats via `duck_hunt`
- **YAML Configs**: Process K8s, Docker, CI configs via `yaml`
- **Observability**: Read OpenTelemetry data via `otlp`
- **Network**: IP address functions via `inet`
- **Remote Access**: SSH files, shell commands, zip archives

## Extensions Used

| Extension | Purpose |
|-----------|---------|
| duck_tails | Git repository access (`git://` protocol) |
| duck_hunt | Test/build result parsing |
| yaml | YAML configuration parsing |
| otlp | OpenTelemetry metrics/logs/traces |
| inet | IP address functions |
| hostfs | File system navigation |
| shellfs | Shell command I/O |
| sshfs | SSH file access |
| zipfs | Read from zip archives |

## Quick Start

```bash
# Set the DuckDB binary path (optional)
export DUCKDB=/path/to/duckdb

# Run the MCP server
./launch-mcp.sh
```

## Example Queries

### Git History Analysis
```sql
-- Recent commits
SELECT hash[:8], author_name, message FROM git_log() LIMIT 10;

-- Compare file across versions
SELECT * FROM read_csv('git://data.csv@main')
EXCEPT
SELECT * FROM read_csv('git://data.csv@HEAD~5');
```

### Test Result Parsing
```sql
-- Parse pytest output
SELECT status, COUNT(*) FROM read_test_results('test.log', 'pytest') GROUP BY status;

-- Parse multiple formats
SELECT * FROM read_test_results('junit.xml', 'junit');
SELECT * FROM read_test_results('cargo.log', 'cargo');
```

### YAML Configuration
```sql
-- Parse Kubernetes manifests
SELECT * FROM read_yaml('k8s/**/*.yaml');

-- Extract specific fields
SELECT yaml_extract(content, '$.metadata.name') as name FROM read_yaml('deployment.yaml');
```

### OpenTelemetry Data
```sql
-- Analyze traces
SELECT service_name, AVG(duration_ms) FROM read_otlp_traces('traces.json') GROUP BY 1;

-- Parse logs
SELECT * FROM read_otlp_logs('logs.json') WHERE severity = 'ERROR';
```

### Network Analysis
```sql
-- IP address classification
SELECT ip,
    CASE WHEN ip::INET <<= '10.0.0.0/8'::INET THEN 'private' ELSE 'public' END
FROM access_logs;
```

## Configuration

The server is configured with:
- All read-only tools enabled
- Community extensions for DevOps workflows
- Execute tool disabled for safety

## Use with Claude Desktop

Add to your MCP configuration:
```json
{
  "mcpServers": {
    "duckdb-devops": {
      "command": "/path/to/examples/07-devops/launch-mcp.sh"
    }
  }
}
```
