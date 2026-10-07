"use client";

import { useTranslations } from "next-intl";

import { useConsent } from "@/shared/lib/consent/consent";
import { Button } from "@/shared/ui/button";

export function ConsentBanner() {
  const t = useTranslations("common.consent");
  const { grant, deny } = useConsent();

  return (
    <section
      aria-label={t("title")}
      className="fixed inset-x-4 bottom-4 z-50 mx-auto flex max-w-xl flex-col gap-3 rounded-xl border bg-background p-4 shadow-lg sm:flex-row sm:items-center"
    >
      {/* text-xs: the banner must never out-size the page heading, or it becomes
          the Largest Contentful Paint element (it renders after hydration). */}
      <p className="text-xs">{t("description")}</p>
      <div className="flex shrink-0 gap-2">
        <Button variant="outline" size="sm" onClick={deny}>
          {t("decline")}
        </Button>
        <Button size="sm" onClick={grant}>
          {t("accept")}
        </Button>
      </div>
    </section>
  );
}
