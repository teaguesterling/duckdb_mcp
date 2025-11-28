# Example 09: Documentation MCP Server

A DuckDB MCP server optimized for documentation processing, text analysis, and report generation workflows.

## Features

- **Markdown Processing**: Parse and query markdown documents
- **YAML Front Matter**: Extract metadata from documentation files
- **Full-Text Search**: Index and search document content
- **Fuzzy Matching**: Find similar text with rapidfuzz
- **Text Normalization**: Pluralization, singularization, case conversion
- **Data Visualization**: ASCII charts with textplot
- **Template Rendering**: Generate reports with minijinja
- **Excel Export**: Create formatted spreadsheets

## Extensions Used

| Extension | Type | Purpose |
|-----------|------|---------|
| markdown | Community | Parse markdown into queryable structure |
| yaml | Community | Parse YAML files and front matter |
| fts | Core | Full-text search indexing |
| rapidfuzz | Community | Fuzzy string matching |
| inflector | Community | Word inflection (plural/singular) |
| textplot | Community | ASCII visualization |
| minijinja | Community | Jinja2 templating |
| excel | Core | Read/write Excel files |
| json | Core | JSON processing |

## Quick Start

```bash
cd examples/09-docs
chmod +x launch-mcp.sh
./launch-mcp.sh
```

## Example Queries

### Parse Markdown Structure
```sql
-- Parse a markdown file into sections
SELECT * FROM read_markdown('docs/README.md');

-- Extract all headers
SELECT header_level, header_text
FROM read_markdown('docs/guide.md')
WHERE header_level IS NOT NULL;
```

### YAML Front Matter
```sql
-- Parse YAML front matter from docs
SELECT
    filename,
    yaml_extract(front_matter, '$.title') as title,
    yaml_extract(front_matter, '$.author') as author,
    yaml_extract(front_matter, '$.date') as date
FROM glob('docs/**/*.md') AS g(filename),
LATERAL (SELECT split_part(read_text(filename), '---', 2) as front_matter);
```

### Full-Text Search
```sql
-- Create FTS index on documents
PRAGMA create_fts_index('documents', 'id', 'title', 'content');

-- Search with ranking
SELECT *, fts_main_documents.match_bm25(id, 'query terms') AS score
FROM documents
WHERE score IS NOT NULL
ORDER BY score DESC;
```

### Fuzzy Text Matching
```sql
-- Find similar function names
SELECT
    name,
    rapidfuzz_ratio(name, 'get_user') as similarity
FROM functions
WHERE rapidfuzz_ratio(name, 'get_user') > 70
ORDER BY similarity DESC;

-- Deduplicate similar entries
SELECT DISTINCT ON (group_id)
    name,
    rapidfuzz_partial_ratio(name, first_value(name) OVER w) as similarity
FROM entries
WINDOW w AS (PARTITION BY group_id ORDER BY id);
```

### Text Normalization
```sql
-- Pluralize/singularize words
SELECT
    inflect_pluralize('category') as plural,     -- 'categories'
    inflect_singularize('indices') as singular,  -- 'index'
    inflect_titleize('hello_world') as title,    -- 'Hello World'
    inflect_tableize('UserAccount') as table_name; -- 'user_accounts'
```

### ASCII Visualization
```sql
-- Create histogram
SELECT textplot_histogram(values, 20)
FROM (SELECT random() * 100 as values FROM range(1000));

-- Line chart
SELECT textplot_line(x, y, 40, 10)
FROM timeseries;
```

### Template Rendering
```sql
-- Render report template
SELECT minijinja_render(
    'Report for {{ title }}\n\nItems:\n{% for item in items %}- {{ item }}\n{% endfor %}',
    json_object('title', 'Q4 Summary', 'items', ['Revenue', 'Costs', 'Profit'])
);

-- Render from file
SELECT minijinja_render_file(
    'templates/report.jinja2',
    json_object('data', (SELECT json_group_array(row) FROM summary))
);
```

### Excel Integration
```sql
-- Read Excel file
SELECT * FROM read_excel('data/report.xlsx', sheet='Summary');

-- Write results to Excel
COPY (SELECT * FROM final_report) TO 'output/report.xlsx' (FORMAT EXCEL);
```

## Use Cases

1. **Documentation Site Generation**: Parse markdown docs, extract metadata, generate index
2. **Content Search**: Build searchable documentation with full-text indexing
3. **Style Guide Enforcement**: Use fuzzy matching to find inconsistent terminology
4. **Report Generation**: Combine queries with Jinja2 templates
5. **Data-Driven Docs**: Query databases and render into documentation templates
6. **Excel Reporting**: Generate formatted spreadsheets from query results

## Configuration

The server enables these MCP tools:
- `query`: Run SQL queries with documentation extensions
- `describe`: Get table/view schema information
- `list_tables`: List available tables
- `database_info`: Get database metadata
- `export`: Export query results to files

Execute tool is disabled for safety.
