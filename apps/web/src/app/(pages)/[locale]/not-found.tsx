import { getTranslations } from "next-intl/server";

import { Link } from "@/app/config/i18n/navigation";
import { Button } from "@/shared/ui/button";

export default async function LocaleNotFound() {
  const t = await getTranslations("common");

  return (
    <div className="flex flex-1 flex-col items-center justify-center gap-4 px-4 py-24 text-center">
      <p className="text-sm font-medium text-muted-foreground">404</p>
      <h1 className="text-2xl font-semibold text-foreground">
        {t("errors.notFoundTitle")}
      </h1>
      <p className="max-w-md text-sm text-muted-foreground">
        {t("errors.notFoundDescription")}
      </p>
      <Button render={<Link href="/" />}>{t("actions.goHome")}</Button>
    </div>
  );
}
