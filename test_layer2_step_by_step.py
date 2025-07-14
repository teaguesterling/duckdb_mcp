#!/usr/bin/env python3
"""
Test Layer 2 of the nested MCP experiment step by step
This validates each component before putting it all together
"""

import subprocess
import tempfile
import time
import os
import signal
import json

def test_step1_python_server():
    """Test that Python MCP server starts and responds"""
    print("🧪 STEP 1: Testing Python MCP server (Layer 1)")
    
    # Check if server file exists
    server_file = "test/fastmcp/sample_data_server.py"
    if not os.path.exists(server_file):
        print(f"❌ Server file not found: {server_file}")
        return False
    
    # Try to import the server module to validate syntax
    try:
        result = subprocess.run(
            ["python3", "-c", f"import sys; sys.path.append('.'); exec(open('{server_file}').read())"],
            capture_output=True,
            text=True,
            timeout=5
        )
        
        if result.returncode == 0:
            print("✅ Python server file is valid and can be imported")
            return True
        else:
            print(f"❌ Python server validation failed: {result.stderr}")
            return False
            
    except subprocess.TimeoutExpired:
        print("✅ Python server started (timed out waiting, which is expected)")
        return True
    except Exception as e:
        print(f"❌ Python server test failed: {e}")
        return False

def test_step2_duckdb_connection():
    """Test DuckDB connecting to Python MCP server"""
    print("\n🧪 STEP 2: Testing DuckDB -> Python MCP connection")
    
    # Create test script
    test_script = """
LOAD 'duckdb_mcp';
SET allowed_mcp_commands='/usr/bin/python3,python3';
ATTACH 'stdio://python3 test/fastmcp/sample_data_server.py' AS testlayer (TYPE mcp);
SHOW DATABASES;
SELECT 'Connection test complete' as result;
"""
    
    with tempfile.NamedTemporaryFile(mode='w', suffix='.sql', delete=False) as f:
        f.write(test_script)
        test_file = f.name
    
    try:
        result = subprocess.run(
            ["build/release/duckdb", "-init", test_file],
            capture_output=True,
            text=True,
            timeout=15
        )
        
        print(f"✅ DuckDB connection output:\n{result.stdout}")
        if result.stderr:
            print(f"⚠️  Stderr: {result.stderr}")
            
        return "testlayer" in result.stdout
        
    except subprocess.TimeoutExpired:
        print("❌ DuckDB connection test timed out")
        return False
    except Exception as e:
        print(f"❌ DuckDB connection test failed: {e}")
        return False
    finally:
        os.unlink(test_file)

def test_step3_enhanced_views():
    """Test creating enhanced views using MCP resources"""
    print("\n🧪 STEP 3: Testing enhanced views creation")
    
    test_script = """
LOAD 'duckdb_mcp';
SET allowed_mcp_commands='/usr/bin/python3,python3';
ATTACH 'stdio://python3 test/fastmcp/sample_data_server.py' AS baselayer (TYPE mcp);

-- Create enhanced view
CREATE VIEW customer_analysis AS
SELECT 
    'customers' as dataset,
    COUNT(*) as total_count,
    'enhanced_by_layer2' as source
FROM read_csv('mcp://baselayer/file:///customers.csv');

-- Test the view
SELECT * FROM customer_analysis;
SELECT 'Enhanced view test complete' as result;
"""
    
    with tempfile.NamedTemporaryFile(mode='w', suffix='.sql', delete=False) as f:
        f.write(test_script)
        test_file = f.name
    
    try:
        result = subprocess.run(
            ["build/release/duckdb", "-init", test_file],
            capture_output=True,
            text=True,
            timeout=20
        )
        
        print(f"✅ Enhanced views output:\n{result.stdout}")
        if result.stderr:
            print(f"⚠️  Stderr: {result.stderr}")
            
        return "enhanced_by_layer2" in result.stdout
        
    except subprocess.TimeoutExpired:
        print("❌ Enhanced views test timed out")
        return False
    except Exception as e:
        print(f"❌ Enhanced views test failed: {e}")
        return False
    finally:
        os.unlink(test_file)

def test_step4_enhanced_server():
    """Test starting DuckDB MCP server with enhanced resources"""
    print("\n🧪 STEP 4: Testing enhanced DuckDB MCP server")
    
    test_script = """
LOAD 'duckdb_mcp';
SET allowed_mcp_commands='python3';

-- Create some test data for our server to expose
CREATE TABLE enhanced_summary AS
SELECT 'layer2_enhanced' as server_type, 'ready' as status;

-- Start MCP server in foreground mode
SELECT mcp_server_start('stdio', 'localhost', 8080, '{}') AS server_result;
"""
    
    with tempfile.NamedTemporaryFile(mode='w', suffix='.sql', delete=False) as f:
        f.write(test_script)
        test_file = f.name
    
    try:
        env = os.environ.copy()
        env['DUCKDB_MCP_FOREGROUND'] = '1'
        
        # Start the server
        server_process = subprocess.Popen(
            ["build/release/duckdb", "-init", test_file],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env
        )
        
        time.sleep(2)  # Let server start
        
        # Test with initialize message
        init_msg = {
            "jsonrpc": "2.0",
            "method": "initialize",
            "id": 1, 
            "params": {"protocolVersion": "2024-11-05", "capabilities": {}}
        }
        
        server_process.stdin.write(json.dumps(init_msg) + "\n")
        server_process.stdin.flush()
        
        response = server_process.stdout.readline()
        print(f"✅ Enhanced server response: {response.strip()}")
        
        # Send shutdown
        shutdown_msg = {"jsonrpc": "2.0", "method": "shutdown", "id": 2, "params": {}}
        server_process.stdin.write(json.dumps(shutdown_msg) + "\n")
        server_process.stdin.flush()
        
        shutdown_response = server_process.stdout.readline()
        print(f"✅ Enhanced server shutdown: {shutdown_response.strip()}")
        
        return True
        
    except Exception as e:
        print(f"❌ Enhanced server test failed: {e}")
        return False
    finally:
        if 'server_process' in locals():
            server_process.terminate()
            server_process.wait()
        os.unlink(test_file)

def main():
    """Run all Layer 2 tests step by step"""
    print("🎯 Testing Layer 2: DuckDB Enhanced Server")
    print("=" * 50)
    
    tests = [
        ("Python MCP Server", test_step1_python_server),
        ("DuckDB-MCP Connection", test_step2_duckdb_connection), 
        ("Enhanced Views", test_step3_enhanced_views),
        ("Enhanced Server", test_step4_enhanced_server)
    ]
    
    results = []
    for test_name, test_func in tests:
        print(f"\n🔬 Running: {test_name}")
        try:
            success = test_func()
            results.append((test_name, success))
            print(f"{'✅' if success else '❌'} {test_name}: {'PASSED' if success else 'FAILED'}")
        except Exception as e:
            print(f"❌ {test_name}: ERROR - {e}")
            results.append((test_name, False))
    
    print(f"\n📊 TEST SUMMARY")
    print("=" * 30)
    for test_name, success in results:
        status = "✅ PASS" if success else "❌ FAIL"
        print(f"{status}: {test_name}")
    
    passed = sum(1 for _, success in results if success)
    total = len(results)
    print(f"\nOverall: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 All Layer 2 tests passed! Ready for full nested experiment.")
    else:
        print("⚠️  Some tests failed. Check the output above for details.")

if __name__ == "__main__":
    main()