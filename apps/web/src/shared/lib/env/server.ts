import "server-only";
import { z } from "zod";

// `import "server-only"` makes the build fail immediately if a Client
// Component ever imports this file — the only thing standing between a
// future mistake and a leaked secret in the browser bundle. Never weaken
// this file to also export client vars "for convenience"; import
// `clientEnv` from `./client` separately wherever both are needed
// server-side (that direction is always safe).
const serverEnvSchema = z.object({
  // Server-to-server address for apps/server, used by anything that calls
  // the API from a Server Component/Route Handler instead of the browser
  // (e.g. a future server action) — kept distinct from
  // `NEXT_PUBLIC_API_BASE_URL` because production points them at different
  // hosts (an internal service address here vs. the public edge there).
  API_BASE_URL: z.url().default("http://localhost:8080"),

  // Not read yet — reserved for the session cookie once server-side auth
  // lands (see docs/content/decisions/0002-connectrpc.md and
  // apps/server/internal/platform/auth). Named now so the cookie name is
  // one env-driven constant from day one instead of a string literal
  // scattered across auth code later.
  AUTH_COOKIE_NAME: z.string().min(1).default("aedst_session"),

  // Sentry source-map upload during `next build` (see next.config.ts's
  // withSentryConfig call) reads this directly via `process.env`, not
  // through this schema — next.config.ts runs before the app boots, so it
  // can't import an app module. It's validated here too so any *runtime*
  // code that needs it (there is none today) gets the same
  // fail-fast-on-missing-value guarantee as every other server var.
  SENTRY_AUTH_TOKEN: z.string().optional(),
});

export const serverEnv = serverEnvSchema.parse({
  API_BASE_URL: process.env.API_BASE_URL,
  AUTH_COOKIE_NAME: process.env.AUTH_COOKIE_NAME,
  SENTRY_AUTH_TOKEN: process.env.SENTRY_AUTH_TOKEN || undefined,
});
