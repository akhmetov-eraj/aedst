import { withSentryConfig } from "@sentry/nextjs/config";
import type { NextConfig } from "next";
import createNextIntlPlugin from "next-intl/plugin";

const withNextIntl = createNextIntlPlugin("./src/app/config/i18n/request.ts");

const nextConfig: NextConfig = {
  /* config options here */
  reactCompiler: true,
};

export default withSentryConfig(withNextIntl(nextConfig), {
  org: process.env.SENTRY_ORG,
  project: process.env.SENTRY_PROJECT,
  authToken: process.env.SENTRY_AUTH_TOKEN,
  // Without an auth token there's nothing to authenticate the upload with,
  // so skip it entirely instead of failing (or noisily warning) local builds.
  sourcemaps: {
    disable: !process.env.SENTRY_AUTH_TOKEN,
  },
  silent: !process.env.CI,
});
