import * as Sentry from "@sentry/nextjs";

import { sentrySharedOptions } from "@/app/config/sentry/shared";

export function initSentryServer() {
  Sentry.init(sentrySharedOptions);
}
