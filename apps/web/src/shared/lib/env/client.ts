import { z } from "zod";

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
const clientEnvSchema = z.object({
  // Canonical origin this app is served from — metadataBase, canonical
  // links, sitemap/robots, and absolute OG image URLs all derive from this
  // (see src/app/config/seo.ts). Must be the real public origin in every
  // deployed environment; the localhost default is dev-only.
  NEXT_PUBLIC_SITE_URL: z.url().default("http://localhost:3000"),

  // apps/server's ConnectRPC address (see apps/server's AEDST_HTTP_ADDR,
  // ":8080" by default). ConnectRPC's wire protocol is plain HTTP/1.1 +
  // JSON/protobuf (docs/content/decisions/0002-connectrpc.md), so the
  // browser talks to it directly — no gateway in between.
  NEXT_PUBLIC_API_BASE_URL: z.url().default("http://localhost:8080"),

  NEXT_PUBLIC_SENTRY_DSN: z.url().optional(),
  NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL: z.url().optional(),
});

export const clientEnv = clientEnvSchema.parse({
  NEXT_PUBLIC_SITE_URL: process.env.NEXT_PUBLIC_SITE_URL,
  NEXT_PUBLIC_API_BASE_URL: process.env.NEXT_PUBLIC_API_BASE_URL,
  NEXT_PUBLIC_SENTRY_DSN: process.env.NEXT_PUBLIC_SENTRY_DSN || undefined,
  NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL:
    process.env.NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL || undefined,
});
