import type { MetadataRoute } from "next";

import { locales } from "@/app/config/i18n/locales";
import { buildLanguageAlternates, siteConfig } from "@/app/config/seo";

// One entry per locale for every route (just `/` today — add a new
// `{ pathname, changeFrequency, priority }` to `routes` below as real pages
// ship, the locale fan-out and hreflang alternates stay automatic).
const routes: Array<{
  // Excludes the locale segment and any leading slash — see
  // buildLanguageAlternates's doc comment in config/seo.ts.
  pathname: string;
  changeFrequency: MetadataRoute.Sitemap[number]["changeFrequency"];
  priority: number;
}> = [{ pathname: "", changeFrequency: "weekly", priority: 1 }];

export default function sitemap(): MetadataRoute.Sitemap {
  return routes.flatMap(({ pathname, changeFrequency, priority }) =>
    locales.map((locale) => ({
      url: new URL(
        `/${locale}${pathname ? `/${pathname}` : ""}`,
        siteConfig.url,
      ).toString(),
      lastModified: new Date(),
      changeFrequency,
      priority,
      alternates: { languages: buildLanguageAlternates(pathname) },
    })),
  );
}
