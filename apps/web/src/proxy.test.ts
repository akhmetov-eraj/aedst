// @vitest-environment node
import { NextRequest } from "next/server";
import { beforeEach, describe, expect, it } from "vitest";

import { AuthService } from "@/shared/api/connect/generated/aedst/auth/v1/auth_pb";
import { fakeJwt } from "@/test/jwt";
import {
  rpcErrorHandler,
  rpcHandler,
  TEST_API_ORIGIN,
} from "@/test/mocks/connect";
import { server } from "@/test/mocks/server";
import proxy from "./proxy";

const ACCESS = "aedst_session";
const REFRESH = "aedst_session_refresh";

function request(path: string, cookies: Record<string, string> = {}) {
  const cookie = Object.entries(cookies)
    .map(([name, value]) => `${name}=${value}`)
    .join("; ");
  return new NextRequest(`http://localhost:3000${path}`, {
    headers: cookie ? { cookie } : {},
  });
}

const rotated = () => ({
  tokens: {
    accessToken: fakeJwt({ sub: "rotated", expiresInSeconds: 900 }),
    refreshToken: "rotated-refresh",
    accessTokenExpiresInSeconds: 900n,
    refreshTokenExpiresInSeconds: 86_400n,
  },
});

// next-intl also sets its own NEXT_LOCALE cookie; these tests are about the session ones.
const sessionCookies = (response: Response) =>
  response.headers
    .getSetCookie()
    .filter((cookie) => cookie.startsWith("aedst_session"));

let refreshCounter = 0;
const freshRefreshToken = () => `rt-proxy-${++refreshCounter}`;

beforeEach(() => {
  refreshCounter += 100;
});

describe("proxy: content security policy", () => {
  it("issues a fresh nonce per request and exposes it to the render", async () => {
    const first = await proxy(request("/en"));
    const second = await proxy(request("/en"));

    const csp = first.headers.get("content-security-policy") ?? "";
    const nonce = /'nonce-([^']+)'/.exec(csp)?.[1];
    expect(nonce).toBeTruthy();
    expect(csp).not.toMatch(/script-src[^;]*'unsafe-inline'/);
    expect(nonce).not.toBe(
      /'nonce-([^']+)'/.exec(
        second.headers.get("content-security-policy") ?? "",
      )?.[1],
    );

    // The request headers Next.js renders with must carry the nonce/CSP, or
    // it cannot stamp the nonce onto its scripts.
    expect(first.headers.get("x-middleware-request-x-nonce")).toBe(nonce);
    expect(
      first.headers.get("x-middleware-request-content-security-policy"),
    ).toBe(csp);
  });
});

describe("proxy: route protection", () => {
  it("redirects an anonymous visitor from a protected page, keeping the destination", async () => {
    const response = await proxy(request("/en/tasks?status=done"));

    expect(response.status).toBe(307);
    expect(response.headers.get("location")).toBe(
      `http://localhost:3000/en?next=${encodeURIComponent("/tasks?status=done")}`,
    );
  });

  it("lets an anonymous visitor see the login page", async () => {
    const response = await proxy(request("/en"));
    expect(response.headers.get("location")).toBeNull();
  });

  it("sends a signed-in visitor from the login page to the dashboard", async () => {
    const response = await proxy(
      request("/en", { [ACCESS]: fakeJwt({ expiresInSeconds: 600 }) }),
    );

    expect(response.headers.get("location")).toBe(
      "http://localhost:3000/en/tasks",
    );
  });

  it("lets a signed-in visitor through to protected pages", async () => {
    const response = await proxy(
      request("/en/tasks", { [ACCESS]: fakeJwt({ expiresInSeconds: 600 }) }),
    );

    expect(response.headers.get("location")).toBeNull();
    expect(sessionCookies(response)).toEqual([]);
  });

  it("negotiates the locale for a bare path (next-intl still runs)", async () => {
    const response = await proxy(request("/"));
    expect(response.headers.get("location")).toMatch(/\/en$/);
  });
});

describe("proxy: session refresh", () => {
  it("renews an expired access token, sets the cookies and hands the fresh token to the render", async () => {
    const refreshToken = freshRefreshToken();
    server.use(
      rpcHandler(AuthService.method.refreshToken, rotated, TEST_API_ORIGIN),
    );

    const response = await proxy(
      request("/en/tasks", {
        [ACCESS]: fakeJwt({ expiresInSeconds: -60 }),
        [REFRESH]: refreshToken,
      }),
    );

    expect(response.headers.get("location")).toBeNull();
    const setCookies = sessionCookies(response);
    expect(setCookies.some((c) => c.startsWith(`${ACCESS}=`))).toBe(true);
    expect(
      setCookies.some((c) => c.startsWith(`${REFRESH}=rotated-refresh`)),
    ).toBe(true);
    for (const cookie of setCookies) {
      expect(cookie).toMatch(/HttpOnly/i);
    }
    // Server Components in this same request must see the new token.
    expect(response.headers.get("x-middleware-request-cookie")).toContain(
      `${REFRESH}=rotated-refresh`,
    );
  });

  it("renews when only the refresh cookie is left", async () => {
    server.use(
      rpcHandler(AuthService.method.refreshToken, rotated, TEST_API_ORIGIN),
    );

    const response = await proxy(
      request("/en/tasks", { [REFRESH]: freshRefreshToken() }),
    );

    expect(response.headers.get("location")).toBeNull();
    expect(sessionCookies(response).length).toBe(2);
  });

  it("ends the session when the refresh token is rejected", async () => {
    server.use(
      rpcErrorHandler(
        AuthService.method.refreshToken,
        "unauthenticated",
        "revoked",
        TEST_API_ORIGIN,
      ),
    );

    const response = await proxy(
      request("/en/tasks", { [REFRESH]: freshRefreshToken() }),
    );

    expect(response.status).toBe(307);
    expect(response.headers.get("location")).toContain("/en?next=");
    const cleared = sessionCookies(response);
    expect(cleared.length).toBe(2);
    for (const cookie of cleared) {
      expect(cookie).toMatch(/Max-Age=0/i);
    }
  });

  it("does not sign anyone out because the API is down", async () => {
    server.use(
      rpcErrorHandler(
        AuthService.method.refreshToken,
        "internal",
        "boom",
        TEST_API_ORIGIN,
      ),
    );

    const response = await proxy(
      request("/en/tasks", { [REFRESH]: freshRefreshToken() }),
    );

    expect(response.headers.get("location")).toBeNull();
    expect(sessionCookies(response)).toEqual([]);
  });
});
