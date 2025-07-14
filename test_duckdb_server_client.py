#!/usr/bin/env python3
"""
Comprehensive Python test client for DuckDB MCP Server
Tests all server functionality systematically
"""
import json
import subprocess
import time
import tempfile
import os
from typing import Dict, Any, Optional

class DuckDBMCPClient:
    def __init__(self):
        self.process = None
        self.request_id = 0
        
    def start_server(self, setup_sql: str = None):
        """Start DuckDB MCP server with optional setup SQL"""
        print("🚀 Starting DuckDB MCP server...")
        
        # Create setup file if provided
        setup_file = None
        if setup_sql:
            with tempfile.NamedTemporaryFile(mode='w', suffix='.sql', delete=False) as f:
                f.write(setup_sql)
                setup_file = f.name
        
        # Start server in foreground mode
        env = os.environ.copy()
        env['DUCKDB_MCP_FOREGROUND'] = '1'
        
        cmd = ["build/release/duckdb"]
        if setup_file:
            cmd.extend(["-init", setup_file])
            
        self.process = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env,
            bufsize=0
        )
        
        # Give server time to start
        time.sleep(2)
        
        # Cleanup setup file
        if setup_file:
            os.unlink(setup_file)
            
        print("✅ Server started")
        
    def send_request(self, method: str, params: Dict[str, Any] = None) -> Optional[Dict]:
        """Send MCP request and get response"""
        if not self.process:
            raise RuntimeError("Server not started")
            
        self.request_id += 1
        request = {
            "jsonrpc": "2.0",
            "method": method,
            "id": self.request_id
        }
        
        if params:
            request["params"] = params
            
        print(f"📤 Sending: {method}")
        self.process.stdin.write(json.dumps(request) + "\n")
        self.process.stdin.flush()
        
        # Read response
        response_line = self.process.stdout.readline()
        if not response_line:
            print("❌ No response received")
            return None
            
        try:
            response = json.loads(response_line.strip())
            if "error" in response:
                print(f"❌ Error: {response['error']['message']}")
                return response
            else:
                print(f"✅ Success")
                return response
        except json.JSONDecodeError as e:
            print(f"❌ JSON decode error: {e}")
            print(f"Raw response: {response_line}")
            return None
    
    def initialize(self):
        """Initialize MCP connection"""
        return self.send_request("initialize", {
            "protocolVersion": "2024-11-05",
            "capabilities": {"resources": {"subscribe": False}},
            "clientInfo": {"name": "DuckDB-Test-Client", "version": "1.0.0"}
        })
    
    def list_resources(self):
        """List available resources"""
        return self.send_request("resources/list")
    
    def read_resource(self, uri: str):
        """Read a specific resource"""
        return self.send_request("resources/read", {"uri": uri})
    
    def list_tools(self):
        """List available tools"""
        return self.send_request("tools/list")
    
    def call_tool(self, name: str, arguments: Dict[str, Any]):
        """Call a tool"""
        return self.send_request("tools/call", {"name": name, "arguments": arguments})
    
    def stop_server(self):
        """Stop the server"""
        if self.process:
            try:
                self.process.terminate()
                self.process.wait(timeout=3)
            except:
                self.process.kill()
            self.process = None
            print("🛑 Server stopped")

