import { defineRouting } from "next-intl/routing";

import { defaultLocale, locales } from "@/app/config/i18n/locales";

export const routing = defineRouting({
  locales,
  defaultLocale,
  localePrefix: "always",
  localeDetection: true,
});
