import type { Locale } from "@/app/config/i18n/locales";
import { i18nModules } from "@/app/config/i18n/modules";

/**
 * Assembles the request-scoped `messages` object for a locale by merging:
 * - `common`: cross-cutting strings shared by every module (nav, actions, errors)
 * - one namespace per feature module, loaded from `src/modules/<module>/i18n/<locale>.json`
 *
 * This keeps translations co-located with the module that owns them instead
 * of a single growing `messages/<locale>.json` file.
 */
export async function loadMessages(locale: Locale) {
  const [common, ...moduleEntries] = await Promise.all([
    import(`@/app/config/i18n/common/${locale}.json`).then((m) => m.default),
    ...i18nModules.map(async (module) => {
      const messages = await import(`@/modules/${module}/i18n/${locale}.json`);
      return [module, messages.default] as const;
    }),
  ]);

  return {
    common,
    ...Object.fromEntries(moduleEntries),
  };
}
