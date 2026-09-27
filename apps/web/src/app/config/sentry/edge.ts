import * as Sentry from "@sentry/nextjs";

import { sentrySharedOptions } from "@/app/config/sentry/shared";

export function initSentryEdge() {
  Sentry.init(sentrySharedOptions);
}
