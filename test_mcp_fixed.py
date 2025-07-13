#!/usr/bin/env python3
"""
Test the fixed MCP server that should take over the process completely
"""
import json
import subprocess
import time

def test_fixed_mcp_server():
    """Test that mcp_server_start takes over the process for stdio transport"""
    print("Testing fixed MCP server...")
    
    # SQL commands to set up data and start server
    setup_commands = """
CREATE TABLE test_data (id INT, name VARCHAR, value FLOAT);
INSERT INTO test_data VALUES (1, 'Alice', 100.5), (2, 'Bob', 200.7);
SELECT mcp_publish_table('test_data', 'data://tables/test_data', 'json');
SELECT mcp_server_start('stdio', 'localhost', 8080, '{}');
"""
    
    try:
        # Start DuckDB with our setup commands
        print("Starting DuckDB with MCP server...")
        process = subprocess.Popen(
            ["./build/release/duckdb", "-c", setup_commands],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=0
        )
        
        # Give the server time to start and take over
        time.sleep(1)
        
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
        
        print("Sending MCP initialize request...")
        process.stdin.write(json.dumps(init_request) + "\n")
        process.stdin.flush()
        
        # Read response
        print("Reading response...")
        response = process.stdout.readline()
        print(f"Raw response: {repr(response)}")
        
        if response.strip():
            try:
                parsed = json.loads(response.strip())
                print(f"✓ Got valid JSON response: {parsed}")
                
                # If we got this far, the server is working properly
                if "result" in parsed:
                    print("✓ MCP server is responding correctly!")
                    return True
                else:
                    print(f"✗ Unexpected response format: {parsed}")
                    
            except json.JSONDecodeError as e:
                print(f"✗ Invalid JSON response: {e}")
                print(f"Response: {response}")
        else:
            print("✗ No response received")
            
        # Check stderr for any startup messages
        stderr_output = process.stderr.read()
        if stderr_output:
            print(f"Stderr: {stderr_output}")
            
    except Exception as e:
        print(f"Test failed with error: {e}")
        return False
    finally:
        try:
            process.terminate()
            process.wait(timeout=3)
        except:
            process.kill()
            
    return False

if __name__ == "__main__":
    success = test_fixed_mcp_server()
    print(f"\nTest result: {'SUCCESS' if success else 'FAILED'}")