import { describe, expect, it, vi } from "vitest";

import { AuthService } from "@/shared/api/connect/generated/aedst/auth/v1/auth_pb";
import { refreshSession } from "@/shared/lib/auth/refresh-session";
import {
  rpcErrorHandler,
  rpcHandler,
  TEST_API_ORIGIN,
} from "@/test/mocks/connect";
import { server } from "@/test/mocks/server";

const tokenPair = {
  accessToken: "new-access",
  refreshToken: "new-refresh",
  accessTokenExpiresInSeconds: 900n,
  refreshTokenExpiresInSeconds: 86_400n,
};

describe("refreshSession", () => {
  it("exchanges a refresh token for a new session", async () => {
    server.use(
      rpcHandler(
        AuthService.method.refreshToken,
        (input) => {
          expect(input.refreshToken).toBe("rt-ok");
          return { tokens: tokenPair };
        },
        TEST_API_ORIGIN,
      ),
    );

    await expect(refreshSession("rt-ok")).resolves.toEqual({
      status: "ok",
      tokens: {
        accessToken: "new-access",
        refreshToken: "new-refresh",
        accessMaxAgeSeconds: 900,
        refreshMaxAgeSeconds: 86_400,
      },
    });
  });

  it("shares one upstream call between concurrent requests (rotation would reject the second)", async () => {
    const upstream = vi.fn(() => ({ tokens: tokenPair }));
    server.use(
      rpcHandler(AuthService.method.refreshToken, upstream, TEST_API_ORIGIN),
    );

    const results = await Promise.all([
      refreshSession("rt-concurrent"),
      refreshSession("rt-concurrent"),
      refreshSession("rt-concurrent"),
    ]);

    expect(upstream).toHaveBeenCalledTimes(1);
    expect(new Set(results.map((r) => r.status))).toEqual(new Set(["ok"]));
  });

  it("reuses a fresh result for requests that were already on the wire with the old token", async () => {
    const upstream = vi.fn(() => ({ tokens: tokenPair }));
    server.use(
      rpcHandler(AuthService.method.refreshToken, upstream, TEST_API_ORIGIN),
    );

    await refreshSession("rt-reuse");
    await refreshSession("rt-reuse");

    expect(upstream).toHaveBeenCalledTimes(1);
  });

  it("reports a definitive rejection when the server refuses the token", async () => {
    server.use(
      rpcErrorHandler(
        AuthService.method.refreshToken,
        "unauthenticated",
        "revoked",
        TEST_API_ORIGIN,
      ),
    );

    await expect(refreshSession("rt-revoked")).resolves.toEqual({
      status: "rejected",
    });
  });

  it("treats a response without tokens as a rejection, never an empty session", async () => {
    server.use(
      rpcHandler(AuthService.method.refreshToken, () => ({}), TEST_API_ORIGIN),
    );

    await expect(refreshSession("rt-empty")).resolves.toEqual({
      status: "rejected",
    });
  });

  it("does not call a server outage a sign-out", async () => {
    server.use(
      rpcErrorHandler(
        AuthService.method.refreshToken,
        "internal",
        "boom",
        TEST_API_ORIGIN,
      ),
    );

    await expect(refreshSession("rt-outage")).resolves.toEqual({
      status: "unavailable",
    });
  });
});
