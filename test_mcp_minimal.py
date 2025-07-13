#!/usr/bin/env python3
"""
Minimal test - just start the MCP server without any other SQL commands
"""
import json
import subprocess
import time

def test_minimal_mcp_server():
    """Test minimal MCP server startup"""
    print("Testing minimal MCP server...")
    
    # Just start the server, no other commands
    cmd = ["./build/release/duckdb", "-c", "SELECT mcp_server_start('stdio', 'localhost', 8080, '{}');"]
    
    try:
        print("Starting DuckDB MCP server...")
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
        
        # Send MCP initialize
        init_request = {
            "jsonrpc": "2.0",
            "method": "initialize",
            "id": 1,
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "test", "version": "1.0.0"}
            }
        }
        
        print("Sending initialize...")
        process.stdin.write(json.dumps(init_request) + "\n")
        process.stdin.flush()
        
        # Try to read multiple lines to see what we get
        print("Reading responses...")
        for i in range(5):
            try:
                line = process.stdout.readline()
                print(f"Line {i}: {repr(line)}")
                if line.strip() and line.strip().startswith('{'):
                    # Try to parse as JSON
                    try:
                        parsed = json.loads(line.strip())
                        print(f"✓ Got JSON: {parsed}")
                        return True
                    except:
                        pass
            except:
                break
                
        stderr_output = process.stderr.read()
        if stderr_output:
            print(f"Stderr: {stderr_output}")
            
    except Exception as e:
        print(f"Error: {e}")
    finally:
        try:
            process.terminate()
            process.wait(timeout=2)
        except:
            process.kill()
            
    return False

if __name__ == "__main__":
    test_minimal_mcp_server()