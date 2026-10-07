import "server-only";

import { Code, ConnectError } from "@connectrpc/connect";

import { createServerAuthClient } from "@/shared/api/connect/server";
import type { SessionTokens } from "@/shared/lib/auth/session-cookies";
import { sessionTokensFromPair } from "@/shared/lib/auth/session-tokens";

export type RefreshResult =
  | { status: "ok"; tokens: SessionTokens }
  | { status: "rejected" }
  | { status: "unavailable" };

/**
 * Refresh tokens rotate, so two concurrent requests presenting the same
 * (old) token would race: one wins, the other is rejected and would log the
 * user out. Concurrent calls therefore share one in-flight promise, and a
 * successful result stays reusable for a few seconds for requests that were
 * already on the wire with the old cookie. In-process only — a multi-replica
 * deployment relies on sticky routing or a server-side reuse grace period.
 */
const REUSE_WINDOW_MS = 10_000;
const cache = new Map<string, Promise<RefreshResult>>();

export function refreshSession(refreshToken: string): Promise<RefreshResult> {
  const existing = cache.get(refreshToken);
  if (existing) {
    return existing;
  }

  const attempt = performRefresh(refreshToken);
  cache.set(refreshToken, attempt);
  void attempt.then((result) => {
    const ttl = result.status === "ok" ? REUSE_WINDOW_MS : 0;
    setTimeout(() => cache.delete(refreshToken), ttl).unref?.();
  });
  return attempt;
}

async function performRefresh(refreshToken: string): Promise<RefreshResult> {
  try {
    const response = await createServerAuthClient().refreshToken({
      refreshToken,
    });
    const tokens = sessionTokensFromPair(response.tokens);
    return tokens ? { status: "ok", tokens } : { status: "rejected" };
  } catch (error) {
    const code = ConnectError.from(error).code;
    const definitive =
      code === Code.Unauthenticated ||
      code === Code.PermissionDenied ||
      code === Code.InvalidArgument ||
      code === Code.NotFound;
    return { status: definitive ? "rejected" : "unavailable" };
  }
}
