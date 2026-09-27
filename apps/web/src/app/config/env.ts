import { z } from "zod";

const envSchema = z.object({
  NEXT_PUBLIC_GRAPHQL_ENDPOINT: z
    .url()
    .default("http://localhost:4000/graphql"),
  NEXT_PUBLIC_REST_API_URL: z.url().default("http://localhost:4000/api"),
  NEXT_PUBLIC_SENTRY_DSN: z.url().optional(),
  NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL: z.url().optional(),
});

export const env = envSchema.parse({
  NEXT_PUBLIC_GRAPHQL_ENDPOINT: process.env.NEXT_PUBLIC_GRAPHQL_ENDPOINT,
  NEXT_PUBLIC_REST_API_URL: process.env.NEXT_PUBLIC_REST_API_URL,
  NEXT_PUBLIC_SENTRY_DSN: process.env.NEXT_PUBLIC_SENTRY_DSN || undefined,
  NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL:
    process.env.NEXT_PUBLIC_BETTER_UPTIME_HEARTBEAT_URL || undefined,
});
