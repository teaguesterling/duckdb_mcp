#!/usr/bin/env python3
"""
Simple test to verify DuckDB MCP server responds to JSON-RPC messages
"""
import json
import subprocess
import tempfile
import os

def test_simple_server():
    """Test basic server communication"""
    
    # Create minimal setup script
    setup_script = """
LOAD 'duckdb_mcp';
CREATE TABLE test (id INT, name VARCHAR);
INSERT INTO test VALUES (1, 'Alice'), (2, 'Bob');
SELECT mcp_server_start('stdio', 'localhost', 8080, '{}') AS result;
"""

    with tempfile.NamedTemporaryFile(mode='w', suffix='.sql', delete=False) as f:
        f.write(setup_script)
        setup_file = f.name

    try:
        # Start server in foreground mode
        env = os.environ.copy()
        env['DUCKDB_MCP_FOREGROUND'] = '1'
        
        print("🚀 Starting DuckDB MCP server...")
        process = subprocess.Popen(
            ["build/release/duckdb", "-init", setup_file],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env
        )
        
        # Send a simple initialize message
        print("📨 Sending initialize message...")
        init_msg = {
            "jsonrpc": "2.0",
            "method": "initialize",
            "id": 1,
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "test", "version": "1.0"}
            }
        }
        
        process.stdin.write(json.dumps(init_msg) + "\n")
        process.stdin.flush()
        
        # Try to read response (with timeout)
        import select
        import sys
        
        print("⏳ Waiting for response...")
        
        # Check if there's data to read
        ready, _, _ = select.select([process.stdout], [], [], 5.0)  # 5 second timeout
        
        if ready:
            response = process.stdout.readline()
            print(f"✅ Got response: {response.strip()}")
            
            # Try to parse as JSON
            try:
                parsed = json.loads(response.strip())
                print(f"✅ Valid JSON response: {parsed}")
                
                # Send shutdown message
                print("📨 Sending shutdown message...")
                shutdown_msg = {
                    "jsonrpc": "2.0",
                    "method": "shutdown",
                    "id": 2,
                    "params": {}
                }
                
                process.stdin.write(json.dumps(shutdown_msg) + "\n")
                process.stdin.flush()
                
                # Read shutdown response
                ready, _, _ = select.select([process.stdout], [], [], 2.0)
                if ready:
                    shutdown_response = process.stdout.readline()
                    print(f"✅ Shutdown response: {shutdown_response.strip()}")
                
            except json.JSONDecodeError:
                print(f"❌ Invalid JSON response")
        else:
            print("⏰ Timeout waiting for response")
        
        # Check stderr for any error messages
        stderr_data = process.stderr.read()
        if stderr_data:
            print(f"📝 Stderr: {stderr_data}")
            
    except Exception as e:
        print(f"❌ Error: {e}")
        
    finally:
        process.terminate()
        process.wait()
        os.unlink(setup_file)

if __name__ == "__main__":
    test_simple_server()