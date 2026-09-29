"use client";

import { useEffect, useState } from "react";
import { useRouter } from "@/app/config/i18n/navigation";
import { LoginForm } from "@/modules/auth/ui/login-form";
import { RegisterForm } from "@/modules/auth/ui/register-form";
import { useAuth } from "@/shared/lib/auth/auth-provider";

/**
 * The home page's content: a Login/Register toggle for a signed-out
 * visitor, redirecting to `/tasks` the moment sign-in succeeds (`useAuth`'s
 * `isAuthenticated` flips as soon as `LoginForm`/`RegisterForm` calls
 * `login()`) — including for a visitor who's already signed in and lands
 * here directly.
 */
export function AuthGate() {
  const router = useRouter();
  const { isAuthenticated, isLoading } = useAuth();
  const [mode, setMode] = useState<"login" | "register">("login");

  useEffect(() => {
    if (!isLoading && isAuthenticated) {
      router.replace("/tasks");
    }
  }, [isLoading, isAuthenticated, router]);

  if (isLoading || isAuthenticated) {
    return null;
  }

  return mode === "login" ? (
    <LoginForm onSwitchToRegister={() => setMode("register")} />
  ) : (
    <RegisterForm onSwitchToLogin={() => setMode("login")} />
  );
}
