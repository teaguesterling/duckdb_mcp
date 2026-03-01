#!/bin/bash
# Mock MCP server that takes a while to start up (simulates slow startup).
# Used to test CR-21 (startup polling vs fixed sleep).
sleep 0.5

# Once "started", act as a simple echo server
while IFS= read -r line; do
    id=$(echo "$line" | grep -o '"id":[0-9]*' | head -1 | cut -d: -f2)
    if [ -z "$id" ]; then
        id=1
    fi
    echo "{\"jsonrpc\":\"2.0\",\"id\":${id},\"result\":{\"status\":\"ok\"}}"
done
