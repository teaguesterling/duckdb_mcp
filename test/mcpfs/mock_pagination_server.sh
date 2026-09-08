#!/bin/sh
# Mock MCP server used by test/sql/mcp_list_cursor.test.
#
# Speaks just enough JSON-RPC 2.0 over stdio to answer `initialize` and the
# three MCP list methods, each of which is *paginated*: the first page carries
# a `nextCursor`, and asking again with that cursor returns the second page.
#
# It behaves like a standards-compliant MCP server in one respect that matters
# for the test: it exposes no tools at all, so a `tools/call` for a tool named
# `list_resources_paginated` (or the tools/prompts equivalents) is answered
# with a JSON-RPC error, exactly as a real server would.

respond() {
    # $1 = id, $2 = result JSON
    printf '{"jsonrpc":"2.0","id":%s,"result":%s}\n' "$1" "$2"
}

fail() {
    # $1 = id, $2 = code, $3 = message
    printf '{"jsonrpc":"2.0","id":%s,"error":{"code":%s,"message":"%s"}}\n' "$1" "$2" "$3"
}

INIT_RESULT='{"protocolVersion":"2024-11-05","capabilities":{"resources":{},"tools":{},"prompts":{}},"serverInfo":{"name":"pagination-mock","version":"1.0"}}'

RESOURCES_P1='{"resources":[{"uri":"res://page1","name":"page1","mimeType":"text/plain"}],"nextCursor":"CURSOR-2"}'
RESOURCES_P2='{"resources":[{"uri":"res://page2","name":"page2","mimeType":"text/plain"}]}'

TOOLS_P1='{"tools":[{"name":"tool_page1","description":"first page"}],"nextCursor":"CURSOR-2"}'
TOOLS_P2='{"tools":[{"name":"tool_page2","description":"second page"}]}'

PROMPTS_P1='{"prompts":[{"name":"prompt_page1","description":"first page"}],"nextCursor":"CURSOR-2"}'
PROMPTS_P2='{"prompts":[{"name":"prompt_page2","description":"second page"}]}'

while IFS= read -r line; do
    method=$(printf '%s' "$line" | sed -n 's/.*"method":"\([^"]*\)".*/\1/p')
    id=$(printf '%s' "$line" | sed -n 's/.*"id":\([0-9][0-9]*\).*/\1/p')
    cursor=$(printf '%s' "$line" | sed -n 's/.*"cursor":"\([^"]*\)".*/\1/p')

    # Notifications carry no id and must not be answered.
    if [ -z "$id" ]; then
        continue
    fi

    case "$method" in
    initialize)
        respond "$id" "$INIT_RESULT"
        ;;
    ping)
        respond "$id" '{}'
        ;;
    resources/list)
        if [ "$cursor" = "CURSOR-2" ]; then
            respond "$id" "$RESOURCES_P2"
        elif [ -z "$cursor" ]; then
            respond "$id" "$RESOURCES_P1"
        else
            fail "$id" -32602 "Invalid cursor"
        fi
        ;;
    tools/list)
        if [ "$cursor" = "CURSOR-2" ]; then
            respond "$id" "$TOOLS_P2"
        elif [ -z "$cursor" ]; then
            respond "$id" "$TOOLS_P1"
        else
            fail "$id" -32602 "Invalid cursor"
        fi
        ;;
    prompts/list)
        if [ "$cursor" = "CURSOR-2" ]; then
            respond "$id" "$PROMPTS_P2"
        elif [ -z "$cursor" ]; then
            respond "$id" "$PROMPTS_P1"
        else
            fail "$id" -32602 "Invalid cursor"
        fi
        ;;
    tools/call)
        # A real MCP server has no `list_*_paginated` tool. Reject it the way
        # the spec says: an error, not a page of results.
        name=$(printf '%s' "$line" | sed -n 's/.*"name":"\([^"]*\)".*/\1/p')
        fail "$id" -32602 "Unknown tool: $name"
        ;;
    *)
        fail "$id" -32601 "Method not found"
        ;;
    esac
done
