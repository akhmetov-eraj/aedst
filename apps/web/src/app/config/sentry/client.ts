import * as Sentry from "@sentry/nextjs";

import { sentrySharedOptions } from "@/app/config/sentry/shared";

export function initSentryClient() {
  Sentry.init({
    ...sentrySharedOptions,
    replaysSessionSampleRate: 0.1,
    replaysOnErrorSampleRate: 1,
    integrations: [Sentry.replayIntegration()],
  });
}
