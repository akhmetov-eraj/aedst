import { beforeEach, describe, expect, it, vi } from "vitest";

import { AuthService } from "@/shared/api/connect/generated/aedst/auth/v1/auth_pb";
import {
  rpcErrorHandler,
  rpcHandler,
  TEST_API_ORIGIN,
} from "@/test/mocks/connect";
import { server } from "@/test/mocks/server";
import { loginAction, logoutAction, registerAction } from "./auth-actions";

type SetCall = [string, string, Record<string, unknown>];
const setCalls: SetCall[] = [];
const jar = new Map<string, string>();

vi.mock("next/headers", () => ({
  cookies: async () => ({
    get: (name: string) =>
      jar.has(name) ? { name, value: jar.get(name) } : undefined,
    set: (name: string, value: string, options: Record<string, unknown>) => {
      setCalls.push([name, value, options]);
    },
  }),
}));

const tokens = {
  accessToken: "access-1",
  refreshToken: "refresh-1",
  accessTokenExpiresInSeconds: 900n,
  refreshTokenExpiresInSeconds: 86_400n,
};
const credentials = {
  email: "ada@example.com",
  password: "correct horse",
  deviceId: "web-device-1",
};

beforeEach(() => {
  setCalls.length = 0;
  jar.clear();
});

describe("loginAction", () => {
  it("stores tokens only in httpOnly cookies and returns just the user id", async () => {
    server.use(
      rpcHandler(
        AuthService.method.login,
        (input) => {
          expect(input).toMatchObject(credentials);
          return { userId: "user-1", tokens };
        },
        TEST_API_ORIGIN,
      ),
    );

    const result = await loginAction(credentials);

    expect(result).toEqual({ ok: true, userId: "user-1" });
    expect(JSON.stringify(result)).not.toContain("access-1");
    expect(setCalls.map(([name, value]) => [name, value])).toEqual([
      ["aedst_session", "access-1"],
      ["aedst_session_refresh", "refresh-1"],
    ]);
    for (const [, , options] of setCalls) {
      expect(options).toMatchObject({
        httpOnly: true,
        sameSite: "lax",
        path: "/",
      });
    }
    expect(setCalls[0]?.[2].maxAge).toBe(900);
    expect(setCalls[1]?.[2].maxAge).toBe(86_400);
  });

  it("refuses a 'successful' response without tokens instead of signing in with blanks", async () => {
    server.use(
      rpcHandler(
        AuthService.method.login,
        () => ({ userId: "user-1", tokens: undefined }),
        TEST_API_ORIGIN,
      ),
    );

    await expect(loginAction(credentials)).resolves.toEqual({
      ok: false,
      error: "noTokens",
    });
    expect(setCalls).toEqual([]);
  });

  it("surfaces the server's rejection message without setting cookies", async () => {
    server.use(
      rpcErrorHandler(
        AuthService.method.login,
        "unauthenticated",
        "invalid credentials",
        TEST_API_ORIGIN,
      ),
    );

    await expect(loginAction(credentials)).resolves.toEqual({
      ok: false,
      error: "rejected",
      message: "invalid credentials",
    });
    expect(setCalls).toEqual([]);
  });

  it.each([
    { ...credentials, email: "not-an-email" },
    { ...credentials, password: "" },
    { ...credentials, deviceId: "" },
    { email: "a@b.co" },
    null,
    "nope",
  ])("validates input on the server too: %j", async (input) => {
    await expect(loginAction(input)).resolves.toEqual({
      ok: false,
      error: "invalidInput",
    });
  });
});

describe("registerAction", () => {
  it("creates the account and signs in", async () => {
    server.use(
      rpcHandler(
        AuthService.method.register,
        () => ({ user: { id: "user-9", email: credentials.email }, tokens }),
        TEST_API_ORIGIN,
      ),
    );

    await expect(registerAction(credentials)).resolves.toEqual({
      ok: true,
      userId: "user-9",
    });
    expect(setCalls).toHaveLength(2);
  });

  it("enforces the 8-character password minimum", async () => {
    await expect(
      registerAction({ ...credentials, password: "short" }),
    ).resolves.toEqual({ ok: false, error: "invalidInput" });
  });

  it("fails without tokens or user", async () => {
    server.use(
      rpcHandler(
        AuthService.method.register,
        () => ({ user: undefined, tokens }),
        TEST_API_ORIGIN,
      ),
    );

    await expect(registerAction(credentials)).resolves.toMatchObject({
      ok: false,
      error: "noTokens",
    });
  });
});

describe("logoutAction", () => {
  it("clears both cookies and revokes the refresh token server-side", async () => {
    jar.set("aedst_session_refresh", "refresh-1");
    let revoked: string | undefined;
    server.use(
      rpcHandler(
        AuthService.method.logout,
        (input) => {
          revoked = input.refreshToken;
          return {};
        },
        TEST_API_ORIGIN,
      ),
    );

    await logoutAction();

    expect(revoked).toBe("refresh-1");
    expect(setCalls.map(([name, value]) => [name, value])).toEqual([
      ["aedst_session", ""],
      ["aedst_session_refresh", ""],
    ]);
    for (const [, , options] of setCalls) {
      expect(options.maxAge).toBe(0);
    }
  });

  it("still signs this browser out when the revoke call fails", async () => {
    jar.set("aedst_session_refresh", "refresh-1");
    server.use(
      rpcErrorHandler(
        AuthService.method.logout,
        "internal",
        "boom",
        TEST_API_ORIGIN,
      ),
    );

    await expect(logoutAction()).resolves.toBeUndefined();
    expect(setCalls).toHaveLength(2);
  });

  it("makes no server call when there is no session", async () => {
    await logoutAction();
    expect(setCalls).toHaveLength(2);
  });
});
