"use client";

/**
 * Client-side session storage for this reference app. Real AEDST products
 * are expected to move to a server-issued httpOnly cookie (see
 * `AUTH_COOKIE_NAME` in `.env.example` / `shared/env/server.ts` —
 * reserved, not read yet) once a Server Actions/Route Handler-based BFF
 * exists; that's a bigger change than this reference module's scope
 * justifies on its own. `localStorage` is the honest middle ground: it
 * survives a page refresh (a cookie-less SPA that didn't would be
 * unusable to actually try), but unlike an httpOnly cookie it's readable
 * by any script on the page — acceptable for a reference/demo app, **not**
 * the pattern to copy for a real deployment without that caveat in mind.
 */
const STORAGE_KEY = "aedst.auth.tokens";

export type StoredTokens = {
  accessToken: string;
  refreshToken: string;
};

export function loadTokens(): StoredTokens | null {
  if (typeof window === "undefined") {
    return null;
  }
  try {
    const raw = window.localStorage.getItem(STORAGE_KEY);
    if (!raw) {
      return null;
    }
    const parsed = JSON.parse(raw) as Partial<StoredTokens>;
    if (
      typeof parsed.accessToken !== "string" ||
      typeof parsed.refreshToken !== "string"
    ) {
      return null;
    }
    return {
      accessToken: parsed.accessToken,
      refreshToken: parsed.refreshToken,
    };
  } catch {
    return null;
  }
}

export function saveTokens(tokens: StoredTokens): void {
  if (typeof window === "undefined") {
    return;
  }
  window.localStorage.setItem(STORAGE_KEY, JSON.stringify(tokens));
  window.dispatchEvent(new StorageEvent("storage", { key: STORAGE_KEY }));
}

export function clearTokens(): void {
  if (typeof window === "undefined") {
    return;
  }
  window.localStorage.removeItem(STORAGE_KEY);
  window.dispatchEvent(new StorageEvent("storage", { key: STORAGE_KEY }));
}
