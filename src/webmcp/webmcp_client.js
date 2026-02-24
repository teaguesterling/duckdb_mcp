/**
 * WebMCP Client Interceptor for DuckDB-WASM
 *
 * Load this script BEFORE other page scripts to intercept WebMCP tool
 * registrations and build a catalog that DuckDB-WASM can query.
 *
 * This patches navigator.modelContext methods to track what other scripts
 * on the page register, exposing it via window.__duckdb_webmcp_catalog.
 *
 * Usage:
 *   <script src="webmcp_client.js"></script>
 *   <script src="other-tool-provider.js"></script>
 *   <script>
 *     // DuckDB-WASM can now discover page tools:
 *     // SELECT webmcp_list_page_tools();
 *   </script>
 */
(function () {
  'use strict';

  if (typeof navigator === 'undefined' || !navigator.modelContext) {
    // WebMCP not available — nothing to intercept
    return;
  }

  var catalog = {};
  var originalRegister = navigator.modelContext.registerTool.bind(navigator.modelContext);
  var originalUnregister = navigator.modelContext.unregisterTool.bind(navigator.modelContext);
  var originalProvide = navigator.modelContext.provideContext
    ? navigator.modelContext.provideContext.bind(navigator.modelContext)
    : null;
  var originalClear = navigator.modelContext.clearContext
    ? navigator.modelContext.clearContext.bind(navigator.modelContext)
    : null;

  navigator.modelContext.registerTool = function (toolDef) {
    catalog[toolDef.name] = {
      name: toolDef.name,
      description: toolDef.description || '',
      inputSchema: toolDef.inputSchema || {},
      annotations: toolDef.annotations || {},
      execute: toolDef.execute
    };
    return originalRegister(toolDef);
  };

  navigator.modelContext.unregisterTool = function (name) {
    delete catalog[name];
    return originalUnregister(name);
  };

  if (originalProvide) {
    navigator.modelContext.provideContext = function (ctx) {
      // Replace catalog with what provideContext sets
      catalog = {};
      if (ctx && ctx.tools) {
        ctx.tools.forEach(function (t) {
          catalog[t.name] = {
            name: t.name,
            description: t.description || '',
            inputSchema: t.inputSchema || {},
            annotations: t.annotations || {},
            execute: t.execute
          };
        });
      }
      return originalProvide(ctx);
    };
  }

  if (originalClear) {
    navigator.modelContext.clearContext = function () {
      catalog = {};
      return originalClear();
    };
  }

  window.__duckdb_webmcp_catalog = {
    /** @returns {Array<{name: string, description: string, inputSchema: object}>} */
    listTools: function () {
      return Object.values(catalog).map(function (t) {
        return {
          name: t.name,
          description: t.description,
          inputSchema: t.inputSchema
        };
      });
    },

    /**
     * Call a page tool by name (async — returns a Promise).
     * Note: DuckDB-WASM cannot await this from C++ without ASYNCIFY.
     * This is exposed for JS-side usage.
     * @param {string} name
     * @param {object} args
     * @returns {Promise<any>}
     */
    callTool: async function (name, args) {
      var tool = catalog[name];
      if (!tool || !tool.execute) {
        throw new Error('Tool not found or not callable: ' + name);
      }
      return tool.execute(args);
    }
  };
})();
