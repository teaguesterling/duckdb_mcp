#!/usr/bin/env python3
"""
MCP server with pagination support for testing DuckDB MCP extension.
Implements cursor-based pagination following MCP specification.
"""

import json
import sys
import logging
from typing import Any, Optional
from mcp.server import Server
from mcp.types import (
    Resource,
    Tool,
    CallToolResult,
    TextContent,
    Prompt,
)
import base64

# Set up logging
log_file = '/mnt/aux-data/teague/Projects/duckdb_mcp/mcp_pagination_server.log'

logger = logging.getLogger()
logger.setLevel(logging.DEBUG)

file_handler = logging.FileHandler(log_file, mode='w')
file_handler.setLevel(logging.DEBUG)
file_formatter = logging.Formatter('[MCP-PAGINATION-SERVER] %(asctime)s - %(levelname)s - %(message)s')
file_handler.setFormatter(file_formatter)
logger.addHandler(file_handler)

stderr_handler = logging.StreamHandler(sys.stderr)
stderr_handler.setLevel(logging.DEBUG)
stderr_formatter = logging.Formatter('[MCP-PAGINATION] %(asctime)s - %(levelname)s - %(message)s')
stderr_handler.setFormatter(stderr_formatter)
logger.addHandler(stderr_handler)

logger.info(f"MCP Pagination Server logging to: {log_file}")


# Generate large dataset for pagination testing
def generate_large_resources(count: int) -> list[dict]:
    """Generate a large number of test resources."""
    resources = []
    categories = ['data', 'config', 'logs', 'reports', 'backups']

    for i in range(count):
        category = categories[i % len(categories)]
        resources.append(
            {
                'id': f'resource_{i:06d}',
                'uri': f'test://{category}/resource_{i:06d}.json',
                'name': f'Resource {i:06d}',
                'description': f'Test resource {i} in category {category}',
                'category': category,
                'size': 1024 + (i * 47) % 10000,  # Varying sizes
                'created_at': f'2024-01-{(i % 28) + 1:02d}T10:{(i % 60):02d}:00Z',
                'mime_type': 'application/json',
            }
        )

    return resources


def generate_large_tools(count: int) -> list[dict]:
    """Generate a large number of test tools."""
    tools = []
    tool_types = ['data_processor', 'analyzer', 'transformer', 'validator', 'exporter']

    for i in range(count):
        tool_type = tool_types[i % len(tool_types)]
        tools.append(
            {
                'id': f'tool_{i:06d}',
                'name': f'{tool_type}_{i:06d}',
                'description': f'Test {tool_type} tool number {i}',
                'type': tool_type,
                'version': f'1.{i % 10}.{i % 100}',
                'parameters': ['input', 'output', 'config'][: (i % 3) + 1],
                'category': tool_type,
            }
        )

    return tools


def generate_large_prompts(count: int) -> list[dict]:
    """Generate a large number of test prompts."""
    prompts = []
    prompt_types = ['analysis', 'summary', 'transformation', 'validation', 'reporting']

    for i in range(count):
        prompt_type = prompt_types[i % len(prompt_types)]
        prompts.append(
            {
                'id': f'prompt_{i:06d}',
                'name': f'{prompt_type}_prompt_{i:06d}',
                'description': f'Test {prompt_type} prompt number {i}',
                'type': prompt_type,
                'template': f'Execute {prompt_type} operation with parameters: {{input}}',
                'parameters': {
                    'input': {'type': 'string', 'description': f'Input for {prompt_type}'},
                    'options': {'type': 'object', 'description': 'Additional options'},
                },
            }
        )

    return prompts


# Generate test datasets
LARGE_RESOURCES = generate_large_resources(500)  # 500 resources for testing
LARGE_TOOLS = generate_large_tools(300)  # 300 tools for testing
LARGE_PROMPTS = generate_large_prompts(400)  # 400 prompts for testing


