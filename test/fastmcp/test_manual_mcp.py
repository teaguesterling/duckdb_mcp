#!/usr/bin/env python3
"""
Manual test of MCP server communication
"""

import subprocess
import json
import sys
import os


def test_mcp_server():
    # Path to our MCP server
    server_path = os.path.join(os.path.dirname(__file__), "sample_data_server.py")
    venv_python = os.path.join(os.path.dirname(__file__), "../../venv/bin/python")

    print(f"Testing MCP server: {server_path}")
    print(f"Using Python: {venv_python}")

    # Start the MCP server
    process = subprocess.Popen(
        [venv_python, server_path], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )

    # Send initialize request
    init_request = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "protocolVersion": "2024-11-05",
            "clientInfo": {"name": "DuckDB MCP Test", "version": "0.1.0"},
            "capabilities": {"resources": {}},
        },
    }

    print("Sending initialize request...")
    print(json.dumps(init_request, indent=2))

    try:
        # Send the request
        stdout, stderr = process.communicate(input=json.dumps(init_request) + '\n', timeout=5)

        print("\n--- STDOUT ---")
        print(stdout)
        print("\n--- STDERR ---")
        print(stderr)

        if stdout:
            try:
                response = json.loads(stdout.strip())
                print("\n--- PARSED RESPONSE ---")
                print(json.dumps(response, indent=2))
            except json.JSONDecodeError as e:
                print(f"Failed to parse JSON response: {e}")

    except subprocess.TimeoutExpired:
        print("Request timed out!")
        process.kill()
        stdout, stderr = process.communicate()
        print(f"STDOUT: {stdout}")
        print(f"STDERR: {stderr}")

    finally:
        if process.poll() is None:
            process.terminate()


if __name__ == "__main__":
    test_mcp_server()
