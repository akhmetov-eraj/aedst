import { NextRequest, NextResponse } from "next/server";
import createMiddleware from "next-intl/middleware";

import { routing } from "@/app/config/i18n/routing";
import {
  buildContentSecurityPolicy,
  buildTrustedTypesReportOnlyPolicy,
  generateNonce,
  parseOrigins,
  sentryCspReportUri,
} from "@/app/config/security/csp";
import {
  classifySession,
  decideRoute,
  type SessionState,
} from "@/app/config/security/route-guard";
import { clientEnv } from "@/shared/env/client";
import { refreshSession } from "@/shared/lib/auth/refresh-session";
import { decodeSessionRoles } from "@/shared/lib/auth/session-claims";
import {
  ACCESS_COOKIE,
  clearSessionCookies,
  isSecureSite,
  REFRESH_COOKIE,
  type SessionTokens,
  writeSessionCookies,
} from "@/shared/lib/auth/session-cookies";

const handleI18nRouting = createMiddleware(routing);

const mediaOrigins = parseOrigins(process.env.MEDIA_ORIGINS);
const cspReportUri = sentryCspReportUri(clientEnv.NEXT_PUBLIC_SENTRY_DSN);

/**
 * Per-request pipeline for page routes:
 * 1. mint a CSP nonce (Next stamps it onto its scripts during render);
 * 2. resolve the session — rotating an expired access token via the refresh
 *    cookie *here*, because Server Components cannot set cookies;
 * 3. guard protected pages / skip the login page for signed-in users;
 * 4. delegate locale negotiation to next-intl.
 */
export default async function proxy(request: NextRequest) {
  const nonce = generateNonce();
  const csp = buildContentSecurityPolicy({
    nonce,
    isDev: process.env.NODE_ENV !== "production",
    upgradeInsecureRequests:
      process.env.NODE_ENV === "production" && isSecureSite,
    mediaOrigins,
    reportUri: cspReportUri,
  });

  let session: SessionState = classifySession({
    accessToken: request.cookies.get(ACCESS_COOKIE)?.value,
    refreshToken: request.cookies.get(REFRESH_COOKIE)?.value,
  });
  let rotated: SessionTokens | null = null;
  let ended = false;

  if (session === "refreshable") {
    const refreshToken = request.cookies.get(REFRESH_COOKIE)?.value ?? "";
    const result = await refreshSession(refreshToken);
    if (result.status === "ok") {
      rotated = result.tokens;
      session = "active";
      // Make the fresh token visible to this same request's Server Components.
      request.cookies.set(ACCESS_COOKIE, result.tokens.accessToken);
      request.cookies.set(REFRESH_COOKIE, result.tokens.refreshToken);
    } else if (result.status === "rejected") {
      ended = true;
      session = "anonymous";
    }
    // "unavailable": apps/server is down — do not sign the user out for it.
  }

  const accessToken = request.cookies.get(ACCESS_COOKIE)?.value;
  const decision = decideRoute({
    pathname: request.nextUrl.pathname,
    search: request.nextUrl.search,
    session,
    roles:
      session === "active" && accessToken
        ? decodeSessionRoles(accessToken)
        : [],
  });

  let response: NextResponse;
  if (decision.type === "redirect") {
    response = NextResponse.redirect(new URL(decision.location, request.url));
  } else {
    const requestHeaders = new Headers(request.headers);
    requestHeaders.set("x-nonce", nonce);
    requestHeaders.set("content-security-policy", csp);
    response = handleI18nRouting(
      new NextRequest(request, { headers: requestHeaders }),
    );
  }

  response.headers.set("content-security-policy", csp);
  if (cspReportUri) {
    response.headers.set(
      "content-security-policy-report-only",
      buildTrustedTypesReportOnlyPolicy(cspReportUri),
    );
  }
  if (rotated) {
    writeSessionCookies(response.cookies, rotated);
  } else if (ended) {
    clearSessionCookies(response.cookies);
  }
  return response;
}

export const config = {
  // `api` covers the RPC proxy route; `monitoring` is Sentry's `tunnelRoute`
  // and `ingest` the PostHog reverse proxy (next-intl would 30x-redirect
  // them to add a locale prefix and break them, see next.config.ts);
  // `serwist` serves the service worker; prefetches are skipped so `<Link>`
  // hover does not mint nonces or rotate tokens.
  matcher: [
    {
      source:
        "/((?!api|trpc|_next|_vercel|monitoring|ingest|serwist|offline|.*\\..*).*)",
      missing: [
        { type: "header", key: "next-router-prefetch" },
        { type: "header", key: "purpose", value: "prefetch" },
      ],
    },
  ],
};