class PaginationCursor:
    """Helper class for pagination cursor management."""

    @staticmethod
    def encode_cursor(offset: int, limit: int, total: int) -> str:
        """Encode cursor as base64 JSON."""
        cursor_data = {'offset': offset, 'limit': limit, 'total': total, 'version': '1.0'}
        json_str = json.dumps(cursor_data)
        return base64.b64encode(json_str.encode()).decode()

    @staticmethod
    def decode_cursor(cursor: str) -> dict:
        """Decode cursor from base64 JSON."""
        try:
            json_str = base64.b64decode(cursor.encode()).decode()
            return json.loads(json_str)
        except Exception as e:
            logger.error(f"Failed to decode cursor: {cursor}, error: {e}")
            raise ValueError(f"Invalid cursor format: {cursor}")

    @staticmethod
    def paginate_list(items: list, cursor: Optional[str] = None, default_limit: int = 50) -> dict:
        """Paginate a list with cursor support."""
        total_items = len(items)

        if cursor:
            cursor_data = PaginationCursor.decode_cursor(cursor)
            offset = cursor_data.get('offset', 0)
            limit = cursor_data.get('limit', default_limit)
        else:
            offset = 0
            limit = default_limit

        # Ensure offset is within bounds
        offset = max(0, min(offset, total_items))
        limit = max(1, min(limit, 100))  # Cap limit at 100

        # Get page items
        page_items = items[offset : offset + limit]

        # Calculate next cursor
        next_offset = offset + limit
        has_more = next_offset < total_items
        next_cursor = None

        if has_more:
            next_cursor = PaginationCursor.encode_cursor(next_offset, limit, total_items)

        return {
            'items': page_items,
            'next_cursor': next_cursor,
            'has_more_pages': has_more,
            'total_items': len(page_items),
            'offset': offset,
            'limit': limit,
            'total_available': total_items,
        }


