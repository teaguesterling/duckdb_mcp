# WebMCP Transport Browser Tests

Playwright-based browser tests for the DuckDB MCP extension's WebMCP transport,
which registers tools with the browser's `navigator.modelContext` API.

## Prerequisites

- **Chrome 146+** (or Chromium nightly) with WebMCP support
- **Node.js 18+**
- **WASM extension build** — the DuckDB MCP extension compiled for Emscripten
  (built via `make wasm` or CI's WASM matrix)

## Setup

```bash
cd test/webmcp
npm install
npx playwright install chromium
```

## Running

```bash
# Headless (default)
npx playwright test

# Headed (see the browser)
npx playwright test --headed
```

## How It Works

The tests use Playwright to launch Chromium with `--enable-features=ModelContextProtocol`,
load a test page (`test-page.html`) that initializes DuckDB-WASM and loads the MCP
extension, then exercise the WebMCP transport through SQL commands.

Tests that require `navigator.modelContext` will skip gracefully if the API is not
available (e.g., Chrome version too old or flag not enabled).

## Test Coverage

| Test | What it verifies |
|------|-----------------|
| `navigator.modelContext is available` | Chrome flag is active |
| `DuckDB-WASM loads` | Extension loads in browser |
| `webmcp transport starts` | `mcp_server_start('webmcp')` succeeds, tools registered |
| `publish then sync` | New tools appear after `mcp_webmcp_sync()` |
| `publish resources` | `read_resource` wrapper tool registered |
| `stop unregisters tools` | `mcp_server_stop()` cleans up |
| `webmcp_list_page_tools()` | Returns JSON from page catalog |

## Notes

- These tests are **not yet in CI** — they require a Chrome build with WebMCP support
- The `test-page.html` loads DuckDB-WASM from CDN; for local extension testing,
  you may need to side-load the WASM extension bundle
- The WebMCP API shape (`navigator.modelContext.getTools()`, etc.) is experimental
  and may change as the Chrome implementation evolves
