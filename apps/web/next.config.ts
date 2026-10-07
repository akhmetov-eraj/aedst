import { withSentryConfig } from "@sentry/nextjs/config";
import { withSerwist } from "@serwist/turbopack";
import type { NextConfig } from "next";
import createNextIntlPlugin from "next-intl/plugin";

const withNextIntl = createNextIntlPlugin("./src/app/config/i18n/request.ts");

// The page CSP is per-request (nonce-based) and lives in `src/proxy.ts` /
// `src/app/config/security/csp.ts`. Only headers that are the same for every
// response are static here.
const securityHeaders = [
  { key: "X-Content-Type-Options", value: "nosniff" },
  { key: "X-Frame-Options", value: "DENY" },
  { key: "Referrer-Policy", value: "strict-origin-when-cross-origin" },
  {
    key: "Permissions-Policy",
    // Camera stays available to this origin only: the media uploader's
    // file picker can offer "take a photo" on phones.
    value:
      "camera=(self), microphone=(), geolocation=(), payment=(), usb=(), interest-cohort=(), browsing-topics=()",
  },
  {
    key: "Strict-Transport-Security",
    value: "max-age=63072000; includeSubDomains; preload",
  },
  { key: "Cross-Origin-Opener-Policy", value: "same-origin" },
  { key: "Cross-Origin-Resource-Policy", value: "same-origin" },
  { key: "X-DNS-Prefetch-Control", value: "off" },
];

/**
 * PostHog reverse proxy: the browser posts to `/ingest/*` on this origin and
 * Next forwards it to PostHog, so ad blockers that block PostHog's hostnames
 * do not drop events and the CSP needs no PostHog origin. Only wired when a
 * project key is configured.
 */
function posthogRewrites() {
  if (!process.env.NEXT_PUBLIC_POSTHOG_KEY) {
    return [];
  }
  const ingest = new URL(
    process.env.NEXT_PUBLIC_POSTHOG_HOST || "https://eu.i.posthog.com",
  );
  const assets = new URL(ingest);
  assets.hostname = ingest.hostname.replace(
    ".i.posthog.com",
    "-assets.i.posthog.com",
  );
  return [
    {
      source: "/ingest/static/:path*",
      destination: `${assets.origin}/static/:path*`,
    },
    {
      source: "/ingest/array/:path*",
      destination: `${assets.origin}/array/:path*`,
    },
    { source: "/ingest/:path*", destination: `${ingest.origin}/:path*` },
  ];
}

const nextConfig: NextConfig = {
  reactCompiler: true,
  // The Playwright suite drives the dev server through 127.0.0.1; Next blocks
  // dev-only resources (HMR) from origins not listed here.
  allowedDevOrigins: ["127.0.0.1"],
  poweredByHeader: false,
  // PostHog's API paths end in a slash (`/e/`, `/flags/?v=2`); Next's
  // trailing-slash redirect would otherwise break the `/ingest` proxy.
  skipTrailingSlashRedirect: true,
  async rewrites() {
    return posthogRewrites();
  },
  async headers() {
    return [
      { source: "/:path*", headers: securityHeaders },
      {
        // JSON responses (the /api/rpc proxy) never render anything, so the
        // strictest possible policy costs nothing.
        source: "/api/:path*",
        headers: [
          {
            key: "Content-Security-Policy",
            value: "default-src 'none'; frame-ancestors 'none'",
          },
          { key: "Cache-Control", value: "no-store" },
        ],
      },
      {
        // The service worker must never be served stale (an old worker
        // would keep serving an old app), and it only ever runs our code.
        source: "/serwist/:path*",
        headers: [
          {
            key: "Cache-Control",
            value: "no-cache, no-store, must-revalidate",
          },
          {
            key: "Content-Security-Policy",
            value: "default-src 'self'; script-src 'self'",
          },
        ],
      },
    ];
  },
};

export default withSentryConfig(withSerwist(withNextIntl(nextConfig)), {
  org: process.env.SENTRY_ORG,
  project: process.env.SENTRY_PROJECT,
  authToken: process.env.SENTRY_AUTH_TOKEN,
  // Release = commit SHA in CI (GITHUB_SHA) unless SENTRY_RELEASE overrides
  // it; the SDK injects the same value at runtime, which is what ties an
  // event to its uploaded source maps and to "suspect commits".
  release: {
    name:
      process.env.SENTRY_RELEASE ||
      process.env.GITHUB_SHA ||
      process.env.VERCEL_GIT_COMMIT_SHA ||
      undefined,
  },
  sourcemaps: {
    // Without an auth token there's nothing to authenticate the upload with,
    // so skip it entirely instead of failing (or noisily warning) local builds.
    disable: !process.env.SENTRY_AUTH_TOKEN,
    // Source maps are uploaded to Sentry, then deleted from the build output,
    // so they are never served publicly.
    deleteSourcemapsAfterUpload: true,
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
  // Replay's iframe/shadow-DOM recording is unused here; dropping it trims the
  // (lazy, consent-gated) replay chunk.
  bundleSizeOptimizations: {
    excludeReplayIframe: true,
    excludeReplayShadowDom: true,
  },
});
