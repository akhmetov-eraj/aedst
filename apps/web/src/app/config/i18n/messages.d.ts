// Keep this in sync with `i18nModules` in modules.ts: one type-only import
// per module gives `useTranslations`/`t()` full namespace + key autocompletion.
import type common from "@/app/config/i18n/common/en.json";
import type home from "@/modules/home/i18n/en.json";

type Messages = {
  common: typeof common;
  home: typeof home;
};

declare module "next-intl" {
  interface AppConfig {
    Messages: Messages;
  }
}
