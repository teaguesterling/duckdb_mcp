#!/usr/bin/env python3
"""
Test the new structured ATTACH syntax for MCP connections
"""
import subprocess
import tempfile
import os
import time
import signal
from pathlib import Path

def test_structured_attach():
    """Test the new structured ATTACH syntax"""
    
    print("🚀 Testing JSON-Based ATTACH Syntax")
    print("=" * 50)
    
    # Test 1: Basic structured syntax with JSON
    print("\n1️⃣ Testing JSON-based structured syntax...")
    
    sql_script = '''
LOAD 'duckdb_mcp';
SET allowed_mcp_commands='/usr/bin/python3,python3';

-- Test JSON-based structured ATTACH syntax
ATTACH 'python3' AS test_server (
    TYPE mcp,
    TRANSPORT 'stdio',
    ARGS '["test/fastmcp/sample_data_server.py"]',
    CWD '/mnt/aux-data/teague/Projects/duckdb_mcp',
    ENV '{"PYTHONPATH": "/usr/local/lib/python3.8/site-packages", "MCP_DEBUG": "1"}'
);

-- Verify connection
SHOW DATABASES;

-- List resources
SELECT mcp_list_resources('test_server') AS resources;
'''
    
    try:
        result = subprocess.run(
            ['./build/release/duckdb', '-c', sql_script],
            capture_output=True,
            text=True,
            timeout=30
        )
        
        print(f"Exit code: {result.returncode}")
        print(f"Output: {result.stdout}")
        if result.stderr:
            print(f"Errors: {result.stderr}")
            
        if result.returncode == 0:
            print("✅ Structured syntax test PASSED")
        else:
            print("❌ Structured syntax test FAILED")
            
    except subprocess.TimeoutExpired:
        print("⏰ Structured syntax test TIMED OUT")
    except Exception as e:
        print(f"❌ Structured syntax test ERROR: {e}")
    
    # Test 2: Config file syntax
    print("\n2️⃣ Testing .mcp.json config file syntax...")
    
    sql_script_config = '''
LOAD 'duckdb_mcp';
SET allowed_mcp_commands='/usr/bin/python3,python3';

-- Test config file ATTACH syntax
ATTACH 'sample_data' AS config_server (
    TYPE mcp,
    FROM_CONFIG_FILE 'test/.mcp.json'
);

-- Verify connection
SHOW DATABASES;

-- List resources
SELECT mcp_list_resources('config_server') AS resources;
'''
    
    try:
        result = subprocess.run(
            ['./build/release/duckdb', '-c', sql_script_config],
            capture_output=True,
            text=True,
            timeout=30
        )
        
        print(f"Exit code: {result.returncode}")
        print(f"Output: {result.stdout}")
        if result.stderr:
            print(f"Errors: {result.stderr}")
            
        if result.returncode == 0:
            print("✅ Config file syntax test PASSED")
        else:
            print("❌ Config file syntax test FAILED")
            
    except subprocess.TimeoutExpired:
        print("⏰ Config file syntax test TIMED OUT")
    except Exception as e:
        print(f"❌ Config file syntax test ERROR: {e}")
    
    # Test 3: Simple parameter fallback
    print("\n3️⃣ Testing simple parameter fallback...")
    
    sql_script_simple = '''
LOAD 'duckdb_mcp';
SET allowed_mcp_commands='/usr/bin/python3,python3';

-- Test simple string parameters (fallback mode)
ATTACH 'python3' AS simple_server (
    TYPE mcp,
    TRANSPORT 'stdio',
    ARGS 'test/fastmcp/sample_data_server.py'
);

-- Verify connection
SHOW DATABASES;

-- List resources
SELECT mcp_list_resources('simple_server') AS resources;
'''
    
    try:
        result = subprocess.run(
            ['./build/release/duckdb', '-c', sql_script_simple],
            capture_output=True,
            text=True,
            timeout=30
        )
        
        print(f"Exit code: {result.returncode}")
        print(f"Output: {result.stdout}")
        if result.stderr:
            print(f"Errors: {result.stderr}")
            
        if result.returncode == 0:
            print("✅ Simple parameter test PASSED")
        else:
            print("❌ Simple parameter test FAILED")
            
    except subprocess.TimeoutExpired:
        print("⏰ Simple parameter test TIMED OUT")
    except Exception as e:
        print(f"❌ Simple parameter test ERROR: {e}")
    
    # Test 4: Resource access with new syntax
    print("\n4️⃣ Testing resource access with JSON syntax...")
    
    sql_script_resources = '''
LOAD 'duckdb_mcp';
SET allowed_mcp_commands='/usr/bin/python3,python3';

-- Connect with JSON-based structured syntax
ATTACH 'python3' AS resource_server (
    TYPE mcp,
    TRANSPORT 'stdio',
    ARGS '["test/fastmcp/sample_data_server.py"]',
    ENV '{"MCP_DEBUG": "1"}'
);

-- Test resource access
SELECT COUNT(*) as customer_count FROM read_csv('mcp://resource_server/file:///customers.csv');
SELECT mcp_call_tool('resource_server', 'get_data_info', '{"dataset": "customers"}') AS tool_result;
'''
    
    try:
        result = subprocess.run(
            ['./build/release/duckdb', '-c', sql_script_resources],
            capture_output=True,
            text=True,
            timeout=30
        )
        
        print(f"Exit code: {result.returncode}")
        print(f"Output: {result.stdout}")
        if result.stderr:
            print(f"Errors: {result.stderr}")
            
        if result.returncode == 0:
            print("✅ Resource access test PASSED")
        else:
            print("❌ Resource access test FAILED")
            
    except subprocess.TimeoutExpired:
        print("⏰ Resource access test TIMED OUT")
    except Exception as e:
        print(f"❌ Resource access test ERROR: {e}")
    
    print("\n" + "=" * 50)
    print("🎯 JSON-Based ATTACH Syntax Testing Complete!")

if __name__ == "__main__":
    test_structured_attach()