import type { MetadataRoute } from "next";

import { locales } from "@/app/config/i18n/locales";
import { siteConfig } from "@/app/config/seo";

export default function robots(): MetadataRoute.Robots {
  return {
    rules: {
      userAgent: "*",
      allow: "/",
      // Signed-in, per-user pages and the BFF proxy have nothing to index.
      // (The pages also carry `noindex`; this saves crawl budget.)
      disallow: ["/api/", ...locales.map((locale) => `/${locale}/tasks`)],
    },
    sitemap: new URL("/sitemap.xml", siteConfig.url).toString(),
  };
}
