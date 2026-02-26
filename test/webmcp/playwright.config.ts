import { defineConfig } from "@playwright/test";

export default defineConfig({
  testDir: ".",
  testMatch: "*.spec.ts",
  timeout: 60_000,
  retries: 0,
  workers: 1, // Serial — DuckDB-WASM is heavy
  use: {
    headless: true,
    viewport: { width: 1280, height: 720 },
    launchOptions: {
      args: [
        // Enable the experimental WebMCP / Model Context Protocol flag
        "--enable-features=ModelContextProtocol",
      ],
    },
  },
  projects: [
    {
      name: "chromium",
      use: {
        browserName: "chromium",
        channel: "chrome",
      },
    },
  ],
});
