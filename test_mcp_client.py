#!/usr/bin/env python3
"""
Simple MCP client to test DuckDB MCP server functionality
"""
import json
import sys
import subprocess
import time

def send_mcp_request(process, method, params=None, request_id=1):
    """Send an MCP JSON-RPC request"""
    request = {
        "jsonrpc": "2.0",
        "method": method,
        "id": request_id
    }
    if params:
        request["params"] = params
    
    json_str = json.dumps(request)
    print(f"-> {json_str}", file=sys.stderr)
    
    process.stdin.write(json_str + "\n")
    process.stdin.flush()
    
    # Read response
    response_line = process.stdout.readline()
    if response_line:
        response_line = response_line.strip()
        print(f"<- {response_line}", file=sys.stderr)
        try:
            return json.loads(response_line)
        except json.JSONDecodeError as e:
            print(f"Failed to parse response: {e}", file=sys.stderr)
            return None
    return None

def test_mcp_server():
    """Test basic MCP server functionality"""
    print("Testing DuckDB MCP Server...")
    
    # Start DuckDB with MCP server - use .mode to suppress output
    cmd = [
        "./build/release/duckdb", 
        "-c", 
        """
        .mode json
        CREATE TABLE test_data (id INT, name VARCHAR, value FLOAT);
        INSERT INTO test_data VALUES (1, 'Alice', 100.5), (2, 'Bob', 200.7), (3, 'Charlie', 300.2);
        SELECT mcp_server_start('stdio', 'localhost', 8080, '{}');
        SELECT mcp_publish_table('test_data', 'data://tables/test_data', 'json');
        """
    ]
    
    try:
        # Start the process
        process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, 
                                 stderr=subprocess.PIPE, text=True, bufsize=0)
        
        time.sleep(1)  # Give server time to start
        
        # Test 1: Initialize
        print("\n1. Testing initialize...")
        response = send_mcp_request(process, "initialize", {
            "protocolVersion": "2024-11-05",
            "capabilities": {
                "resources": {"subscribe": False}
            },
            "clientInfo": {
                "name": "test-client",
                "version": "1.0.0"
            }
        })
        
        if response and "result" in response:
            print("✓ Initialize successful")
        else:
            print("✗ Initialize failed")
            
        # Test 2: List resources
        print("\n2. Testing list resources...")
        response = send_mcp_request(process, "resources/list", {}, 2)
        
        if response and "result" in response:
            print("✓ List resources successful")
            print(f"Resources: {response['result']}")
        else:
            print("✗ List resources failed")
            
        # Test 3: Read resource
        print("\n3. Testing read resource...")
        response = send_mcp_request(process, "resources/read", {
            "uri": "data://tables/test_data"
        }, 3)
        
        if response and "result" in response:
            print("✓ Read resource successful")
            print(f"Content: {response['result']}")
        else:
            print("✗ Read resource failed")
            
        # Test 4: List tools
        print("\n4. Testing list tools...")
        response = send_mcp_request(process, "tools/list", {}, 4)
        
        if response and "result" in response:
            print("✓ List tools successful")
            print(f"Tools: {response['result']}")
        else:
            print("✗ List tools failed")
            
        # Test 5: Call query tool
        print("\n5. Testing call query tool...")
        response = send_mcp_request(process, "tools/call", {
            "name": "query",
            "arguments": {
                "sql": "SELECT COUNT(*) as total FROM test_data",
                "format": "json"
            }
        }, 5)
        
        if response and "result" in response:
            print("✓ Call tool successful")
            print(f"Result: {response['result']}")
        else:
            print("✗ Call tool failed")
            
    except Exception as e:
        print(f"Test failed with error: {e}")
    finally:
        try:
            process.terminate()
            process.wait(timeout=5)
        except:
            process.kill()

if __name__ == "__main__":
    test_mcp_server()