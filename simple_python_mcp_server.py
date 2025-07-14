#!/usr/bin/env python3
"""
Simple Python MCP Server - Base Layer
Provides basic resources and tools for testing nested MCP architecture
"""
import json
import sys
import csv
import io

class SimpleMCPServer:
    def __init__(self):
        self.resources = {
            "data://customers": {
                "name": "customers",
                "description": "Sample customer data",
                "mimeType": "application/json",
                "data": [
                    {"id": 1, "name": "Alice Smith", "city": "New York", "spent": 1200.50},
                    {"id": 2, "name": "Bob Jones", "city": "Chicago", "spent": 850.75},
                    {"id": 3, "name": "Carol Brown", "city": "San Francisco", "spent": 2100.25}
                ]
            },
            "data://products": {
                "name": "products", 
                "description": "Sample product catalog",
                "mimeType": "application/json",
                "data": [
                    {"id": 101, "name": "Laptop", "category": "Electronics", "price": 999.99},
                    {"id": 102, "name": "Mouse", "category": "Electronics", "price": 29.99},
                    {"id": 103, "name": "Desk", "category": "Furniture", "price": 299.99}
                ]
            }
        }
        
    def handle_initialize(self, request):
        return {
            "jsonrpc": "2.0",
            "id": request["id"],
            "result": {
                "protocolVersion": "2024-11-05",
                "capabilities": {
                    "resources": {"subscribe": False},
                    "tools": {}
                },
                "serverInfo": {
                    "name": "Simple Python MCP Server",
                    "version": "1.0.0"
                }
            }
        }
    
    def handle_resources_list(self, request):
        resources = []
        for uri, info in self.resources.items():
            resources.append({
                "uri": uri,
                "name": info["name"],
                "description": info["description"],
                "mimeType": info["mimeType"]
            })
        
        return {
            "jsonrpc": "2.0", 
            "id": request["id"],
            "result": {"resources": resources}
        }
    
    def handle_resources_read(self, request):
        uri = request["params"]["uri"]
        if uri not in self.resources:
            return {
                "jsonrpc": "2.0",
                "id": request["id"], 
                "error": {"code": -1, "message": f"Resource not found: {uri}"}
            }
        
        resource = self.resources[uri]
        content = json.dumps(resource["data"], indent=2)
        
        return {
            "jsonrpc": "2.0",
            "id": request["id"],
            "result": {
                "contents": [{
                    "uri": uri,
                    "mimeType": resource["mimeType"],
                    "text": content
                }]
            }
        }
    
    def handle_tools_list(self, request):
        tools = [
            {
                "name": "count_records",
                "description": "Count records in a dataset",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "dataset": {"type": "string", "description": "Dataset name (customers or products)"}
                    },
                    "required": ["dataset"]
                }
            },
            {
                "name": "filter_by_field",
                "description": "Filter dataset by field value",
                "inputSchema": {
                    "type": "object", 
                    "properties": {
                        "dataset": {"type": "string"},
                        "field": {"type": "string"},
                        "value": {"type": "string"}
                    },
                    "required": ["dataset", "field", "value"]
                }
            }
        ]
        
        return {
            "jsonrpc": "2.0",
            "id": request["id"],
            "result": {"tools": tools}
        }
    
    def handle_tools_call(self, request):
        tool_name = request["params"]["name"]
        args = request["params"]["arguments"]
        
        if tool_name == "count_records":
            dataset = args["dataset"]
            if dataset == "customers":
                count = len(self.resources["data://customers"]["data"])
            elif dataset == "products":
                count = len(self.resources["data://products"]["data"])
            else:
                return {
                    "jsonrpc": "2.0",
                    "id": request["id"],
                    "error": {"code": -1, "message": f"Unknown dataset: {dataset}"}
                }
            
            return {
                "jsonrpc": "2.0",
                "id": request["id"], 
                "result": {
                    "content": [{"type": "text", "text": f"Dataset '{dataset}' has {count} records"}]
                }
            }
            
        elif tool_name == "filter_by_field":
            dataset = args["dataset"]
            field = args["field"]
            value = args["value"]
            
            if dataset == "customers":
                data = self.resources["data://customers"]["data"]
            elif dataset == "products":
                data = self.resources["data://products"]["data"]
            else:
                return {
                    "jsonrpc": "2.0",
                    "id": request["id"],
                    "error": {"code": -1, "message": f"Unknown dataset: {dataset}"}
                }
            
            filtered = [item for item in data if str(item.get(field, "")) == value]
            result_text = json.dumps(filtered, indent=2)
            
            return {
                "jsonrpc": "2.0",
                "id": request["id"],
                "result": {
                    "content": [{"type": "text", "text": result_text}]
                }
            }
        
        return {
            "jsonrpc": "2.0",
            "id": request["id"],
            "error": {"code": -1, "message": f"Unknown tool: {tool_name}"}
        }
    
    def run(self):
        """Main server loop"""
        for line in sys.stdin:
            try:
                request = json.loads(line.strip())
                method = request["method"]
                
                if method == "initialize":
                    response = self.handle_initialize(request)
                elif method == "resources/list":
                    response = self.handle_resources_list(request)
                elif method == "resources/read":
                    response = self.handle_resources_read(request)
                elif method == "tools/list":
                    response = self.handle_tools_list(request)
                elif method == "tools/call":
                    response = self.handle_tools_call(request)
                else:
                    response = {
                        "jsonrpc": "2.0",
                        "id": request.get("id"),
                        "error": {"code": -32601, "message": f"Method not found: {method}"}
                    }
                
                print(json.dumps(response), flush=True)
                
            except json.JSONDecodeError:
                error_response = {
                    "jsonrpc": "2.0", 
                    "id": None,
                    "error": {"code": -32700, "message": "Parse error"}
                }
                print(json.dumps(error_response), flush=True)
            except Exception as e:
                error_response = {
                    "jsonrpc": "2.0",
                    "id": request.get("id") if 'request' in locals() else None,
                    "error": {"code": -32603, "message": f"Internal error: {str(e)}"}
                }
                print(json.dumps(error_response), flush=True)

if __name__ == "__main__":
    server = SimpleMCPServer()
    server.run()