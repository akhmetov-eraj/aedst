import * as z from "zod/mini";

// Every var here is a `NEXT_PUBLIC_*` var: Next.js inlines these into the
// client bundle at build time, so nothing in this schema may ever hold a
// secret — that's the one invariant this file must keep.
//
// Deliberately NOT guarded with `import "client-only"`: per Next's own docs
// (data-security.md / server-and-client-components.md), that package is for
// modules with genuinely browser-only logic (code touching `window`, etc.),
// not for env access — `NEXT_PUBLIC_*` vars are safe and meant to be read
// from the server too (e.g. `config/monitoring/better-uptime.ts`'s heartbeat
// is called from a server-side cron route). Guarding this file would make
// that import throw at build time for no security benefit, since these
// values were never secret to begin with. The asymmetric guard is
// intentional: only `./server.ts` (real secrets) gets `server-only`.

/** A 0..1 ratio given as a string env var ("0.2"). */
const sampleRate = (fallback: number) =>
  z._default(z.coerce.number().check(z.gte(0), z.lte(1)), fallback);

const clientEnvSchema = z.object({
  // Canonical origin this app is served from — metadataBase, canonical
  // links, sitemap/robots, and absolute OG image URLs all derive from this
  // (see src/app/config/seo.ts). Must be the real public origin in every
  // deployed environment; the localhost default is dev-only.
  NEXT_PUBLIC_SITE_URL: z._default(z.url(), "http://localhost:3000"),

  NEXT_PUBLIC_SENTRY_DSN: z.optional(z.url()),
  // Defaults to NODE_ENV when unset (see app/config/sentry/shared.ts).
  NEXT_PUBLIC_SENTRY_ENVIRONMENT: z.optional(z.string()),
  // Share of transactions traced. 1 locally; size it to traffic in prod.
  NEXT_PUBLIC_SENTRY_TRACES_SAMPLE_RATE: sampleRate(
    process.env.NODE_ENV === "production" ? 0.2 : 1,
  ),
  // Session Replay (only ever after analytics consent): share of all
  // sessions, and share of sessions with an error.
  NEXT_PUBLIC_SENTRY_REPLAY_SESSION_SAMPLE_RATE: sampleRate(0.1),
  NEXT_PUBLIC_SENTRY_REPLAY_ON_ERROR_SAMPLE_RATE: sampleRate(1),

  // PostHog product analytics + feature flags. Empty key = PostHog is never
  // loaded. HOST is the PostHog *ingestion* host (EU or US cloud, or
  // self-hosted); the browser never calls it directly — it posts to this
  // app's `/ingest` rewrite (next.config.ts), so no CSP origin is needed.
  NEXT_PUBLIC_POSTHOG_KEY: z.optional(z.string().check(z.minLength(1))),
  NEXT_PUBLIC_POSTHOG_HOST: z._default(z.url(), "https://eu.i.posthog.com"),

  NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL: z.optional(z.url()),
});

export const clientEnv = clientEnvSchema.parse({
  NEXT_PUBLIC_SITE_URL: process.env.NEXT_PUBLIC_SITE_URL,
  NEXT_PUBLIC_SENTRY_DSN: process.env.NEXT_PUBLIC_SENTRY_DSN || undefined,
  NEXT_PUBLIC_SENTRY_ENVIRONMENT:
    process.env.NEXT_PUBLIC_SENTRY_ENVIRONMENT || undefined,
  NEXT_PUBLIC_SENTRY_TRACES_SAMPLE_RATE:
    process.env.NEXT_PUBLIC_SENTRY_TRACES_SAMPLE_RATE || undefined,
  NEXT_PUBLIC_SENTRY_REPLAY_SESSION_SAMPLE_RATE:
    process.env.NEXT_PUBLIC_SENTRY_REPLAY_SESSION_SAMPLE_RATE || undefined,
  NEXT_PUBLIC_SENTRY_REPLAY_ON_ERROR_SAMPLE_RATE:
    process.env.NEXT_PUBLIC_SENTRY_REPLAY_ON_ERROR_SAMPLE_RATE || undefined,
  NEXT_PUBLIC_POSTHOG_KEY: process.env.NEXT_PUBLIC_POSTHOG_KEY || undefined,
  NEXT_PUBLIC_POSTHOG_HOST: process.env.NEXT_PUBLIC_POSTHOG_HOST || undefined,
  NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL:
    process.env.NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL || undefined,
});
