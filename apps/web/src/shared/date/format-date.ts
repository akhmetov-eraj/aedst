import { format, formatDistanceToNow } from "date-fns";
import { ar, enUS, ru, uz } from "date-fns/locale";

import type { Locale } from "@/app/config/i18n/locales";

type DateFnsLocale = typeof enUS;

// BOLIPLARE Library-First rule: date-fns is the one primary date tool for
// this app — no toLocaleDateString()/toLocaleString(), no second date
// library. date-fns has no Tajik locale, so "tj" falls back to "ru" (closest
// shared date/number conventions of the app's supported locales) rather than
// silently defaulting to English formatting.
const DATE_FNS_LOCALES: Record<Locale, DateFnsLocale> = {
  en: enUS,
  ru,
  tj: ru,
  uz,
  ar,
};

export function formatDate(date: Date, locale: Locale): string {
  return format(date, "PP", { locale: DATE_FNS_LOCALES[locale] });
}

export function formatDateTime(date: Date, locale: Locale): string {
  return format(date, "PPp", { locale: DATE_FNS_LOCALES[locale] });
}

/** "5 minutes ago", localized (inbox, audit log). */
export function formatRelative(date: Date, locale: Locale): string {
  return formatDistanceToNow(date, {
    addSuffix: true,
    locale: DATE_FNS_LOCALES[locale],
  });
}
