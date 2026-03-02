#!/bin/bash
# Simple mock MCP server that reads JSON-RPC requests and echoes back a
# fixed valid response. Used as a baseline for stdio transport tests.

while IFS= read -r line; do
    # Extract the id from the request (simple grep)
    id=$(echo "$line" | grep -o '"id":[0-9]*' | head -1 | cut -d: -f2)
    if [ -z "$id" ]; then
        id=1
    fi
    echo "{\"jsonrpc\":\"2.0\",\"id\":${id},\"result\":{\"status\":\"ok\"}}"
done
