#!/bin/sh
# Mock MCP server used by test/sql/mcpfs_resource_extension.test (issue #100).
#
# Speaks just enough JSON-RPC 2.0 over stdio to answer `initialize`,
# `resources/list` and `resources/read`.
#
# Readers decide what they can open from a path's extension, so a resource
# whose URI carries none (`note://readme`) is refused by e.g. read_markdown
# even though mcpfs can read it. The resource set is shaped around that:
#
#   note://readme         text/markdown                -> exposed as note://readme.md
#   note://data           application/json             -> exposed as note://data.json
#   note://page           text/html; charset=utf-8     -> exposed as note://page.html
#   note://already.md     text/markdown                -> already carries it; unchanged
#   note://plain          text/plain                   -> no extension for this type
#   note://bare           (no mimeType at all)         -> unchanged
#   note://twin           text/markdown  \  decorating twin would collide with
#   note://twin.md        text/markdown  /  twin.md, so twin stays undecorated
#
# Every `resources/read` answer echoes the resource's mimeType, because the
# extension-stripping fallback on open checks it.

respond() {
    # $1 = id, $2 = result JSON
    printf '{"jsonrpc":"2.0","id":%s,"result":%s}\n' "$1" "$2"
}

fail() {
    # $1 = id, $2 = code, $3 = message
    printf '{"jsonrpc":"2.0","id":%s,"error":{"code":%s,"message":"%s"}}\n' "$1" "$2" "$3"
}

INIT_RESULT='{"protocolVersion":"2024-11-05","capabilities":{"resources":{}},"serverInfo":{"name":"extension-mock","version":"1.0"}}'

LIST_RESULT='{"resources":[{"uri":"note://readme","name":"readme","mimeType":"text/markdown"},{"uri":"note://data","name":"data","mimeType":"application/json"},{"uri":"note://page","name":"page","mimeType":"text/html; charset=utf-8"},{"uri":"note://already.md","name":"already","mimeType":"text/markdown"},{"uri":"note://plain","name":"plain","mimeType":"text/plain"},{"uri":"note://bare","name":"bare"},{"uri":"note://twin","name":"twin","mimeType":"text/markdown"},{"uri":"note://twin.md","name":"twin-md","mimeType":"text/markdown"}]}'

read_result() {
    case "$1" in
    "note://readme")
        printf '%s' '{"contents":[{"uri":"note://readme","mimeType":"text/markdown","text":"README"}]}'
        ;;
    "note://data")
        printf '%s' '{"contents":[{"uri":"note://data","mimeType":"application/json","text":"DATA"}]}'
        ;;
    "note://page")
        printf '%s' '{"contents":[{"uri":"note://page","mimeType":"text/html; charset=utf-8","text":"PAGE"}]}'
        ;;
    "note://already.md")
        printf '%s' '{"contents":[{"uri":"note://already.md","mimeType":"text/markdown","text":"ALREADY"}]}'
        ;;
    "note://plain")
        printf '%s' '{"contents":[{"uri":"note://plain","mimeType":"text/plain","text":"PLAIN"}]}'
        ;;
    "note://bare")
        printf '%s' '{"contents":[{"uri":"note://bare","text":"BARE"}]}'
        ;;
    "note://twin")
        printf '%s' '{"contents":[{"uri":"note://twin","mimeType":"text/markdown","text":"TWIN-BARE"}]}'
        ;;
    "note://twin.md")
        printf '%s' '{"contents":[{"uri":"note://twin.md","mimeType":"text/markdown","text":"TWIN-MD"}]}'
        ;;
    *)
        printf '%s' ''
        ;;
    esac
}

while IFS= read -r line; do
    method=$(printf '%s' "$line" | sed -n 's/.*"method":"\([^"]*\)".*/\1/p')
    id=$(printf '%s' "$line" | sed -n 's/.*"id":\([0-9][0-9]*\).*/\1/p')

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
        respond "$id" "$LIST_RESULT"
        ;;
    resources/read)
        uri=$(printf '%s' "$line" | sed -n 's/.*"uri":"\([^"]*\)".*/\1/p')
        result=$(read_result "$uri")
        if [ -z "$result" ]; then
            fail "$id" -32002 "Resource not found: $uri"
        else
            respond "$id" "$result"
        fi
        ;;
    *)
        fail "$id" -32601 "Method not found"
        ;;
    esac
done
