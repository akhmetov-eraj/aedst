import type { TokenPair } from "@/shared/api/connect/generated/aedst/auth/v1/auth_pb";
import type { SessionTokens } from "@/shared/lib/auth/session-cookies";

/**
 * Maps the wire `TokenPair` to what the cookie layer stores. Returns `null`
 * when either token is missing/empty: a "successful" response without
 * tokens must fail loudly instead of becoming a signed-in session holding
 * blank credentials.
 */
export function sessionTokensFromPair(
  pair: TokenPair | undefined,
): SessionTokens | null {
  if (!pair?.accessToken || !pair.refreshToken) {
    return null;
  }
  return {
    accessToken: pair.accessToken,
    refreshToken: pair.refreshToken,
    accessMaxAgeSeconds: positiveSeconds(pair.accessTokenExpiresInSeconds),
    refreshMaxAgeSeconds: positiveSeconds(pair.refreshTokenExpiresInSeconds),
  };
}

const FALLBACK_MAX_AGE_SECONDS = 60 * 15;

function positiveSeconds(value: bigint): number {
  const seconds = Number(value);
  return Number.isFinite(seconds) && seconds > 0
    ? seconds
    : FALLBACK_MAX_AGE_SECONDS;
}
