import { describe, expect, it } from "vitest";

import type { TokenPair } from "@/shared/api/connect/generated/aedst/auth/v1/auth_pb";
import { sessionTokensFromPair } from "@/shared/lib/auth/session-tokens";

function pair(overrides: Partial<TokenPair> = {}): TokenPair {
  return {
    $typeName: "aedst.auth.v1.TokenPair",
    accessToken: "access",
    refreshToken: "refresh",
    accessTokenExpiresInSeconds: 900n,
    refreshTokenExpiresInSeconds: 86_400n,
    ...overrides,
  };
}

describe("sessionTokensFromPair", () => {
  it("maps a complete pair to cookie lifetimes in seconds", () => {
    expect(sessionTokensFromPair(pair())).toEqual({
      accessToken: "access",
      refreshToken: "refresh",
      accessMaxAgeSeconds: 900,
      refreshMaxAgeSeconds: 86_400,
    });
  });

  it("returns null when the response carried no tokens at all", () => {
    expect(sessionTokensFromPair(undefined)).toBeNull();
  });

  it.each([{ accessToken: "" }, { refreshToken: "" }])(
    "returns null when a token is empty (%o) — never a signed-in session with blank credentials",
    (overrides) => {
      expect(sessionTokensFromPair(pair(overrides))).toBeNull();
    },
  );

  it("falls back to a sane lifetime when the server sends none", () => {
    const tokens = sessionTokensFromPair(
      pair({
        accessTokenExpiresInSeconds: 0n,
        refreshTokenExpiresInSeconds: -1n,
      }),
    );
    expect(tokens?.accessMaxAgeSeconds).toBe(900);
    expect(tokens?.refreshMaxAgeSeconds).toBe(900);
  });
});
