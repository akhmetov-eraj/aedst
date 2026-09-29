"use client";

import { useEffect } from "react";

import { useRouter } from "@/app/config/i18n/navigation";
import { useAuth } from "@/shared/lib/auth/auth-provider";

/** Redirects a signed-out visitor to `/` — the inverse of `AuthGate`. */
export function RequireAuth({ children }: { children: React.ReactNode }) {
  const router = useRouter();
  const { isAuthenticated, isLoading } = useAuth();

  useEffect(() => {
    if (!isLoading && !isAuthenticated) {
      router.replace("/");
    }
  }, [isLoading, isAuthenticated, router]);

  if (isLoading || !isAuthenticated) {
    return null;
  }

  return <>{children}</>;
}
