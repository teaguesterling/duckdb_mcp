#!/bin/bash
# Mock MCP server that sends a JSON response in multiple chunks without
# a newline until the very end. Used to test CR-10 (partial read bug).
#
# Protocol: reads one line from stdin, then writes a JSON-RPC response
# in small chunks with delays between them.

# Read the incoming request (we don't care about contents)
read -r request

# Send a valid JSON-RPC response in chunks WITHOUT newline between them.
# The partial-read bug causes ReadFromProcess to return after the first
# chunk when it gets EAGAIN, yielding incomplete JSON.
printf '{"jsonrpc":"2.0"'
sleep 0.05
printf ',"id":1'
sleep 0.05
printf ',"result":{"status":"ok"}}'
# Final newline to terminate the JSON-RPC line
printf '\n'
