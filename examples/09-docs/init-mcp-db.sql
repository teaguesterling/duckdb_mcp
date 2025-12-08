-- init-mcp-db.sql: Documentation MCP server initialization
-- Loads extensions for markdown, YAML, FTS, fuzzy matching, and templating

-- ============================================
-- Load Core Extensions
-- ============================================

-- JSON processing
INSTALL json;
LOAD json;

-- Full-text search
INSTALL fts;
LOAD fts;

-- Excel file support
INSTALL excel;
LOAD excel;

-- Remote file access (for fetching remote docs)
INSTALL httpfs;
LOAD httpfs;

-- ============================================
-- Load Community Extensions
-- ============================================

-- Markdown parsing
INSTALL markdown FROM community;
LOAD markdown;

-- YAML parsing
INSTALL yaml FROM community;
LOAD yaml;

-- Fuzzy string matching
INSTALL rapidfuzz FROM community;
LOAD rapidfuzz;

-- Word inflection
INSTALL inflector FROM community;
LOAD inflector;

-- ASCII charts
INSTALL textplot FROM community;
LOAD textplot;

-- Jinja2 templating
INSTALL minijinja FROM community;
LOAD minijinja;

.print '[init-mcp-db] Documentation extensions loaded'

-- ============================================
-- Helper Macros
-- ============================================

-- Extract YAML front matter from markdown content
CREATE OR REPLACE MACRO extract_front_matter(content) AS (
    CASE
        WHEN content LIKE '---%'
        THEN split_part(split_part(content, '---', 2), '---', 1)
        ELSE NULL
    END
);

-- Parse markdown file with front matter
CREATE OR REPLACE MACRO parse_doc(file_path) AS (
    SELECT
        file_path as path,
        extract_front_matter(content) as front_matter,
        CASE
            WHEN content LIKE '---%'
            THEN trim(split_part(content, '---', 3))
            ELSE content
        END as body
    FROM (SELECT read_text(file_path) as content)
);

-- Similarity search helper
CREATE OR REPLACE MACRO find_similar(text, target, threshold := 70) AS (
    rapidfuzz_ratio(text, target) >= threshold
);

.print '[init-mcp-db] Helper macros created'

-- ============================================
-- Sample Document Structure
-- ============================================

-- Documents table for FTS demo
CREATE TABLE IF NOT EXISTS documents (
    id INTEGER PRIMARY KEY,
    title VARCHAR,
    content TEXT,
    tags VARCHAR[],
    created_at TIMESTAMP DEFAULT current_timestamp
);

-- Create FTS index (commented - run after loading data)
-- PRAGMA create_fts_index('documents', 'id', 'title', 'content');

.print '[init-mcp-db] Sample tables created'
