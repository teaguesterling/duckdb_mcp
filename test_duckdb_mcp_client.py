#!/usr/bin/env python3
"""
Test client for DuckDB MCP server
"""
import json
import subprocess
import time
import tempfile
import os

def test_duckdb_mcp_server():
    """Test connecting to a DuckDB MCP server"""
    print("Testing DuckDB MCP Server as client...")
    
    # Create setup script for DuckDB server
    setup_script = """
LOAD 'duckdb_mcp';

CREATE TABLE test_customers (id INT, name VARCHAR, city VARCHAR, spent DECIMAL(10,2));
INSERT INTO test_customers VALUES 
  (1, 'Alice', 'NYC', 1200.50),
  (2, 'Bob', 'Chicago', 850.75),
  (3, 'Carol', 'SF', 2100.25);

SELECT mcp_server_start('stdio', 'localhost', 8080, '{}') AS server_start;
SELECT mcp_publish_table('test_customers', 'data://tables/customers', 'json') AS publish_result;
"""

    with tempfile.NamedTemporaryFile(mode='w', suffix='.sql', delete=False) as f:
        f.write(setup_script)
        setup_file = f.name

    try:
        # Start DuckDB MCP server in foreground mode
        env = os.environ.copy()
        env['DUCKDB_MCP_FOREGROUND'] = '1'
        
        print("Starting DuckDB MCP server...")
        cmd = [
            "build/release/duckdb", 
            "-init", setup_file
        ]
        
        process = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env,
            bufsize=0
        )
        
        time.sleep(2)  # Give server time to start
        
        # Test 1: Initialize
        print("\n1. Testing initialize...")
        init_request = {
            "jsonrpc": "2.0",
            "method": "initialize",
            "id": 1,
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {"resources": {"subscribe": False}},
                "clientInfo": {"name": "test-client", "version": "1.0.0"}
            }
        }
        
        process.stdin.write(json.dumps(init_request) + "\n")
        process.stdin.flush()
        
        response = process.stdout.readline()
        if response:
            try:
                parsed = json.loads(response.strip())
                print(f"✓ Initialize: {parsed.get('result', {}).get('serverInfo', {}).get('name')}")
            except:
                print(f"✗ Initialize failed: {response}")
        
        # Check stderr for any errors
        stderr_output = process.stderr.read()
        if stderr_output:
            print(f"Server stderr: {stderr_output}")
        
        # Test 2: List resources
        print("\n2. Testing list resources...")
        list_request = {
            "jsonrpc": "2.0",
            "method": "resources/list", 
            "id": 2
        }
        
        process.stdin.write(json.dumps(list_request) + "\n")
        process.stdin.flush()
        
        response = process.stdout.readline()
        if response:
            try:
                parsed = json.loads(response.strip())
                resources = parsed.get('result', {}).get('resources', [])
                print(f"✓ Found {len(resources)} resources:")
                for r in resources:
                    print(f"  - {r.get('uri')}: {r.get('description')}")
            except:
                print(f"✗ List resources failed: {response}")
        
        # Test 3: Read resource
        print("\n3. Testing read resource...")
        read_request = {
            "jsonrpc": "2.0",
            "method": "resources/read",
            "id": 3,
            "params": {"uri": "data://tables/customers"}
        }
        
        process.stdin.write(json.dumps(read_request) + "\n")
        process.stdin.flush()
        
        response = process.stdout.readline()
        if response:
            try:
                parsed = json.loads(response.strip())
                contents = parsed.get('result', {}).get('contents', [])
                if contents:
                    print(f"✓ Resource content received ({len(contents[0].get('text', ''))} chars)")
                    print(f"  Preview: {contents[0].get('text', '')[:100]}...")
                else:
                    print("✗ No resource content")
            except:
                print(f"✗ Read resource failed: {response}")
        
        # Test 4: List tools
        print("\n4. Testing list tools...")
        tools_request = {
            "jsonrpc": "2.0",
            "method": "tools/list",
            "id": 4
        }
        
        process.stdin.write(json.dumps(tools_request) + "\n")
        process.stdin.flush()
        
        response = process.stdout.readline()
        if response:
            try:
                parsed = json.loads(response.strip())
                tools = parsed.get('result', {}).get('tools', [])
                print(f"✓ Found {len(tools)} tools:")
                for t in tools:
                    print(f"  - {t.get('name')}: {t.get('description')}")
            except:
                print(f"✗ List tools failed: {response}")
        
        # Test 5: Call query tool
        print("\n5. Testing query tool...")
        query_request = {
            "jsonrpc": "2.0",
            "method": "tools/call",
            "id": 5,
            "params": {
                "name": "query",
                "arguments": {
                    "sql": "SELECT COUNT(*) as total FROM test_customers",
                    "format": "json"
                }
            }
        }
        
        process.stdin.write(json.dumps(query_request) + "\n")
        process.stdin.flush()
        
        response = process.stdout.readline()
        if response:
            try:
                parsed = json.loads(response.strip())
                content = parsed.get('result', {}).get('content', [])
                if content:
                    print(f"✓ Query result: {content[0].get('text', '')[:100]}...")
                else:
                    print("✗ No query result")
            except:
                print(f"✗ Query tool failed: {response}")
                
    except Exception as e:
        print(f"Error: {e}")
    finally:
        try:
            process.terminate()
            process.wait(timeout=3)
        except:
            process.kill()
        
        # Cleanup
        os.unlink(setup_file)

if __name__ == "__main__":
    test_duckdb_mcp_server()