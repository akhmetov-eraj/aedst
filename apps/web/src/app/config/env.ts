import { z } from "zod";

const envSchema = z.object({
  // apps/server's ConnectRPC address (see apps/server/internal/platform/config
  // and cmd/server/main.go — AEDST_HTTP_ADDR, ":8080" by default). ConnectRPC's
  // wire protocol is plain HTTP/1.1 + JSON/protobuf (docs/decisions/0002-connectrpc.md),
  // so the browser talks to it directly, no gateway in between.
  NEXT_PUBLIC_API_URL: z.url().default("http://localhost:8080"),
  NEXT_PUBLIC_SENTRY_DSN: z.url().optional(),
  NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL: z.url().optional(),
});

export const env = envSchema.parse({
  NEXT_PUBLIC_API_URL: process.env.NEXT_PUBLIC_API_URL,
  NEXT_PUBLIC_SENTRY_DSN: process.env.NEXT_PUBLIC_SENTRY_DSN || undefined,
  NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL:
    process.env.NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL || undefined,
});
