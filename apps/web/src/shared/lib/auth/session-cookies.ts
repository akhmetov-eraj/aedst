import { clientEnv } from "@/shared/env/client";

/**
 * The session lives in two httpOnly cookies set only by this app's server
 * side (Server Actions, the RPC proxy, `proxy.ts`) — page JavaScript never
 * sees a token, so an XSS can ride the session but can't exfiltrate it.
 *
 * `__Host-` (Secure + Path=/ + no Domain) is added whenever the site is
 * served over HTTPS; on plain-HTTP localhost browsers reject Secure
 * cookies, so dev uses the bare names.
 */
const BASE_NAME = process.env.AUTH_COOKIE_NAME || "aedst_session";

export const isSecureSite = clientEnv.NEXT_PUBLIC_SITE_URL.startsWith("https:");
const prefix = isSecureSite ? "__Host-" : "";

export const ACCESS_COOKIE = `${prefix}${BASE_NAME}`;
export const REFRESH_COOKIE = `${prefix}${BASE_NAME}_refresh`;

export type SessionTokens = {
  accessToken: string;
  refreshToken: string;
  accessMaxAgeSeconds: number;
  refreshMaxAgeSeconds: number;
};

type CookieOptions = {
  httpOnly: boolean;
  secure: boolean;
  sameSite: "lax";
  path: string;
  maxAge: number;
};

/** The subset of `cookies()` / `NextResponse.cookies` this module writes through. */
export type CookieWriter = {
  set(name: string, value: string, options?: CookieOptions): unknown;
};

function baseOptions(maxAge: number): CookieOptions {
  return {
    httpOnly: true,
    secure: isSecureSite,
    sameSite: "lax",
    path: "/",
    maxAge,
  };
}

export function writeSessionCookies(
  jar: CookieWriter,
  tokens: SessionTokens,
): void {
  jar.set(
    ACCESS_COOKIE,
    tokens.accessToken,
    baseOptions(tokens.accessMaxAgeSeconds),
  );
  jar.set(
    REFRESH_COOKIE,
    tokens.refreshToken,
    baseOptions(tokens.refreshMaxAgeSeconds),
  );
}

export function clearSessionCookies(jar: CookieWriter): void {
  jar.set(ACCESS_COOKIE, "", baseOptions(0));
  jar.set(REFRESH_COOKIE, "", baseOptions(0));
}
