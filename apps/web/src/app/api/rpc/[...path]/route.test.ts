import { HttpResponse, http } from "msw";
import { beforeEach, describe, expect, it, vi } from "vitest";

import { AuthService } from "@/shared/api/connect/generated/aedst/auth/v1/auth_pb";
import { TasksService } from "@/shared/api/connect/generated/aedst/tasks/v1/tasks_pb";
import { fakeJwt } from "@/test/jwt";
import {
  rpcErrorHandler,
  rpcHandler,
  TEST_API_ORIGIN,
} from "@/test/mocks/connect";
import { server } from "@/test/mocks/server";
import { POST } from "./route";

const jar = new Map<string, string>();

vi.mock("next/headers", () => ({
  cookies: async () => ({
    get: (name: string) =>
      jar.has(name) ? { name, value: jar.get(name) } : undefined,
  }),
}));

const LIST = "aedst.tasks.v1.TasksService/ListTasks";

function call(
  procedure: string,
  init: { origin?: string | null; headers?: Record<string, string> } = {},
) {
  const headers: Record<string, string> = {
    "content-type": "application/json",
    host: "localhost:3000",
    ...init.headers,
  };
  if (init.origin !== null) {
    headers.origin = init.origin ?? "http://localhost:3000";
  }
  return POST(
    new Request(`http://localhost:3000/api/rpc/${procedure}`, {
      method: "POST",
      headers,
      body: "{}",
    }),
    { params: Promise.resolve({ path: procedure.split("/") }) },
  );
}

function setCookies(response: Response): string[] {
  return response.headers.getSetCookie();
}

beforeEach(() => {
  jar.clear();
});

