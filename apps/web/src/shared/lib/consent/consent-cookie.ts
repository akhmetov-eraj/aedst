export type ConsentState = "unknown" | "granted" | "denied";

/**
 * Consent lives in a plain (non-httpOnly) first-party cookie rather than
 * `localStorage` so the *server* can read it: the banner is then rendered in
 * the first HTML only for visitors who have not answered — no flash for those
 * who have, and no client-only banner popping in after hydration (which
 * would also become the page's Largest Contentful Paint element).
 */
export const CONSENT_COOKIE = "aedst_consent";
export const CONSENT_MAX_AGE_SECONDS = 60 * 60 * 24 * 365;

export function parseConsent(value: string | undefined): ConsentState {
  return value === "granted" || value === "denied" ? value : "unknown";
}
