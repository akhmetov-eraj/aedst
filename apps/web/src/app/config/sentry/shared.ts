import { env } from "@/app/config/env";

export const sentrySharedOptions = {
  dsn: env.NEXT_PUBLIC_SENTRY_DSN,
  tracesSampleRate: 1,
  debug: false,
} as const;
