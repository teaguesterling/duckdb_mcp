#!/usr/bin/env python3
"""
Test both background and foreground modes for MCP server
"""
import json
import subprocess
import time
import os

def test_background_mode():
    """Test default background mode (should return to SQL prompt)"""
    print("=== Testing Background Mode (Default) ===")
    
    try:
        # Start DuckDB and test background mode
        process = subprocess.Popen(
            ["./build/release/duckdb", "-c", "SELECT mcp_server_start('stdio', 'localhost', 8080, '{}');"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=0
        )
        
        # Should get SQL result output and process should continue
        output_lines = []
        for i in range(8):  # Read multiple lines to capture table output
            line = process.stdout.readline()
            if not line:
                break
            output_lines.append(line.strip())
            print(f"Line {i}: {repr(line.strip())}")
            
        # Look for success message
        success_found = any("SUCCESS: MCP server started on stdio (background mode)" in line for line in output_lines)
        if success_found:
            print("✓ Background mode working - got success message")
            return True
        else:
            print("✗ Background mode failed - no success message found")
            
    except Exception as e:
        print(f"Error: {e}")
    finally:
        try:
            process.terminate()
            process.wait(timeout=2)
        except:
            process.kill()
            
    return False

def test_foreground_mode():
    """Test foreground mode with DUCKDB_MCP_FOREGROUND=1"""
    print("\n=== Testing Foreground Mode (DUCKDB_MCP_FOREGROUND=1) ===")
    
    # Set environment variable
    env = os.environ.copy()
    env['DUCKDB_MCP_FOREGROUND'] = '1'
    
    try:
        # Start DuckDB with foreground mode
        process = subprocess.Popen(
            ["./build/release/duckdb", "-c", "SELECT mcp_server_start('stdio', 'localhost', 8080, '{}');"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=0,
            env=env
        )
        
        # Give it time to start up
        time.sleep(1)
        
        # Send MCP initialize request
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
        
        print("Sending MCP initialize request...")
        process.stdin.write(json.dumps(init_request) + "\n")
        process.stdin.flush()
        
        # Try to read responses
        print("Reading responses...")
        for i in range(10):
            try:
                line = process.stdout.readline()
                if not line:
                    break
                    
                line = line.strip()
                print(f"Response {i}: {repr(line)}")
                
                # Check if it looks like JSON
                if line and line.startswith('{') and line.endswith('}'):
                    try:
                        parsed = json.loads(line)
                        print(f"✓ Got valid MCP JSON response: {parsed}")
                        if "result" in parsed:
                            print("✓ Foreground mode working - MCP server responding!")
                            return True
                    except json.JSONDecodeError:
                        continue
                        
            except Exception as e:
                print(f"Read error: {e}")
                break
                
        print("✗ Foreground mode failed - no valid MCP responses")
        
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
    print("Testing DuckDB MCP Server Dual Mode Implementation")
    print("=" * 60)
    
    background_success = test_background_mode()
    foreground_success = test_foreground_mode()
    
    print("\n" + "=" * 60)
    print("RESULTS:")
    print(f"Background Mode: {'✓ PASS' if background_success else '✗ FAIL'}")
    print(f"Foreground Mode: {'✓ PASS' if foreground_success else '✗ FAIL'}")
    print(f"Overall: {'✓ SUCCESS' if background_success and foreground_success else '✗ NEEDS WORK'}")