#!/usr/bin/env python3
"""Test listing resources from MCP server"""

import subprocess
import json
import sys
import os


def test_list_resources():
    server_path = os.path.join(os.path.dirname(__file__), "sample_data_server.py")
    venv_python = os.path.join(os.path.dirname(__file__), "../../venv/bin/python")

    process = subprocess.Popen(
        [venv_python, server_path], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )

    # Initialize first
    init_request = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "protocolVersion": "2024-11-05",
            "clientInfo": {"name": "Test", "version": "1.0"},
            "capabilities": {"resources": {}},
        },
    }

    # List resources request
    list_request = {"jsonrpc": "2.0", "id": 2, "method": "resources/list", "params": {}}

    requests = json.dumps(init_request) + '\n' + json.dumps(list_request) + '\n'

    try:
        stdout, stderr = process.communicate(input=requests, timeout=5)

        print("--- STDOUT ---")
        print(stdout)
        print("--- STDERR ---")
        print(stderr)

        # Parse responses
        lines = stdout.strip().split('\n')
        for i, line in enumerate(lines):
            if line:
                try:
                    response = json.loads(line)
                    print(f"\n--- Response {i+1} ---")
                    print(json.dumps(response, indent=2))
                except json.JSONDecodeError as e:
                    print(f"Failed to parse line {i+1}: {e}")

    except subprocess.TimeoutExpired:
        print("Request timed out!")
        process.kill()

    finally:
        if process.poll() is None:
            process.terminate()


if __name__ == "__main__":
    test_list_resources()
