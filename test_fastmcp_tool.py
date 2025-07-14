#!/usr/bin/env python3
"""
Quick test to trigger the FastMCP validation error
"""
import json
import subprocess
import time
import os

def test_fastmcp_tool():
    """Test calling a tool to see the validation error"""
    
    # Start the FastMCP server
    env = os.environ.copy()
    server_path = "test/fastmcp/sample_data_server.py"
    venv_path = "venv"
    
    cmd = [f"{venv_path}/bin/python", server_path]
    
    process = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=0
    )
    
    try:
        time.sleep(1)  # Give server time to start
        
        # Initialize
        init_request = {
            "jsonrpc": "2.0",
            "method": "initialize",
            "id": 1,
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {"tools": {"list_changed": False}},
                "clientInfo": {"name": "test-client", "version": "1.0.0"}
            }
        }
        
        process.stdin.write(json.dumps(init_request) + "\n")
        process.stdin.flush()
        
        response = process.stdout.readline()
        print(f"Initialize response: {response.strip()}")
        
        # Call tool to trigger validation error
        tool_request = {
            "jsonrpc": "2.0", 
            "method": "tools/call",
            "id": 2,
            "params": {
                "name": "get_data_info",
                "arguments": {"dataset": "customers.csv"}
            }
        }
        
        process.stdin.write(json.dumps(tool_request) + "\n")
        process.stdin.flush()
        
        response = process.stdout.readline()
        print(f"Tool call response: {response.strip()}")
        
        # Check stderr for validation errors
        stderr_output = process.stderr.read()
        if stderr_output:
            print(f"Stderr: {stderr_output}")
            
    finally:
        process.terminate()
        process.wait()

if __name__ == "__main__":
    test_fastmcp_tool()