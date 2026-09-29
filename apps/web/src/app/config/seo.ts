import { defaultLocale, locales } from "@/app/config/i18n/locales";
import { clientEnv } from "@/shared/env/client";

/**
 * Central SEO/branding constants. Everything metadata-related (root
 * metadata in the locale layout, sitemap.ts, robots.ts, manifest.ts,
 * opengraph-image.tsx) reads from here instead of hardcoding the app name
 * or origin a second time.
 */
export const siteConfig = {
  name: "iman",
  // Kept intentionally short and generic — the product doesn't have
  // marketing copy yet (see src/modules/home/i18n/*.json's placeholder
  // strings). Update alongside real copy once it exists; every locale's
  // `common.appDescription` key is what generateMetadata actually reads.
  description: "Sign in to iman.",
  url: new URL(clientEnv.NEXT_PUBLIC_SITE_URL),
  // shadcn's neutral theme (see src/app/styles/globals.css) — pure
  // white/near-black, not a placeholder brand color.
  themeColor: { light: "#ffffff", dark: "#0a0a0a" },
} as const;

/**
 * `hreflang` alternates for a locale-prefixed pathname (routing.localePrefix
 * is "always", so every locale — including the default — is prefixed).
 * `pathname` excludes the locale segment and any leading slash — `""` for
 * the home page, `"about"` for `/about`. (Not `"/"`/`"/about"`: `/en/` is a
 * 308 redirect to `/en` here, same as any Next.js route without a trailing
 * slash — this avoids ever generating the redirecting form.)
 */
export function buildLanguageAlternates(pathname: string) {
  const suffix = pathname ? `/${pathname}` : "";
  const languages: Record<string, string> = {};
  for (const locale of locales) {
    languages[locale] = new URL(
      `/${locale}${suffix}`,
      siteConfig.url,
    ).toString();
  }
  // next-intl's `localeDetection` negotiates a locale for `/`, so an
  // x-default pointing at the default locale's URL is the closest
  // equivalent for crawlers that don't do content negotiation.
  languages["x-default"] = new URL(
    `/${defaultLocale}${suffix}`,
    siteConfig.url,
  ).toString();
  return languages;
}
