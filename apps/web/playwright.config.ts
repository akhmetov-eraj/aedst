import { defineConfig, devices } from "@playwright/test";

const baseURL = "http://127.0.0.1:3100";

export default defineConfig({
  testDir: "./e2e",

  fullyParallel: true,

  forbidOnly: Boolean(process.env.CI),

  retries: process.env.CI ? 2 : 0,

  workers: process.env.CI ? 1 : undefined,

  reporter: process.env.CI
    ? [["line"], ["html", { open: "never" }]]
    : [["list"], ["html", { open: "never" }]],

  timeout: 30_000,

  expect: {
    timeout: 5_000,
  },

  use: {
    baseURL,

    trace: "retain-on-failure",
    screenshot: "only-on-failure",
    video: "retain-on-failure",

    actionTimeout: 10_000,
    navigationTimeout: 30_000,

    testIdAttribute: "data-testid",
  },

  webServer: {
    command: process.env.CI
      ? "pnpm start --port 3100"
      : "pnpm dev --port 3100",
    url: baseURL,
    timeout: 120_000,
    reuseExistingServer: !process.env.CI,
  },

  projects: [
    {
      name: "chromium",
      use: {
        ...devices["Desktop Chrome"],
      },
    },
  ],
});
