-- init-mcp-db.sql: Developer MCP server initialization
-- Loads extensions for AST parsing, git, test results, and developer tooling

-- ============================================
-- Load Core Extensions
-- ============================================

-- JSON processing
INSTALL json;
LOAD json;

-- Remote file access
INSTALL httpfs;
LOAD httpfs;

-- ============================================
-- Load Community Extensions
-- ============================================

-- SQL/language AST parsing
INSTALL sitting_duck FROM community;
LOAD sitting_duck;

-- Git history access
INSTALL duck_tails FROM community;
LOAD duck_tails;

-- Test result parsing
INSTALL duck_hunt FROM community;
LOAD duck_hunt;

-- Additional parsing utilities
INSTALL parser_tools FROM community;
LOAD parser_tools;

-- PRQL query language
INSTALL prql FROM community;
LOAD prql;

-- Extended hash functions
INSTALL hashfuncs FROM community;
LOAD hashfuncs;

-- Cryptographic functions
INSTALL crypto FROM community;
LOAD crypto;

-- Fake data generation
INSTALL fakeit FROM community;
LOAD fakeit;

.print '[init-mcp-db] Developer extensions loaded'

-- ============================================
-- Helper Macros
-- ============================================

-- Parse SQL and count node types
CREATE OR REPLACE MACRO sql_complexity(query) AS (
    SELECT count(DISTINCT node_type) as node_types, count(*) as total_nodes
    FROM parse_sql(query)
);

-- Get all table references from SQL
CREATE OR REPLACE MACRO sql_tables(query) AS (
    SELECT DISTINCT table_name
    FROM parse_sql(query)
    WHERE node_type = 'table_ref'
);

-- Generate test user record
CREATE OR REPLACE MACRO fake_user() AS (
    SELECT
        fake_uuid() as id,
        fake_first_name() as first_name,
        fake_last_name() as last_name,
        fake_email() as email,
        fake_phone() as phone,
        fake_city() as city,
        fake_country() as country
);

-- File checksum helper
CREATE OR REPLACE MACRO file_checksum(file_path) AS (
    sha256(read_blob(file_path))
);

.print '[init-mcp-db] Helper macros created'

-- ============================================
-- Sample Tables for Testing
-- ============================================

-- Test users table
CREATE TABLE IF NOT EXISTS test_users AS
SELECT
    fake_uuid() as id,
    fake_first_name() as first_name,
    fake_last_name() as last_name,
    fake_email() as email,
    (random() * 50 + 18)::INTEGER as age
FROM range(10);

.print '[init-mcp-db] Sample tables created'
