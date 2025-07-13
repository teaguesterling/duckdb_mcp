#!/usr/bin/env python3
"""
Simple test for DuckDB MCP server - start server and immediately test MCP communication
"""
import json
import sys
import subprocess
import time
import tempfile
import os

def test_mcp_server_direct():
    """Test MCP server by starting it and communicating directly"""
    print("Testing DuckDB MCP Server (direct mode)...")
    
    # Create a temporary script to set up data and start server
    setup_sql = """
CREATE TABLE test_data (id INT, name VARCHAR, value FLOAT);
INSERT INTO test_data VALUES (1, 'Alice', 100.5), (2, 'Bob', 200.7), (3, 'Charlie', 300.2);
SELECT mcp_server_start('stdio', 'localhost', 8080, '{}');
"""
    
    with tempfile.NamedTemporaryFile(mode='w', suffix='.sql', delete=False) as f:
        f.write(setup_sql)
        sql_file = f.name
    
    try:
        # Start DuckDB with the setup script
        cmd = ["./build/release/duckdb", "-init", sql_file]
        
        print(f"Starting DuckDB with command: {' '.join(cmd)}")
        process = subprocess.Popen(
            cmd, 
            stdin=subprocess.PIPE, 
            stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE,
            text=True, 
            bufsize=0
        )
        
        # Give it time to start
        time.sleep(2)
        
        # Send MCP initialize request
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
        
        print("Sending initialize request...")
        process.stdin.write(json.dumps(init_request) + "\n")
        process.stdin.flush()
        
        # Try to read response
        print("Waiting for response...")
        response = process.stdout.readline()
        print(f"Raw response: {repr(response)}")
        
        if response.strip():
            try:
                parsed = json.loads(response.strip())
                print(f"Parsed response: {parsed}")
            except json.JSONDecodeError as e:
                print(f"Failed to parse as JSON: {e}")
                print(f"Response content: {response}")
        else:
            print("No response received")
            
        # Check stderr for any errors
        stderr_data = process.stderr.read()
        if stderr_data:
            print(f"Stderr: {stderr_data}")
            
    except Exception as e:
        print(f"Test failed: {e}")
    finally:
        try:
            process.terminate()
            process.wait(timeout=5)
        except:
            process.kill()
        
        # Clean up temp file
        os.unlink(sql_file)

if __name__ == "__main__":
    test_mcp_server_direct()