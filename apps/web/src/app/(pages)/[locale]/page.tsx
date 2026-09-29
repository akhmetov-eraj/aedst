import type { Metadata } from "next";
import { useTranslations } from "next-intl";
import { getTranslations } from "next-intl/server";

import { buildLanguageAlternates } from "@/app/config/seo";
import { AuthGate } from "@/modules/auth/ui/auth-gate";

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
    description: t("description"),
    alternates: {
      languages: buildLanguageAlternates(""),
    },
  };
}

export default function HomePage() {
  const t = useTranslations("home");

  return (
    <>
      <h1>{t("title")}</h1>
      <p>{t("description")}</p>

      <AuthGate />
    </>
  );
}