def test_basic_server():
    """Test basic server functionality without resources"""
    print("\n" + "="*60)
    print("TEST 1: Basic Server (No Resources)")
    print("="*60)
    
    client = DuckDBMCPClient()
    
    try:
        # Start server with minimal setup
        setup_sql = """
        CREATE TABLE dummy (id INT);
        INSERT INTO dummy VALUES (1);
        SELECT mcp_server_start('stdio', 'localhost', 8080, '{}') AS start_result;
        """
        
        client.start_server(setup_sql)
        
        # Test initialize
        print("\n1. Initialize:")
        result = client.initialize()
        if result and "result" in result:
            server_info = result["result"].get("serverInfo", {})
            print(f"   Server: {server_info.get('name')} v{server_info.get('version')}")
            caps = result["result"].get("capabilities", {})
            print(f"   Capabilities: resources={caps.get('resources', False)}, tools={caps.get('tools', False)}")
        
        # Test list resources (should be empty)
        print("\n2. List Resources:")
        result = client.list_resources()
        if result and "result" in result:
            resources = result["result"].get("resources", [])
            print(f"   Found {len(resources)} resources")
        
        # Test list tools
        print("\n3. List Tools:")
        result = client.list_tools()
        if result and "result" in result:
            tools = result["result"].get("tools", [])
            print(f"   Found {len(tools)} tools:")
            for tool in tools:
                print(f"     - {tool.get('name')}: {tool.get('description')}")
                
    finally:
        client.stop_server()

def test_server_with_resources():
    """Test server with published resources"""
    print("\n" + "="*60)
    print("TEST 2: Server With Published Resources")
    print("="*60)
    
    client = DuckDBMCPClient()
    
    try:
        # Start server and publish resources
        setup_sql = """
        CREATE TABLE customers (id INT, name VARCHAR, city VARCHAR, spent DECIMAL(10,2));
        INSERT INTO customers VALUES 
          (1, 'Alice', 'NYC', 1200.50),
          (2, 'Bob', 'Chicago', 850.75),
          (3, 'Carol', 'SF', 2100.25);
          
        CREATE TABLE products (id INT, name VARCHAR, price DECIMAL(10,2));
        INSERT INTO products VALUES
          (101, 'Widget A', 19.99),
          (102, 'Widget B', 29.99);
        
        SELECT mcp_server_start('stdio', 'localhost', 8080, '{}') AS start_result;
        SELECT mcp_publish_table('customers', 'data://tables/customers', 'json') AS pub1;
        SELECT mcp_publish_table('products', 'data://tables/products', 'csv') AS pub2;
        """
        
        client.start_server(setup_sql)
        
        # Initialize
        print("\n1. Initialize:")
        client.initialize()
        
        # List resources
        print("\n2. List Resources:")
        result = client.list_resources()
        resources = []
        if result and "result" in result:
            resources = result["result"].get("resources", [])
            print(f"   Found {len(resources)} resources:")
            for resource in resources:
                print(f"     - {resource.get('uri')}: {resource.get('name')} ({resource.get('mimeType')})")
        
        # Test reading each resource
        for resource in resources:
            uri = resource.get('uri')
            print(f"\n3. Read Resource: {uri}")
            result = client.read_resource(uri)
            if result and "result" in result:
                contents = result["result"].get("contents", [])
                if contents:
                    text = contents[0].get("text", "")
                    print(f"   Content preview: {text[:100]}{'...' if len(text) > 100 else ''}")
        
        # Test query tool
        print("\n4. Test Query Tool:")
        result = client.call_tool("query", {
            "sql": "SELECT city, COUNT(*) as customer_count, SUM(spent) as total_spent FROM customers GROUP BY city",
            "format": "json"
        })
        if result and "result" in result:
            content = result["result"].get("content", [])
            if content:
                print(f"   Query result: {content[0].get('text', '')[:200]}...")
                
        # Test describe tool
        print("\n5. Test Describe Tool:")
        result = client.call_tool("describe", {"object_name": "customers"})
        if result and "result" in result:
            content = result["result"].get("content", [])
            if content:
                print(f"   Describe result: {content[0].get('text', '')[:200]}...")
                
    finally:
        client.stop_server()

def main():
    """Run all tests"""
    print("🧪 DuckDB MCP Server Test Suite")
    print("Testing our DuckDB MCP server implementation")
    
    try:
        test_basic_server()
        test_server_with_resources()
        
        print("\n" + "="*60)
        print("✅ ALL TESTS COMPLETED")
        print("="*60)
        
    except Exception as e:
        print(f"\n❌ Test suite failed: {e}")

if __name__ == "__main__":
    main()