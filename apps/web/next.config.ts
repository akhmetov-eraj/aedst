import { withSentryConfig } from "@sentry/nextjs/config";
import type { NextConfig } from "next";
import createNextIntlPlugin from "next-intl/plugin";

const withNextIntl = createNextIntlPlugin("./src/app/config/i18n/request.ts");

const nextConfig: NextConfig = {
  /* config options here */
  reactCompiler: true,
};

export default withSentryConfig(withNextIntl(nextConfig), {
  org: process.env.SENTRY_ORG,
  project: process.env.SENTRY_PROJECT,
  authToken: process.env.SENTRY_AUTH_TOKEN,
  // Without an auth token there's nothing to authenticate the upload with,
  // so skip it entirely instead of failing (or noisily warning) local builds.
  sourcemaps: {
    disable: !process.env.SENTRY_AUTH_TOKEN,
  },
  silent: !process.env.CI,
  // Readable stack traces through Next.js-internal frames too, at the cost
  // of a longer source-map upload — worth it while this is still a small app.
  widenClientFileUpload: true,
  // Routes the client SDK's beacon through our own server instead of
  // Sentry's ingest domain directly, so ad blockers that block known
  // Sentry hostnames don't silently drop events. Requires `src/proxy.ts`'s
  // matcher to exclude this path (next-intl's middleware would otherwise
  // 30x-redirect it to add a locale prefix, breaking the tunnel) — see the
  // `monitoring` exclusion there.
  tunnelRoute: "/monitoring",
});
