export const locales = ["en", "ru", "tj", "uz", "ar"] as const;

export type Locale = (typeof locales)[number];

export const defaultLocale: Locale = "en";

export const localeNames: Record<Locale, string> = {
  en: "English",
  ru: "Русский",
  tj: "Тоҷикӣ",
  uz: "O'zbekcha",
  ar: "العربية",
};

export const rtlLocales: readonly Locale[] = ["ar"];

export function getLocaleDirection(locale: Locale): "ltr" | "rtl" {
  return rtlLocales.includes(locale) ? "rtl" : "ltr";
}
