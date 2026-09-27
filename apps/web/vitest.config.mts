import path from "node:path";
import { fileURLToPath } from "node:url";
import react from "@vitejs/plugin-react";
import tsconfigPaths from "vite-tsconfig-paths";
import { defineConfig } from "vitest/config";

const root = path.dirname(fileURLToPath(import.meta.url));

export default defineConfig({
  plugins: [tsconfigPaths(), react()],

  resolve: {
    alias: {
      "@": path.resolve(root, "src"),
      "@app": path.resolve(root, "src/app"),
    },
  },

  test: {
    environment: "jsdom",

    setupFiles: ["./vitest.setup.ts"],

    include: ["**/*.{test,spec}.{ts,tsx}"],

    exclude: ["node_modules", ".next", "e2e", "playwright"],

    globals: false,

    restoreMocks: true,
    clearMocks: true,
    mockReset: true,

    coverage: {
      provider: "v8",
      reporter: ["text", "html", "lcov"],
      reportsDirectory: "./coverage",
      exclude: ["**/*.d.ts", "**/*.config.*", "**/generated/**", "**/index.ts"],
    },
  },
});
