#!/bin/sh
# Mock MCP server used by test/sql/mcpfs_glob_semantics.test (issue #84, item 3).
#
# Speaks just enough JSON-RPC 2.0 over stdio to answer `initialize`,
# `resources/list` and `resources/read`.
#
# Two properties matter for the test:
#
#   1. `resources/list` is PAGINATED. The first page carries a `nextCursor`;
#      the second page is only reachable by asking again with that cursor.
#      `data://beta.csv` lives on page 2 and is invisible to any caller that
#      stops after the first page.
#
#   2. The resource URIs are chosen so that glob matching and substring
#      matching disagree:
#
#        data://alpha.csv       matches  data://*.csv    (glob and substring)
#        data://alpha.csv.bak   matches  data://*.csv    ONLY as a substring
#        data://notes.txt       matches  neither
#        data://beta.csv        matches  data://*.csv    (page 2 only)
#
#      and the literal, wildcard-free pattern `data://alpha` names no resource
#      at all, yet is a substring of two of them.

respond() {
    # $1 = id, $2 = result JSON
    printf '{"jsonrpc":"2.0","id":%s,"result":%s}\n' "$1" "$2"
}

fail() {
    # $1 = id, $2 = code, $3 = message
    printf '{"jsonrpc":"2.0","id":%s,"error":{"code":%s,"message":"%s"}}\n' "$1" "$2" "$3"
}

INIT_RESULT='{"protocolVersion":"2024-11-05","capabilities":{"resources":{}},"serverInfo":{"name":"glob-mock","version":"1.0"}}'

LIST_P1='{"resources":[{"uri":"data://alpha.csv","name":"alpha","mimeType":"text/csv"},{"uri":"data://alpha.csv.bak","name":"alpha-backup","mimeType":"text/plain"},{"uri":"data://notes.txt","name":"notes","mimeType":"text/plain"}],"nextCursor":"GLOB-PAGE-2"}'
LIST_P2='{"resources":[{"uri":"data://beta.csv","name":"beta","mimeType":"text/csv"}]}'

read_result() {
    case "$1" in
    "data://alpha.csv")
        printf '%s' '{"contents":[{"uri":"data://alpha.csv","mimeType":"text/csv","text":"ALPHA"}]}'
        ;;
    "data://alpha.csv.bak")
        printf '%s' '{"contents":[{"uri":"data://alpha.csv.bak","mimeType":"text/plain","text":"ALPHA-BAK"}]}'
        ;;
    "data://notes.txt")
        printf '%s' '{"contents":[{"uri":"data://notes.txt","mimeType":"text/plain","text":"NOTES"}]}'
        ;;
    "data://beta.csv")
        printf '%s' '{"contents":[{"uri":"data://beta.csv","mimeType":"text/csv","text":"BETA"}]}'
        ;;
    *)
        printf '%s' ''
        ;;
    esac
}

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
        if [ "$cursor" = "GLOB-PAGE-2" ]; then
            respond "$id" "$LIST_P2"
        elif [ -z "$cursor" ]; then
            respond "$id" "$LIST_P1"
        else
            fail "$id" -32602 "Invalid cursor"
        fi
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
