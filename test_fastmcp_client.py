#!/usr/bin/env python3
"""
Test FastMCP as a client to our DuckDB MCP server
"""
import asyncio
import subprocess
import time
import os
import tempfile
from mcp.client.stdio import stdio_client, StdioServerParameters

async def test_duckdb_server():
    """Test connecting to DuckDB MCP server using FastMCP client"""
    
    # Create setup script for DuckDB server
    setup_script = """
    CREATE TABLE test_data (id INT, name VARCHAR, value DECIMAL(10,2));
    INSERT INTO test_data VALUES 
      (1, 'Alice', 100.50),
      (2, 'Bob', 200.75),
      (3, 'Carol', 300.25);

    SELECT mcp_server_start('stdio', 'localhost', 8080, '{}') AS server_start;
    SELECT mcp_publish_table('test_data', 'data://tables/test_data', 'json') AS publish_result;
    """

    with tempfile.NamedTemporaryFile(mode='w', suffix='.sql', delete=False) as f:
        f.write(setup_script)
        setup_file = f.name

    try:
        # Use FastMCP client to connect to DuckDB server
        print("📡 Connecting with FastMCP client...")
        
        # Set up environment for DuckDB server
        env = os.environ.copy()
        env['DUCKDB_MCP_FOREGROUND'] = '1'
        
        server_params = StdioServerParameters(
            command="build/release/duckdb",
            args=["-init", setup_file],
            env=env
        )
        
        async with stdio_client(server_params) as (read_stream, write_stream):
            from mcp import ClientSession
            
            # Create client session
            print("1. Initialize connection...")
            async with ClientSession(read_stream, write_stream) as client:
                # Initialize
                result = await client.initialize()
                print("✅ Connected!")
                
                # List resources
                print("\n2. List resources...")
                resources = await client.list_resources()
                print(f"✅ Found {len(resources)} resources:")
                for resource in resources:
                    print(f"   - {resource.uri}: {resource.name}")
                
                # Read a resource
                if resources:
                    print("\n3. Read first resource...")
                    uri = resources[0].uri
                    content = await client.read_resource(uri)
                    print(f"✅ Resource content: {str(content)[:100]}...")
                
                # List tools
                print("\n4. List tools...")
                tools = await client.list_tools()
                print(f"✅ Found {len(tools)} tools:")
                for tool in tools:
                    print(f"   - {tool.name}: {tool.description}")
                
                # Call query tool
                print("\n5. Call query tool...")
                result = await client.call_tool("query", {
                    "sql": "SELECT COUNT(*) as total FROM test_data",
                    "format": "json"
                })
                print(f"✅ Query result: {result}")
            
        print("\n🎉 All tests completed successfully!")
        
    except Exception as e:
        print(f"❌ Error: {e}")
        import traceback
        traceback.print_exc()
        
    finally:
        os.unlink(setup_file)

if __name__ == "__main__":
    asyncio.run(test_duckdb_server())