#!/usr/bin/env python3
"""
Sample MCP server for testing DuckDB MCP extension.
Provides simple CSV resources for testing file system integration.
"""

import json
import sys
import logging
from typing import Any
from mcp.server import Server

# Set up logging to both stderr and file for debugging
import os

# Create log file in absolute path to this directory  
log_file = '/mnt/aux-data/teague/Projects/duckdb_mcp/mcp_server.log'

# Configure logging
logger = logging.getLogger()
logger.setLevel(logging.DEBUG)

# File handler
file_handler = logging.FileHandler(log_file, mode='w')  # Overwrite each time
file_handler.setLevel(logging.DEBUG)
file_formatter = logging.Formatter('[MCP-SERVER-FILE] %(asctime)s - %(levelname)s - %(message)s')
file_handler.setFormatter(file_formatter)
logger.addHandler(file_handler)

# Stderr handler
stderr_handler = logging.StreamHandler(sys.stderr)
stderr_handler.setLevel(logging.DEBUG) 
stderr_formatter = logging.Formatter('[MCP-SERVER] %(asctime)s - %(levelname)s - %(message)s')
stderr_handler.setFormatter(stderr_formatter)
logger.addHandler(stderr_handler)

# Log the file location for debugging
logger.info(f"MCP Server logging to file: {log_file}")
from mcp.types import (
    Resource,
    Tool,
    CallToolResult,
    TextContent,
)

# Sample CSV data
SAMPLE_CSV_DATA = {
    "customers.csv": """id,name,email,age
1,Alice Johnson,alice@example.com,28
2,Bob Smith,bob@example.com,35
3,Charlie Brown,charlie@example.com,42
4,Diana Prince,diana@example.com,31
5,Eve Wilson,eve@example.com,29""",
    
    "orders.csv": """order_id,customer_id,product,quantity,price
101,1,Widget A,2,19.99
102,2,Widget B,1,29.99
103,1,Widget C,3,15.99
104,3,Widget A,1,19.99
105,4,Widget B,2,29.99
106,5,Widget C,1,15.99""",
    
    "products.csv": """product_id,name,category,price,in_stock
1,Widget A,Electronics,19.99,150
2,Widget B,Electronics,29.99,75
3,Widget C,Home,15.99,200
4,Gadget X,Electronics,49.99,50
5,Tool Y,Tools,35.99,100"""
}

def main():
    # Create the MCP server
    server = Server("sample-data-server")

    @server.list_resources()
    async def handle_list_resources() -> list[Resource]:
        """List available CSV resources."""
        logging.info("Received list_resources request")
        resources = []
        for filename in SAMPLE_CSV_DATA.keys():
            resources.append(Resource(
                uri=f"file:///{filename}",
                name=filename,
                description=f"Sample CSV data: {filename}",
                mimeType="text/csv"
            ))
        logging.info(f"Returning {len(resources)} resources: {[r.uri for r in resources]}")
        logging.info(f"Resource objects: {resources}")
        return resources

    @server.read_resource()
    async def handle_read_resource(uri: str) -> str:
        """Read a CSV resource by URI."""
        logging.info(f"Received read_resource request for URI: {uri}")
        
        # Convert URI to string if it's not already
        uri_str = str(uri)
        
        # Extract filename from URI
        if uri_str.startswith("file:///"):
            filename = uri_str[8:]  # Remove "file:///" prefix
        else:
            logging.error(f"Unsupported URI scheme: {uri_str}")
            raise ValueError(f"Unsupported URI scheme: {uri_str}")
        
        if filename not in SAMPLE_CSV_DATA:
            logging.error(f"Resource not found: {filename}")
            raise ValueError(f"Resource not found: {filename}")
        
        data = SAMPLE_CSV_DATA[filename]
        logging.info(f"Returning {len(data)} bytes for {filename}")
        logging.info(f"Data preview: {data[:100]}...")
        return data

    @server.list_tools()
    async def handle_list_tools() -> list[Tool]:
        """List available tools."""
        return [
            Tool(
                name="get_data_info",
                description="Get information about available datasets",
                inputSchema={
                    "type": "object",
                    "properties": {
                        "dataset": {
                            "type": "string",
                            "description": "Dataset name (optional)",
                            "enum": list(SAMPLE_CSV_DATA.keys())
                        }
                    },
                    "additionalProperties": False
                }
            )
        ]

    @server.call_tool()
    async def handle_call_tool(name: str, arguments: dict[str, Any]) -> CallToolResult:
        """Handle tool calls."""
        if name == "get_data_info":
            dataset = arguments.get("dataset")
            if dataset:
                if dataset not in SAMPLE_CSV_DATA:
                    return CallToolResult(
                        content=[TextContent(type="text", text=f"Error: Unknown dataset: {dataset}")],
                        isError=True
                    )
                
                # Count rows (excluding header)
                rows = len(SAMPLE_CSV_DATA[dataset].strip().split('\n')) - 1
                return CallToolResult(
                    content=[TextContent(type="text", text=f"Dataset '{dataset}' has {rows} rows")]
                )
            else:
                # List all datasets
                info = []
                for name, data in SAMPLE_CSV_DATA.items():
                    rows = len(data.strip().split('\n')) - 1
                    info.append(f"- {name}: {rows} rows")
                
                return CallToolResult(
                    content=[TextContent(
                        type="text", 
                        text=f"Available datasets:\n" + "\n".join(info)
                    )]
                )
        else:
            return CallToolResult(
                content=[TextContent(type="text", text=f"Error: Unknown tool: {name}")],
                isError=True
            )

    # Run the server
    import asyncio
    from mcp.server.stdio import stdio_server
    
    async def main_async():
        logging.info("Starting MCP server with stdio transport")
        logging.info("Waiting for client connections...")
        async with stdio_server() as (read_stream, write_stream):
            logging.info("Client connected, starting server.run()")
            await server.run(
                read_stream,
                write_stream,
                server.create_initialization_options()
            )
    
    asyncio.run(main_async())

if __name__ == "__main__":
    main()