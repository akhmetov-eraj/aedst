import { clientEnv } from "@/shared/lib/env/client";

const isProduction = process.env.NODE_ENV === "production";

export const sentrySharedOptions = {
  dsn: clientEnv.NEXT_PUBLIC_SENTRY_DSN,
  environment: process.env.VERCEL_ENV ?? process.env.NODE_ENV,
  // 100% locally/preview (low volume, worth seeing everything). Sampled in
  // production so trace volume scales with real traffic instead of billing
  // linearly with it — revisit the ratio once there's real traffic to size
  // it against.
  tracesSampleRate: isProduction ? 0.2 : 1,
  debug: false,
  // Well-known noise that isn't actionable: a benign browser quirk
  // (ResizeObserver) and rejections that never had a real Error object to
  // begin with (usually a third-party script, not our code).
  ignoreErrors: [
    "ResizeObserver loop limit exceeded",
    "ResizeObserver loop completed with undelivered notifications",
    "Non-Error promise rejection captured",
  ] as string[],
};
