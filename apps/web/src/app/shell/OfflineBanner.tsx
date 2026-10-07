"use client";

import { useTranslations } from "next-intl";

import { useOnlineStatus } from "@/shared/lib/hooks/use-online-status";

/** Tells the user why things stopped loading — announced politely. */
export function OfflineBanner() {
  const t = useTranslations("common.offline");
  const online = useOnlineStatus();

  return (
    <div role="status" aria-live="polite">
      {!online && (
        <p className="bg-amber-100 px-4 py-2 text-center text-amber-950 text-sm dark:bg-amber-950 dark:text-amber-100">
          {t("banner")}
        </p>
      )}
    </div>
  );
}
