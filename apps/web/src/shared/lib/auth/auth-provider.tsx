"use client";

import { createContext, use, useCallback, useEffect, useState } from "react";

import {
  clearTokens,
  loadTokens,
  type StoredTokens,
  saveTokens,
} from "@/shared/lib/auth/token-storage";

type AuthContextValue = {
  isAuthenticated: boolean;
  /** True until the first read of localStorage completes (avoids a login-flash on load). */
  isLoading: boolean;
  login: (tokens: StoredTokens) => void;
  logout: () => void;
};

const AuthContext = createContext<AuthContextValue | null>(null);

export function AuthProvider({ children }: { children: React.ReactNode }) {
  const [isAuthenticated, setIsAuthenticated] = useState(false);
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    setIsAuthenticated(loadTokens() !== null);
    setIsLoading(false);

    // Reacts to `token-storage.ts`'s manually-dispatched StorageEvent (this
    // tab) and the browser's own `storage` event (other tabs signing
    // out/in), so every open tab's UI stays in sync with one shared
    // localStorage session.
    const onStorage = () => setIsAuthenticated(loadTokens() !== null);
    window.addEventListener("storage", onStorage);
    return () => window.removeEventListener("storage", onStorage);
  }, []);

  const login = useCallback((tokens: StoredTokens) => {
    saveTokens(tokens);
    setIsAuthenticated(true);
  }, []);

  const logout = useCallback(() => {
    clearTokens();
    setIsAuthenticated(false);
  }, []);

  return (
    <AuthContext value={{ isAuthenticated, isLoading, login, logout }}>
      {children}
    </AuthContext>
  );
}

export function useAuth(): AuthContextValue {
  const ctx = use(AuthContext);
  if (!ctx) {
    throw new Error("useAuth() must be called within <AuthProvider>");
  }
  return ctx;
}
