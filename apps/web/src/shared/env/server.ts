import "server-only";
import { z } from "zod";

// `import "server-only"` makes the build fail immediately if a Client
// Component ever imports this file — the only thing standing between a
// future mistake and a leaked secret in the browser bundle. Never weaken
// this file to also export client vars "for convenience"; import
// `clientEnv` from `./client` separately wherever both are needed
// server-side (that direction is always safe).
const serverEnvSchema = z.object({
  // Server-to-server address for apps/server. The browser never uses it: it
  // calls this app's same-origin `/api/rpc` BFF route, and only server code
  // (Server Components, Server Actions, Route Handlers, proxy.ts) reaches the
  // API at this address — in production an internal service address.
  // (The session cookie name, AUTH_COOKIE_NAME, and the roles claim,
  // AUTH_ROLES_CLAIM, are read directly by shared/lib/auth because proxy.ts
  // needs them too.)
  API_BASE_URL: z.url().default("http://localhost:8080"),

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
  SENTRY_AUTH_TOKEN: process.env.SENTRY_AUTH_TOKEN || undefined,
});
