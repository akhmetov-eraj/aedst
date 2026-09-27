"use client";

import * as Sentry from "@sentry/nextjs";
import { useTranslations } from "next-intl";
import { useEffect } from "react";

import { Button } from "@/shared/ui/button";

type LocaleErrorProps = {
  error: Error & { digest?: string };
  reset: () => void;
};

export default function LocaleError({ error, reset }: LocaleErrorProps) {
  const t = useTranslations("common");

  useEffect(() => {
    Sentry.captureException(error);
  }, [error]);

  return (
    <div className="flex flex-1 flex-col items-center justify-center gap-4 px-4 py-24 text-center">
      <h1 className="text-2xl font-semibold text-foreground">
        {t("errors.title")}
      </h1>
      <p className="max-w-md text-sm text-muted-foreground">
        {t("errors.generic")}
      </p>
      <Button onClick={reset}>{t("actions.tryAgain")}</Button>
    </div>
  );
}
