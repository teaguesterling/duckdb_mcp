#!/usr/bin/env python3
import json
import subprocess
import tempfile
import os
import signal
import time

def test_basic():
    # Create minimal setup script
    setup_script = """LOAD 'duckdb_mcp';
SELECT mcp_server_start('stdio', 'localhost', 8080, '{}') AS result;
"""

    with tempfile.NamedTemporaryFile(mode='w', suffix='.sql', delete=False) as f:
        f.write(setup_script)
        setup_file = f.name

    try:
        env = os.environ.copy()
        env['DUCKDB_MCP_FOREGROUND'] = '1'
        
        print("Starting server...")
        process = subprocess.Popen(
            ["build/release/duckdb", "-init", setup_file],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env
        )
        
        # Send initialize message
        init_msg = {"jsonrpc": "2.0", "method": "initialize", "id": 1, "params": {}}
        process.stdin.write(json.dumps(init_msg) + "\n")
        process.stdin.flush()
        
        # Try to read the initialize response first
        try:
            line = process.stdout.readline()
            print(f"Initialize response: {line.strip()}")
        except:
            print("Failed to read initialize response")
        
        # Send shutdown
        shutdown_msg = {"jsonrpc": "2.0", "method": "shutdown", "id": 2, "params": {}}
        process.stdin.write(json.dumps(shutdown_msg) + "\n")
        process.stdin.flush()
        
        # Try to read shutdown response
        try:
            line = process.stdout.readline()
            print(f"Shutdown response: {line.strip()}")
        except:
            print("Failed to read shutdown response")
        
        # Wait for process to exit
        try:
            stdout, stderr = process.communicate(timeout=3)
            print(f"Exit code: {process.returncode}")
            print(f"Stdout: {stdout}")
            print(f"Stderr: {stderr}")
        except subprocess.TimeoutExpired:
            print("Process didn't exit, killing...")
            process.kill()
            stdout, stderr = process.communicate()
            print(f"Stdout: {stdout}")
            print(f"Stderr: {stderr}")
            
    finally:
        if process.poll() is None:
            process.terminate()
        os.unlink(setup_file)

if __name__ == "__main__":
    test_basic()