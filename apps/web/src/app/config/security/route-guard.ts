import { type Locale, locales } from "@/app/config/i18n/locales";
import { ADMIN_ROLE } from "@/shared/lib/auth/roles";
import { safeRedirectPath } from "@/shared/lib/auth/safe-redirect";
import { isAccessTokenExpired } from "@/shared/lib/auth/session-claims";

/** Locale-less path prefixes that require a signed-in user. */
const PROTECTED_PREFIXES = ["/tasks", "/search", "/notifications", "/admin"];

/** Prefixes that additionally require a role (UX only — the server enforces it). */
const ROLE_PREFIXES: ReadonlyArray<{ prefix: string; role: string }> = [
  { prefix: "/admin", role: ADMIN_ROLE },
];

const LOCALE_PATH = new RegExp(`^/(${locales.join("|")})(/.*)?$`);

export type SessionState =
  /** A usable (unexpired) access token is present. */
  | "active"
  /** No usable access token, but a refresh token could mint one. */
  | "refreshable"
  | "anonymous";

export function classifySession({
  accessToken,
  refreshToken,
  nowMs,
}: {
  accessToken: string | undefined;
  refreshToken: string | undefined;
  nowMs?: number;
}): SessionState {
  if (accessToken && !isAccessTokenExpired(accessToken, nowMs)) {
    return "active";
  }
  return refreshToken ? "refreshable" : "anonymous";
}

export function splitLocale(
  pathname: string,
): { locale: Locale; rest: string } | null {
  const match = LOCALE_PATH.exec(pathname);
  if (!match) {
    return null;
  }
  return { locale: match[1] as Locale, rest: match[2] ?? "" };
}

function matchesPrefix(rest: string, prefix: string): boolean {
  return rest === prefix || rest.startsWith(`${prefix}/`);
}

function isProtected(rest: string): boolean {
  return PROTECTED_PREFIXES.some((prefix) => matchesPrefix(rest, prefix));
}

function requiredRole(rest: string): string | undefined {
  return ROLE_PREFIXES.find(({ prefix }) => matchesPrefix(rest, prefix))?.role;
}

export type RouteDecision =
  | { type: "next" }
  | { type: "redirect"; location: string };

/**
 * Where a request should go given its session — pure so the auth rules are
 * unit-testable without a running proxy:
 * - a protected page without a session goes to the login page, remembering
 *   the destination in `?next=` so sign-in lands back on it;
 * - a role-gated page for a signed-in user without that role goes to the
 *   forbidden page (the server would refuse every call on it anyway);
 * - the login page with a session skips straight to that destination.
 */
export function decideRoute({
  pathname,
  search,
  session,
  roles = [],
}: {
  pathname: string;
  search: string;
  session: SessionState;
  /** Unverified roles of the active session. */
  roles?: readonly string[];
}): RouteDecision {
  const parts = splitLocale(pathname);
  if (!parts) {
    return { type: "next" };
  }
  const { locale, rest } = parts;

  if (isProtected(rest) && session === "anonymous") {
    const next = encodeURIComponent(`${rest}${search}`);
    return { type: "redirect", location: `/${locale}?next=${next}` };
  }

  const role = requiredRole(rest);
  if (role && session === "active" && !roles.includes(role)) {
    return { type: "redirect", location: `/${locale}/forbidden` };
  }

  if ((rest === "" || rest === "/") && session !== "anonymous") {
    const requested = new URLSearchParams(search).get("next");
    return {
      type: "redirect",
      location: `/${locale}${safeRedirectPath(requested)}`,
    };
  }

  return { type: "next" };
}
