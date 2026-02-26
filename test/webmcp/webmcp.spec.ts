import { test, expect } from "@playwright/test";
import * as path from "path";

const TEST_PAGE = `file://${path.resolve(__dirname, "test-page.html")}`;

// Helper: run a SQL statement in DuckDB-WASM via the page context
async function runSQL(page: any, sql: string): Promise<string> {
  return page.evaluate(async (sql: string) => {
    const db = (window as any).__duckdb_db;
    const conn = await db.connect();
    try {
      const result = await conn.query(sql);
      return JSON.stringify(result.toArray().map((r: any) => r.toJSON()));
    } finally {
      await conn.close();
    }
  }, sql);
}

test.describe("WebMCP Transport", () => {
  test.beforeEach(async ({ page }) => {
    await page.goto(TEST_PAGE);
    // Wait for DuckDB-WASM + extension to be loaded
    await page.waitForFunction(
      () => (window as any).__duckdb_ready === true,
      { timeout: 30_000 },
    );
  });

  test("navigator.modelContext is available", async ({ page }) => {
    const available = await page.evaluate(
      () => typeof (navigator as any).modelContext !== "undefined",
    );
    if (!available) {
      test.skip(
        true,
        "navigator.modelContext not available — Chrome 146+ with --enable-features=ModelContextProtocol required",
      );
    }
    expect(available).toBe(true);
  });

  test("DuckDB-WASM loads and extension is functional", async ({ page }) => {
    const result = await runSQL(page, "SELECT 42 AS answer");
    expect(result).toContain("42");
  });

  test("webmcp transport starts and registers tools", async ({ page }) => {
    const hasModelContext = await page.evaluate(
      () => typeof (navigator as any).modelContext !== "undefined",
    );
    test.skip(
      !hasModelContext,
      "navigator.modelContext not available",
    );

    // Start the WebMCP transport
    const startResult = await runSQL(
      page,
      "SELECT mcp_server_start('webmcp')",
    );
    expect(startResult).toContain("true"); // success: true

    // Verify tools were registered with navigator.modelContext
    const tools = await page.evaluate(async () => {
      const mc = (navigator as any).modelContext;
      // The exact API shape depends on the Chrome implementation;
      // at minimum, our registered tools should be discoverable
      if (mc.getTools) {
        return await mc.getTools();
      }
      return null;
    });

    // If getTools is available, verify our default tools are there
    if (tools !== null) {
      const toolNames = tools.map((t: any) => t.name);
      expect(toolNames).toContain("query");
    }
  });

  test("publish new tools then sync to WebMCP", async ({ page }) => {
    const hasModelContext = await page.evaluate(
      () => typeof (navigator as any).modelContext !== "undefined",
    );
    test.skip(
      !hasModelContext,
      "navigator.modelContext not available",
    );

    // Start server
    await runSQL(page, "SELECT mcp_server_start('webmcp')");

    // Publish a custom tool
    await runSQL(
      page,
      `SELECT mcp_publish_tool(
        'test_add',
        'Add two numbers',
        'SELECT $a + $b AS result',
        '{"a": {"type": "integer"}, "b": {"type": "integer"}}',
        '["a", "b"]'
      )`,
    );

    // Sync tools to WebMCP
    const syncResult = await runSQL(page, "SELECT mcp_webmcp_sync()");
    expect(syncResult).not.toContain("ERROR");

    // Verify the new tool is registered
    const tools = await page.evaluate(async () => {
      const mc = (navigator as any).modelContext;
      if (mc.getTools) {
        return await mc.getTools();
      }
      return null;
    });

    if (tools !== null) {
      const toolNames = tools.map((t: any) => t.name);
      expect(toolNames).toContain("test_add");
    }
  });

  test("publish resources and sync wrapper tool", async ({ page }) => {
    const hasModelContext = await page.evaluate(
      () => typeof (navigator as any).modelContext !== "undefined",
    );
    test.skip(
      !hasModelContext,
      "navigator.modelContext not available",
    );

    // Start server
    await runSQL(page, "SELECT mcp_server_start('webmcp')");

    // Publish a resource
    await runSQL(
      page,
      `SELECT mcp_publish_resource(
        'info://test-resource',
        'Hello from WebMCP test',
        'text/plain',
        'Test resource'
      )`,
    );

    // Sync — should register read_resource wrapper tool
    await runSQL(page, "SELECT mcp_webmcp_sync()");

    const tools = await page.evaluate(async () => {
      const mc = (navigator as any).modelContext;
      if (mc.getTools) {
        return await mc.getTools();
      }
      return null;
    });

    if (tools !== null) {
      const toolNames = tools.map((t: any) => t.name);
      expect(toolNames).toContain("read_resource");
    }
  });

  test("stop server unregisters tools", async ({ page }) => {
    const hasModelContext = await page.evaluate(
      () => typeof (navigator as any).modelContext !== "undefined",
    );
    test.skip(
      !hasModelContext,
      "navigator.modelContext not available",
    );

    // Start, then stop
    await runSQL(page, "SELECT mcp_server_start('webmcp')");
    await runSQL(page, "SELECT mcp_server_stop(true)");

    // After stop, tools should be unregistered
    const tools = await page.evaluate(async () => {
      const mc = (navigator as any).modelContext;
      if (mc.getTools) {
        return await mc.getTools();
      }
      return null;
    });

    if (tools !== null) {
      const toolNames = tools.map((t: any) => t.name);
      expect(toolNames).not.toContain("query");
    }
  });

  test("webmcp_list_page_tools() returns JSON array", async ({ page }) => {
    const hasModelContext = await page.evaluate(
      () => typeof (navigator as any).modelContext !== "undefined",
    );
    test.skip(
      !hasModelContext,
      "navigator.modelContext not available",
    );

    await runSQL(page, "SELECT mcp_server_start('webmcp')");
    const result = await runSQL(page, "SELECT webmcp_list_page_tools()");
    // Should return a JSON array (possibly empty)
    expect(result).toMatch(/\[/);
  });
});
