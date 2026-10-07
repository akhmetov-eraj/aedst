"use server";

import { ConnectError } from "@connectrpc/connect";
import { cookies } from "next/headers";
import { z } from "zod";

import { createServerAuthClient } from "@/shared/api/connect/server";
import {
  clearSessionCookies,
  REFRESH_COOKIE,
  writeSessionCookies,
} from "@/shared/lib/auth/session-cookies";
import { sessionTokensFromPair } from "@/shared/lib/auth/session-tokens";

/**
 * Sign-in/up/out as Server Actions: the browser posts credentials to *this*
 * app, the server talks to apps/server, and the resulting tokens go straight
 * into httpOnly cookies. Only `{ ok, userId }` (or an error) ever returns to
 * the page. Server Actions also carry Next's built-in Origin check, which
 * covers CSRF for these mutations.
 */

export type AuthActionResult =
  | { ok: true; userId: string }
  | {
      ok: false;
      error: "invalidInput" | "noTokens" | "rejected" | "failed";
      message?: string;
    };

const credentialsSchema = z.object({
  email: z.email().max(254),
  password: z.string().min(1).max(128),
  deviceId: z.string().min(1).max(128),
});

const registrationSchema = credentialsSchema.extend({
  password: z.string().min(8).max(128),
});

function failure(error: unknown): AuthActionResult {
  const connectError = ConnectError.from(error);
  return connectError.rawMessage
    ? { ok: false, error: "rejected", message: connectError.rawMessage }
    : { ok: false, error: "failed" };
}

export async function loginAction(input: unknown): Promise<AuthActionResult> {
  const parsed = credentialsSchema.safeParse(input);
  if (!parsed.success) {
    return { ok: false, error: "invalidInput" };
  }

  try {
    const response = await createServerAuthClient().login(parsed.data);
    const tokens = sessionTokensFromPair(response.tokens);
    if (!tokens || !response.userId) {
      return { ok: false, error: "noTokens" };
    }
    writeSessionCookies(await cookies(), tokens);
    return { ok: true, userId: response.userId };
  } catch (error) {
    return failure(error);
  }
}

export async function registerAction(
  input: unknown,
): Promise<AuthActionResult> {
  const parsed = registrationSchema.safeParse(input);
  if (!parsed.success) {
    return { ok: false, error: "invalidInput" };
  }

  try {
    const response = await createServerAuthClient().register(parsed.data);
    const tokens = sessionTokensFromPair(response.tokens);
    if (!tokens || !response.user?.id) {
      return { ok: false, error: "noTokens" };
    }
    writeSessionCookies(await cookies(), tokens);
    return { ok: true, userId: response.user.id };
  } catch (error) {
    return failure(error);
  }
}

export async function logoutAction(): Promise<void> {
  const jar = await cookies();
  const refreshToken = jar.get(REFRESH_COOKIE)?.value;
  clearSessionCookies(jar);

  if (refreshToken) {
    try {
      await createServerAuthClient().logout({ refreshToken });
    } catch {
      // Best effort: the cookies are already gone, so this browser is signed
      // out; the server-side session simply expires on its own.
    }
  }
}
