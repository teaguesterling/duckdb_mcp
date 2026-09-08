#!/bin/sh
# Mock MCP server used by test/sql/mcp_client_message_routing.test.
#
# It is "noisy" in the way real MCP servers are: before answering any request
# other than `initialize` it emits a `notifications/message` line. Notifications
# are legal at any point in the stream and carry no id, so a client that simply
# takes the next line off the pipe as "the response" gets the notification
# instead -- and the real response then stays buffered and is served to the
# *next* request.
#
# It also echoes back what it actually received for `prompts/get`, so a client
# that drops the params on the wire is visible in the result rather than only
# in a "Missing prompt name" error.

respond() {
    # $1 = id, $2 = result JSON
    printf '{"jsonrpc":"2.0","id":%s,"result":%s}\n' "$1" "$2"
}

chatter() {
    printf '%s\n' '{"jsonrpc":"2.0","method":"notifications/message","params":{"level":"info","logger":"mock","data":"working on it"}}'
}

INIT_RESULT='{"protocolVersion":"2024-11-05","capabilities":{"resources":{},"tools":{},"prompts":{}},"serverInfo":{"name":"noisy-mock","version":"1.0"}}'
TOOLS_RESULT='{"tools":[{"name":"noisy_tool","description":"a tool"}]}'
RESOURCES_RESULT='{"resources":[{"uri":"res://noisy","name":"noisy","mimeType":"text/plain"}]}'

while IFS= read -r line; do
    method=$(printf '%s' "$line" | sed -n 's/.*"method":"\([^"]*\)".*/\1/p')
    id=$(printf '%s' "$line" | sed -n 's/.*"id":\([0-9][0-9]*\).*/\1/p')

    # Notifications carry no id and must not be answered.
    if [ -z "$id" ]; then
        continue
    fi

    case "$method" in
    initialize)
        # Deliberately quiet: the handshake must succeed either way, so that the
        # test isolates the corruption to the calls that follow it.
        respond "$id" "$INIT_RESULT"
        ;;
    ping)
        respond "$id" '{}'
        ;;
    tools/list)
        chatter
        respond "$id" "$TOOLS_RESULT"
        ;;
    resources/list)
        chatter
        respond "$id" "$RESOURCES_RESULT"
        ;;
    prompts/get)
        chatter
        # Report what actually arrived. Pre-fix the client serialised
        # `params: {}` for prompts/get, so both of these come back empty.
        got_name=$(printf '%s' "$line" | sed -n 's/.*"name":"\([^"]*\)".*/\1/p')
        if printf '%s' "$line" | grep -q '"table":"sales"'; then
            got_args=yes
        else
            got_args=no
        fi
        respond "$id" "{\"description\":\"echo\",\"got_name\":\"$got_name\",\"got_args\":\"$got_args\",\"messages\":[]}"
        ;;
    *)
        chatter
        printf '{"jsonrpc":"2.0","id":%s,"error":{"code":-32601,"message":"Method not found"}}\n' "$id"
        ;;
    esac
done
