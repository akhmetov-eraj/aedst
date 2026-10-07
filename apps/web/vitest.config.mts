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
      "server-only": path.resolve(root, "src/test/mocks/empty-module.ts"),
    },
  },

  test: {
    environment: "jsdom",

    setupFiles: ["./vitest.setup.ts"],

    include: ["**/*.{test,spec}.{ts,tsx}"],

    exclude: ["node_modules", ".next", "e2e", "playwright"],

    globals: false,

    // Component tests do real Connect round trips (through MSW) and real
    // user-event typing; 5 s is too tight on a loaded runner with coverage.
    testTimeout: 15_000,

    // next-intl imports `next/server` without a file extension, which Node's ESM
    // loader rejects; inlining lets Vite resolve it (needed by proxy.test.ts).
    server: { deps: { inline: [/next-intl/] } },

    restoreMocks: true,
    clearMocks: true,
    mockReset: true,

    env: {
      API_BASE_URL: "http://localhost:8080",
      NEXT_PUBLIC_SITE_URL: "http://localhost:3000",
      AUTH_COOKIE_NAME: "aedst_session",
    },

    coverage: {
      provider: "v8",
      reporter: ["text", "html", "lcov"],
      reportsDirectory: "./coverage",
      // Measured over hand-written code only: vendored shadcn primitives,
      // generated protobuf stubs, test helpers and framework glue that only a
      // running Next server can exercise (covered by Playwright instead).
      include: ["src/**/*.{ts,tsx}"],
      exclude: [
        "**/*.d.ts",
        "**/*.config.*",
        "**/generated/**",
        "**/index.ts",
        "**/*.test.{ts,tsx}",
        "src/shared/ui/**",
        "src/test/**",
        "src/instrumentation*.ts",
        "src/app/config/sentry/**",
        "src/app/config/monitoring/**",
        "src/app/**/opengraph-image.tsx",
        "src/app/manifest.ts",
        "src/app/sitemap.ts",
        // Framework glue that only a running Next server can execute — layouts,
        // loading states, async route pages (their sections are unit-tested),
        // font loaders and next-intl wiring. Covered end to end by Playwright.
        "**/layout.tsx",
        "**/loading.tsx",
        // Wildcards, not literal "(pages)/[locale]/(app)": parentheses and
        // brackets are glob syntax.
        "src/app/*pages*/*locale*/*app*/**/page.tsx",
        "src/app/*pages*/*locale*/forbidden/page.tsx",
        // Runs inside the service worker / needs a registered worker, and the
        // build-time asset routes: exercised by Playwright (e2e/pwa.spec.ts).
        "src/app/sw.ts",
        "src/app/serwist/**",
        "src/app/icons/**",
        "src/app/providers/pwa-provider.tsx",
        "src/app/config/fonts.ts",
        "src/app/config/i18n/{messages,modules,navigation,request,routing}.ts",
      ],
      // Ratchet: set just under the measured baseline so coverage can only go
      // up. Raise these as tests are added; never lower them to make CI pass.
      thresholds: {
        lines: 88,
        functions: 83,
        branches: 84,
        statements: 88,
      },
    },
  },
});
