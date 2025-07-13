#!/usr/bin/env python3
"""Test full MCP protocol flow"""

import subprocess
import json
import sys
import os

def test_full_protocol():
    server_path = os.path.join(os.path.dirname(__file__), "sample_data_server.py")
    venv_python = os.path.join(os.path.dirname(__file__), "../../venv/bin/python")
    
    process = subprocess.Popen([venv_python, server_path], 
                              stdin=subprocess.PIPE, 
                              stdout=subprocess.PIPE, 
                              stderr=subprocess.PIPE,
                              text=True)
    
    # 1. Initialize
    init_request = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "protocolVersion": "2024-11-05",
            "clientInfo": {"name": "Test", "version": "1.0"},
            "capabilities": {"resources": {}}
        }
    }
    
    # 2. Initialized notification (no response expected)
    initialized_notification = {
        "jsonrpc": "2.0",
        "method": "notifications/initialized",
        "params": {}
    }
    
    # 3. List resources
    list_request = {
        "jsonrpc": "2.0",
        "id": 2,
        "method": "resources/list",
        "params": {}
    }
    
    # 4. Read a resource
    read_request = {
        "jsonrpc": "2.0",
        "id": 3,
        "method": "resources/read",
        "params": {"uri": "file:///customers.csv"}
    }
    
    requests = (json.dumps(init_request) + '\n' + 
                json.dumps(initialized_notification) + '\n' +
                json.dumps(list_request) + '\n' +
                json.dumps(read_request) + '\n')
    
    try:
        stdout, stderr = process.communicate(input=requests, timeout=5)
        
        print("--- STDOUT ---")
        print(stdout)
        print("--- STDERR ---") 
        print(stderr)
        
        # Parse responses
        lines = [line.strip() for line in stdout.strip().split('\n') if line.strip()]
        for i, line in enumerate(lines):
            try:
                response = json.loads(line)
                print(f"\n--- Response {i+1} ---")
                print(json.dumps(response, indent=2))
            except json.JSONDecodeError as e:
                print(f"Failed to parse line {i+1}: {line[:100]}... Error: {e}")
        
    except subprocess.TimeoutExpired:
        print("Request timed out!")
        process.kill()
    
    finally:
        if process.poll() is None:
            process.terminate()

if __name__ == "__main__":
    test_full_protocol()