import { cookies } from "next/headers";
import { NextResponse } from "next/server";

import { siteConfig } from "@/app/config/seo";
import { serverEnv } from "@/shared/env/server";
import { refreshSession } from "@/shared/lib/auth/refresh-session";
import { isAccessTokenExpired } from "@/shared/lib/auth/session-claims";
import {
  ACCESS_COOKIE,
  clearSessionCookies,
  REFRESH_COOKIE,
  type SessionTokens,
  writeSessionCookies,
} from "@/shared/lib/auth/session-cookies";

/**
 * The browser's only door to apps/server (backend-for-frontend): the page
 * calls `/api/rpc/<connect procedure>` same-origin, this handler attaches
 * the bearer token from the httpOnly cookie, and transparently refreshes an
 * expired session once. Tokens therefore never reach page JavaScript, and
 * the browser needs no CORS or cross-origin `connect-src`.
 */

const MAX_BODY_BYTES = 1_000_000;

// Only the app's own services, and never the AuthService procedures that
// mint/revoke tokens — those go through Server Actions, which write the
// cookies. Letting them through here would hand raw tokens back to the page.
const ALLOWED_PROCEDURE =
  /^aedst\.(?:tasks|auth|notifications|search|media|admin)\.v1\.[A-Za-z]+Service\/[A-Za-z]+$/;
const TOKEN_PROCEDURES = new Set([
  "aedst.auth.v1.AuthService/Register",
  "aedst.auth.v1.AuthService/Login",
  "aedst.auth.v1.AuthService/RefreshToken",
  "aedst.auth.v1.AuthService/Logout",
]);

const FORWARDED_REQUEST_HEADERS = [
  "content-type",
  "accept",
  "connect-protocol-version",
  "connect-timeout-ms",
  "idempotency-key",
  // Trace propagation, so a browser span and the server's spans join up.
  "traceparent",
  "tracestate",
  "sentry-trace",
  "baggage",
];

const DROPPED_RESPONSE_HEADERS = new Set([
  "content-encoding",
  "content-length",
  "transfer-encoding",
  "connection",
  "keep-alive",
  "set-cookie",
]);

function sameOrigin(request: Request): boolean {
  const fetchSite = request.headers.get("sec-fetch-site");
  if (fetchSite === "cross-site" || fetchSite === "same-site") {
    return false;
  }
  const origin = request.headers.get("origin");
  if (!origin) {
    return false;
  }
  try {
    const { host } = new URL(origin);
    return (
      origin === siteConfig.url.origin || host === request.headers.get("host")
    );
  } catch {
    return false;
  }
}

function connectError(status: number, code: string, message: string) {
  return Response.json(
    { code, message },
    { status, headers: { "cache-control": "no-store" } },
  );
}

async function forward(
  procedure: string,
  request: Request,
  body: ArrayBuffer,
  accessToken: string | undefined,
): Promise<Response> {
  const headers = new Headers();
  for (const name of FORWARDED_REQUEST_HEADERS) {
    const value = request.headers.get(name);
    if (value) {
      headers.set(name, value);
    }
  }
  if (accessToken) {
    headers.set("authorization", `Bearer ${accessToken}`);
  }
  return await fetch(`${serverEnv.API_BASE_URL}/${procedure}`, {
    method: "POST",
    headers,
    body,
    cache: "no-store",
    signal: AbortSignal.timeout(15_000),
  });
}

function toClientResponse(upstream: Response): NextResponse {
  const headers = new Headers();
  upstream.headers.forEach((value, name) => {
    if (!DROPPED_RESPONSE_HEADERS.has(name)) {
      headers.set(name, value);
    }
  });
  headers.set("cache-control", "no-store");
  return new NextResponse(upstream.body, {
    status: upstream.status,
    headers,
  });
}

export async function POST(
  request: Request,
  { params }: RouteContext<"/api/rpc/[...path]">,
): Promise<Response> {
  if (!sameOrigin(request)) {
    return connectError(403, "permission_denied", "cross-origin request");
  }

  const procedure = (await params).path.join("/");
  if (!ALLOWED_PROCEDURE.test(procedure) || TOKEN_PROCEDURES.has(procedure)) {
    return connectError(404, "not_found", "unknown procedure");
  }

  const declaredLength = Number(request.headers.get("content-length") ?? 0);
  if (declaredLength > MAX_BODY_BYTES) {
    return connectError(413, "resource_exhausted", "request too large");
  }
  const body = await request.arrayBuffer();
  if (body.byteLength > MAX_BODY_BYTES) {
    return connectError(413, "resource_exhausted", "request too large");
  }

  const jar = await cookies();
  let accessToken = jar.get(ACCESS_COOKIE)?.value;
  const refreshToken = jar.get(REFRESH_COOKIE)?.value;
  let rotated: SessionTokens | null = null;
  let sessionEnded = false;

  async function rotate(): Promise<boolean> {
    if (!refreshToken) {
      sessionEnded = true;
      return false;
    }
    const result = await refreshSession(refreshToken);
    if (result.status === "ok") {
      rotated = result.tokens;
      accessToken = result.tokens.accessToken;
      return true;
    }
    sessionEnded = result.status === "rejected";
    return false;
  }

  if (!accessToken || isAccessTokenExpired(accessToken)) {
    await rotate();
  }

  let upstream: Response;
  try {
    upstream = await forward(procedure, request, body, accessToken);
    if (upstream.status === 401 && !rotated && (await rotate())) {
      upstream = await forward(procedure, request, body, accessToken);
    }
  } catch {
    return connectError(503, "unavailable", "upstream unavailable");
  }

  const response = toClientResponse(upstream);
  if (upstream.status === 401 && sessionEnded) {
    clearSessionCookies(response.cookies);
  } else if (rotated) {
    writeSessionCookies(response.cookies, rotated);
  }
  return response;
}
