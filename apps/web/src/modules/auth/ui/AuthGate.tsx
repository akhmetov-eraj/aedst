"use client";

import { useState } from "react";

import { LoginForm } from "@/modules/auth/ui/LoginForm";
import { RegisterForm } from "@/modules/auth/ui/RegisterForm";

/**
 * The home page's content for a signed-out visitor: a Login/Register toggle.
 * Signed-in visitors never reach it — `proxy.ts` redirects them to their
 * destination before the page renders, so there is no client-side redirect
 * effect and no signed-out flash here.
 */
export function AuthGate({ redirectTo }: { redirectTo?: string }) {
  const [mode, setMode] = useState<"login" | "register">("login");

  return mode === "login" ? (
    <LoginForm
      redirectTo={redirectTo}
      onSwitchToRegister={() => setMode("register")}
    />
  ) : (
    <RegisterForm
      redirectTo={redirectTo}
      onSwitchToLogin={() => setMode("login")}
    />
  );
}
