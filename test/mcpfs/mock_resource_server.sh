#!/bin/sh
# Mock MCP server used by test/sql/mcpfs_resource_content.test (issue #67).
#
# Speaks just enough JSON-RPC 2.0 over stdio to answer `initialize`,
# `resources/list` and `resources/read`. Every resource URI returns a
# deliberately shaped `resources/read` payload that exercises one JSON edge
# case the old hand-rolled `"text":"` string scan got wrong.
#
# Responses are emitted with printf so backslash escapes are passed through
# verbatim -- the payloads below are literal JSON.

respond() {
    # $1 = id, $2 = result JSON
    printf '{"jsonrpc":"2.0","id":%s,"result":%s}\n' "$1" "$2"
}

INIT_RESULT='{"protocolVersion":"2024-11-05","capabilities":{"resources":{}},"serverInfo":{"name":"mcpfs-mock","version":"1.0"}}'

LIST_RESULT='{"resources":[{"uri":"case://plain","name":"plain","mimeType":"text/plain"},{"uri":"case://spaced","name":"spaced","mimeType":"text/plain"},{"uri":"case://unicode","name":"unicode","mimeType":"text/plain"},{"uri":"case://decoy","name":"decoy","mimeType":"text/plain"},{"uri":"case://escapes","name":"escapes","mimeType":"text/plain"},{"uri":"case://multi","name":"multi","mimeType":"text/plain"},{"uri":"case://blob","name":"blob","mimeType":"application/octet-stream"}]}'

read_result() {
    case "$1" in
    # Baseline: compact, single text part. Worked before and must keep working.
    "case://plain")
        printf '%s' '{"contents":[{"uri":"case://plain","mimeType":"text/plain","text":"PLAIN-OK"}]}'
        ;;
    # Whitespace after the colons (what json.dumps produces by default).
    # The old scan looked for the literal `"text":"` and found nothing, so it
    # handed back the entire JSON-RPC result as the file body.
    "case://spaced")
        printf '%s' '{"contents": [{"uri": "case://spaced", "mimeType": "text/plain", "text": "SPACED-OK"}]}'
        ;;
    # \uXXXX escapes: the old unescaper only knew \n \t \r \\ \" and passed
    # the \u00e9 escape through literally.
    "case://unicode")
        printf '%s' '{"contents":[{"uri":"case://unicode","mimeType":"text/plain","text":"caf\u00e9-OK"}]}'
        ;;
    # A `text` key that is NOT the content field appears earlier in the
    # payload. The old scan took the first `"text":"` it saw and returned the
    # decoy.
    "case://decoy")
        printf '%s' '{"contents":[{"annotations":{"text":"DECOY"},"uri":"case://decoy","mimeType":"text/plain","text":"REAL-OK"}]}'
        ;;
    # Escapes the old unescaper mangled: \/ was emitted as a literal backslash
    # followed by a slash.
    "case://escapes")
        printf '%s' '{"contents":[{"uri":"case://escapes","mimeType":"text/plain","text":"a\/b\"q\"\\z"}]}'
        ;;
    # Multiple content parts: the old scan stopped after the first one.
    "case://multi")
        printf '%s' '{"contents":[{"uri":"case://multi","mimeType":"text/plain","text":"PART-ONE"},{"uri":"case://multi#2","mimeType":"text/plain","text":"PART-TWO"}]}'
        ;;
    # Binary content part (base64). The old scan found no "text" at all and
    # returned the raw JSON. "QkxPQi1PSw==" is "BLOB-OK".
    "case://blob")
        printf '%s' '{"contents":[{"uri":"case://blob","mimeType":"application/octet-stream","blob":"QkxPQi1PSw=="}]}'
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
            printf '{"jsonrpc":"2.0","id":%s,"error":{"code":-32002,"message":"Resource not found: %s"}}\n' "$id" "$uri"
        else
            respond "$id" "$result"
        fi
        ;;
    *)
        printf '{"jsonrpc":"2.0","id":%s,"error":{"code":-32601,"message":"Method not found"}}\n' "$id"
        ;;
    esac
done
