import type { Interceptor } from "@connectrpc/connect";

import { loadTokens } from "@/shared/lib/auth/token-storage";

/**
 * Attaches `Authorization: Bearer <accessToken>` from client-side token
 * storage to every outgoing call. Cheap to apply unconditionally: the four
 * public AuthService procedures (Register/Login/RefreshToken/Logout) never
 * check it server-side (see apps/server's `publicProcedures`), and every
 * other procedure requires it — there's no call this would wrongly affect.
 */
export const authInterceptor: Interceptor = (next) => async (req) => {
  const tokens = loadTokens();
  if (tokens) {
    req.header.set("Authorization", `Bearer ${tokens.accessToken}`);
  }
  return await next(req);
};
