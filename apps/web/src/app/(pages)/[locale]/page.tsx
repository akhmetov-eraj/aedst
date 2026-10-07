import type { Metadata } from "next";
import { getTranslations, setRequestLocale } from "next-intl/server";
import { type Locale, toLanguageTag } from "@/app/config/i18n/locales";
import { JsonLd } from "@/app/config/json-ld";
import { buildLanguageAlternates, siteConfig } from "@/app/config/seo";
import { AuthGate } from "@/modules/auth/ui/AuthGate";
import { safeRedirectPath } from "@/shared/lib/auth/safe-redirect";

export async function generateMetadata({
  params,
}: PageProps<"/[locale]">): Promise<Metadata> {
  const { locale } = await params;
  const [t, tCommon] = await Promise.all([
    getTranslations({ locale, namespace: "home" }),
    getTranslations({ locale, namespace: "common" }),
  ]);

  return {
    // Composed explicitly, not via the layout's `title.template`: per
    // Next's metadata docs, a layout's template only applies to *nested*
    // segments, never to a page in the same segment as the layout that
    // defines it — and this page is a direct child of
    // `(pages)/[locale]/layout.tsx`. The template still exists for when a
    // real nested route (e.g. `[locale]/settings`) ships.
    title: `${t("title")} · ${tCommon("appName")}`,
    description: tCommon("appDescription"),
    alternates: {
      languages: buildLanguageAlternates(""),
    },
  };
}

export default async function HomePage({
  params,
  searchParams,
}: PageProps<"/[locale]">) {
  const { locale } = await params;
  setRequestLocale(locale);

  const [t, tCommon, query] = await Promise.all([
    getTranslations({ locale, namespace: "home" }),
    getTranslations({ locale, namespace: "common" }),
    searchParams,
  ]);
  // `proxy.ts` puts the page the visitor was headed to in `?next=`; validated
  // here too, so a crafted link cannot turn sign-in into an open redirect.
  const next = typeof query.next === "string" ? query.next : undefined;

  return (
    <>
      <JsonLd
        data={{
          "@context": "https://schema.org",
          "@type": "WebApplication",
          name: tCommon("appName"),
          description: tCommon("appDescription"),
          url: new URL(`/${locale}`, siteConfig.url).toString(),
          inLanguage: toLanguageTag(locale as Locale),
          applicationCategory: "ProductivityApplication",
          operatingSystem: "Any",
        }}
      />
      <div className="mx-auto flex w-full max-w-sm flex-1 flex-col justify-center gap-6 p-6">
        <header className="flex flex-col gap-2">
          <h1 className="font-semibold text-3xl tracking-tight">
            {t("title")}
          </h1>
          <p className="text-muted-foreground">{t("description")}</p>
        </header>

        <AuthGate redirectTo={next ? safeRedirectPath(next) : undefined} />
      </div>
    </>
  );
}