describe("/api/rpc BFF proxy", () => {
  describe("request validation", () => {
    it("rejects cross-origin callers (CSRF)", async () => {
      const response = await call(LIST, { origin: "https://evil.example" });
      expect(response.status).toBe(403);
    });

    it("rejects requests with no Origin header", async () => {
      expect((await call(LIST, { origin: null })).status).toBe(403);
    });

    it("rejects browser-declared cross-site fetches", async () => {
      const response = await call(LIST, {
        headers: { "sec-fetch-site": "cross-site" },
      });
      expect(response.status).toBe(403);
    });

    it.each([
      "aedst.auth.v1.AuthService/Login",
      "aedst.auth.v1.AuthService/Register",
      "aedst.auth.v1.AuthService/RefreshToken",
      "aedst.auth.v1.AuthService/Logout",
    ])(
      "refuses %s — tokens must never be handed to page JavaScript",
      async (procedure) => {
        expect((await call(procedure)).status).toBe(404);
      },
    );

    it.each([
      "evil.v1.Service/Method",
      "aedst.tasks.v1.TasksService/../Admin",
      "aedst.tasks.v1.TasksService",
    ])("refuses unknown procedure %s", async (procedure) => {
      expect((await call(procedure)).status).toBe(404);
    });
  });

  describe("forwarding", () => {
    it("attaches the bearer token from the httpOnly cookie and relays the response", async () => {
      const access = fakeJwt({ expiresInSeconds: 600 });
      jar.set("aedst_session", access);
      let seenAuth: string | null = null;
      server.use(
        rpcHandler(
          TasksService.method.listTasks,
          (_input, request) => {
            seenAuth = request.headers.get("authorization");
            return { tasks: [], totalCount: 5 };
          },
          TEST_API_ORIGIN,
        ),
      );

      const response = await call(LIST);

      expect(response.status).toBe(200);
      expect(seenAuth).toBe(`Bearer ${access}`);
      expect(await response.json()).toMatchObject({ totalCount: 5 });
      expect(response.headers.get("cache-control")).toBe("no-store");
      expect(setCookies(response)).toEqual([]);
    });

    it("forwards trace headers so browser and server spans join up", async () => {
      jar.set("aedst_session", fakeJwt({ expiresInSeconds: 600 }));
      let seen: Record<string, string | null> = {};
      server.use(
        rpcHandler(
          TasksService.method.listTasks,
          (_input, request) => {
            seen = {
              traceparent: request.headers.get("traceparent"),
              sentryTrace: request.headers.get("sentry-trace"),
              cookie: request.headers.get("cookie"),
            };
            return { tasks: [] };
          },
          TEST_API_ORIGIN,
        ),
      );

      await call(LIST, {
        headers: {
          traceparent: "00-abc-def-01",
          "sentry-trace": "abc-def-1",
          cookie: "aedst_session=secret",
        },
      });

      expect(seen.traceparent).toBe("00-abc-def-01");
      expect(seen.sentryTrace).toBe("abc-def-1");
      // The session cookie itself never goes upstream.
      expect(seen.cookie).toBeNull();
    });

    it("answers 503 when the API is unreachable", async () => {
      jar.set("aedst_session", fakeJwt({ expiresInSeconds: 600 }));
      server.use(
        http.post(`${TEST_API_ORIGIN}/${LIST}`, () => HttpResponse.error()),
      );

      expect((await call(LIST)).status).toBe(503);
    });
  });

  describe("session refresh", () => {
    const rotate = (label: string) =>
      rpcHandler(
        AuthService.method.refreshToken,
        () => ({
          tokens: {
            accessToken: fakeJwt({ sub: label, expiresInSeconds: 900 }),
            refreshToken: `${label}-refresh`,
            accessTokenExpiresInSeconds: 900n,
            refreshTokenExpiresInSeconds: 86_400n,
          },
        }),
        TEST_API_ORIGIN,
      );

    it("refreshes an expired access token before calling, and sets the new cookies", async () => {
      jar.set("aedst_session", fakeJwt({ expiresInSeconds: -60 }));
      jar.set("aedst_session_refresh", "rt-expired-access");
      let seenAuth: string | null = null;
      server.use(
        rotate("proactive"),
        rpcHandler(
          TasksService.method.listTasks,
          (_input, request) => {
            seenAuth = request.headers.get("authorization");
            return { tasks: [] };
          },
          TEST_API_ORIGIN,
        ),
      );

      const response = await call(LIST);

      expect(response.status).toBe(200);
      expect(seenAuth).toMatch(/^Bearer .+\..+\..+$/);
      const cookies = setCookies(response);
      expect(cookies.some((c) => c.startsWith("aedst_session="))).toBe(true);
      expect(
        cookies.some((c) =>
          c.startsWith("aedst_session_refresh=proactive-refresh"),
        ),
      ).toBe(true);
      for (const cookie of cookies) {
        expect(cookie).toContain("HttpOnly");
        expect(cookie).toContain("SameSite=lax");
      }
    });

    it("retries once with a fresh token when the API answers 401", async () => {
      jar.set("aedst_session", fakeJwt({ expiresInSeconds: 600 }));
      jar.set("aedst_session_refresh", "rt-server-side-revoked-access");
      const seenAuth: Array<string | null> = [];
      // First call: 401. Second (after refresh): success.
      let attempts = 0;
      server.use(
        rotate("retry"),
        http.post(`${TEST_API_ORIGIN}/${LIST}`, ({ request }) => {
          attempts += 1;
          seenAuth.push(request.headers.get("authorization"));
          return attempts === 1
            ? HttpResponse.json(
                { code: "unauthenticated", message: "expired" },
                { status: 401 },
              )
            : HttpResponse.json({ tasks: [] });
        }),
      );

      const response = await call(LIST);

      expect(response.status).toBe(200);
      expect(attempts).toBe(2);
      expect(seenAuth[0]).not.toBe(seenAuth[1]);
      expect(setCookies(response).length).toBe(2);
    });

    it("ends the session (401 + cleared cookies) when the refresh token is rejected", async () => {
      jar.set("aedst_session", fakeJwt({ expiresInSeconds: -60 }));
      jar.set("aedst_session_refresh", "rt-rejected-by-server");
      server.use(
        rpcErrorHandler(
          AuthService.method.refreshToken,
          "unauthenticated",
          "revoked",
          TEST_API_ORIGIN,
        ),
        rpcErrorHandler(
          TasksService.method.listTasks,
          "unauthenticated",
          "no token",
          TEST_API_ORIGIN,
        ),
      );

      const response = await call(LIST);

      expect(response.status).toBe(401);
      const cleared = setCookies(response);
      expect(cleared.length).toBe(2);
      for (const cookie of cleared) {
        expect(cookie).toMatch(/Max-Age=0/i);
      }
    });

    it("does not sign the user out when the refresh service is merely down", async () => {
      jar.set("aedst_session", fakeJwt({ expiresInSeconds: -60 }));
      jar.set("aedst_session_refresh", "rt-during-outage");
      server.use(
        rpcErrorHandler(
          AuthService.method.refreshToken,
          "internal",
          "boom",
          TEST_API_ORIGIN,
        ),
        rpcErrorHandler(
          TasksService.method.listTasks,
          "unauthenticated",
          "no token",
          TEST_API_ORIGIN,
        ),
      );

      const response = await call(LIST);

      expect(response.status).toBe(401);
      expect(setCookies(response)).toEqual([]);
    });
  });
});
