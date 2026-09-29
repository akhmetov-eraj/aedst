import createMiddleware from "next-intl/middleware";

import { routing } from "@/app/config/i18n/routing";

export default createMiddleware(routing);

export const config = {
  // `monitoring` excluded: Sentry's `tunnelRoute` (next.config.ts) proxies
  // client SDK events through that path — if this middleware ran on it,
  // next-intl would 30x-redirect it to add a locale prefix and break the
  // tunnel (see next.config.ts's `tunnelRoute` comment).
  matcher: ["/((?!api|trpc|_next|_vercel|monitoring|.*\\..*).*)"],
};