def main():
    server = Server("pagination-test-server")

    @server.list_resources()
    async def handle_list_resources() -> list[Resource]:
        """List resources (standard MCP - first page only)."""
        logger.info("Received standard list_resources request")

        # Standard MCP - return first page as list[Resource]
        page_data = PaginationCursor.paginate_list(LARGE_RESOURCES, None, default_limit=25)

        # Convert to Resource objects
        resources = []
        for resource_data in page_data['items']:
            resources.append(
                Resource(
                    uri=resource_data['uri'],
                    name=resource_data['name'],
                    description=resource_data['description'],
                    mimeType=resource_data['mime_type'],
                )
            )

        logger.info(f"Returning {len(resources)} resources (standard MCP)")
        return resources

    # Custom method for cursor-based pagination
    @server.call_tool()
    async def handle_call_tool(name: str, arguments: dict[str, Any]) -> CallToolResult:
        """Handle tool calls including pagination tools."""
        logger.info(f"Received call_tool request: {name} with args: {arguments}")

        if name == "list_resources_paginated":
            cursor = arguments.get("cursor", "")
            page_data = PaginationCursor.paginate_list(LARGE_RESOURCES, cursor or None, default_limit=25)

            # Convert to Resource objects
            resources = []
            for resource_data in page_data['items']:
                resources.append(
                    {
                        'uri': resource_data['uri'],
                        'name': resource_data['name'],
                        'description': resource_data['description'],
                        'mimeType': resource_data['mime_type'],
                    }
                )

            # Create MCP-compliant response
            result = {'resources': resources}

            # Add nextCursor if there are more pages
            if page_data['next_cursor']:
                result['nextCursor'] = page_data['next_cursor']

            logger.info(f"Returning paginated {len(resources)} resources, has_more: {page_data['has_more_pages']}")
            return CallToolResult(content=[TextContent(type="text", text=json.dumps(result))], isError=False)

        elif name == "list_tools_paginated":
            cursor = arguments.get("cursor", "")
            page_data = PaginationCursor.paginate_list(LARGE_TOOLS, cursor or None, default_limit=20)

            tools = []
            for tool_data in page_data['items']:
                tools.append(
                    {
                        'name': tool_data['name'],
                        'description': tool_data['description'],
                        'inputSchema': {
                            "type": "object",
                            "properties": {
                                "input": {"type": "string", "description": "Tool input"},
                                "config": {"type": "object", "description": "Tool configuration"},
                            },
                        },
                    }
                )

            result = {'tools': tools}
            if page_data['next_cursor']:
                result['nextCursor'] = page_data['next_cursor']

            logger.info(f"Returning paginated {len(tools)} tools, has_more: {page_data['has_more_pages']}")
            return CallToolResult(content=[TextContent(type="text", text=json.dumps(result))], isError=False)

        elif name == "list_prompts_paginated":
            cursor = arguments.get("cursor", "")
            page_data = PaginationCursor.paginate_list(LARGE_PROMPTS, cursor or None, default_limit=30)

            prompts = []
            for prompt_data in page_data['items']:
                prompts.append(
                    {
                        'name': prompt_data['name'],
                        'description': prompt_data['description'],
                        'arguments': list(prompt_data['parameters'].keys()),
                    }
                )

            result = {'prompts': prompts}
            if page_data['next_cursor']:
                result['nextCursor'] = page_data['next_cursor']

            logger.info(f"Returning paginated {len(prompts)} prompts, has_more: {page_data['has_more_pages']}")
            return CallToolResult(content=[TextContent(type="text", text=json.dumps(result))], isError=False)

        # Find regular tool by name
        tool_found = None
        for tool in LARGE_TOOLS:
            if tool['name'] == name:
                tool_found = tool
                break

        if not tool_found:
            return CallToolResult(
                content=[TextContent(type="text", text=f"Error: Tool not found: {name}")], isError=True
            )

        return CallToolResult(
            content=[
                TextContent(type="text", text=f"Tool {name} executed successfully with result: {json.dumps(arguments)}")
            ],
            isError=False,
        )

    @server.list_tools()
    async def handle_list_tools() -> list[Tool]:
        """List tools (standard MCP - first page only)."""
        logger.info("Received standard list_tools request")

        page_data = PaginationCursor.paginate_list(LARGE_TOOLS, None, default_limit=20)

        tools = []
        for tool_data in page_data['items']:
            tools.append(
                Tool(
                    name=tool_data['name'],
                    description=tool_data['description'],
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "input": {"type": "string", "description": "Tool input"},
                            "config": {"type": "object", "description": "Tool configuration"},
                        },
                    },
                )
            )

        # Add pagination tools
        tools.extend(
            [
                Tool(
                    name="list_resources_paginated",
                    description="List resources with cursor-based pagination",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "cursor": {"type": "string", "description": "Pagination cursor (empty for first page)"}
                        },
                    },
                ),
                Tool(
                    name="list_tools_paginated",
                    description="List tools with cursor-based pagination",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "cursor": {"type": "string", "description": "Pagination cursor (empty for first page)"}
                        },
                    },
                ),
                Tool(
                    name="list_prompts_paginated",
                    description="List prompts with cursor-based pagination",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "cursor": {"type": "string", "description": "Pagination cursor (empty for first page)"}
                        },
                    },
                ),
            ]
        )

        logger.info(f"Returning {len(tools)} tools (standard MCP)")
        return tools

    @server.list_prompts()
    async def handle_list_prompts() -> list[Prompt]:
        """List prompts (standard MCP - first page only)."""
        logger.info("Received standard list_prompts request")

        page_data = PaginationCursor.paginate_list(LARGE_PROMPTS, None, default_limit=30)

        prompts = []
        for prompt_data in page_data['items']:
            prompts.append(
                Prompt(
                    name=prompt_data['name'],
                    description=prompt_data['description'],
                    arguments=list(prompt_data['parameters'].keys()),
                )
            )

        logger.info(f"Returning {len(prompts)} prompts (standard MCP)")
        return prompts

    @server.read_resource()
    async def handle_read_resource(uri: str) -> str:
        """Read a resource by URI."""
        logger.info(f"Received read_resource request for URI: {uri}")

        # Find resource by URI
        for resource in LARGE_RESOURCES:
            if resource['uri'] == uri:
                return json.dumps(resource, indent=2)

        logger.error(f"Resource not found: {uri}")
        raise ValueError(f"Resource not found: {uri}")

    @server.call_tool()
    async def handle_call_tool(name: str, arguments: dict[str, Any]) -> CallToolResult:
        """Handle tool calls."""
        logger.info(f"Received call_tool request: {name} with args: {arguments}")

        # Find tool by name
        tool_found = None
        for tool in LARGE_TOOLS:
            if tool['name'] == name:
                tool_found = tool
                break

        if not tool_found:
            return CallToolResult(
                content=[TextContent(type="text", text=f"Error: Tool not found: {name}")], isError=True
            )

        return CallToolResult(
            content=[
                TextContent(type="text", text=f"Tool {name} executed successfully with result: {json.dumps(arguments)}")
            ],
            isError=False,
        )

    logger.info("MCP Pagination Test Server starting...")
    logger.info(f"Generated {len(LARGE_RESOURCES)} resources, {len(LARGE_TOOLS)} tools, {len(LARGE_PROMPTS)} prompts")

    # Run the server
    import mcp.server.stdio
    import asyncio

    async def run_server():
        try:
            async with mcp.server.stdio.stdio_server() as (read_stream, write_stream):
                await server.run(read_stream, write_stream, server.create_initialization_options())
        except Exception as e:
            logger.error(f"Server error: {e}")
            raise

    try:
        asyncio.run(run_server())
    except KeyboardInterrupt:
        logger.info("Server shutdown requested")
    except Exception as e:
        logger.error(f"Server failed: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
