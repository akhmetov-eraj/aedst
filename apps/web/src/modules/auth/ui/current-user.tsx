"use client";

import { useQuery } from "@connectrpc/connect-query";
import { useTranslations } from "next-intl";

import { AuthService } from "@/shared/api/connect/client";

/**
 * Demonstrates `AuthService.GetCurrentUser` — identity derived server-side
 * from the bearer token the transport's `authInterceptor` attaches, never
 * from a client-supplied id (see tasks.proto's service doc comment for why
 * that pattern matters).
 */
export function CurrentUser() {
  const t = useTranslations("auth");
  const { data, isLoading } = useQuery(AuthService.method.getCurrentUser, {});

  if (isLoading || !data?.user) {
    return null;
  }

  return (
    <p className="text-muted-foreground text-sm">
      {t("signedInAs", { email: data.user.email })}
    </p>
  );
}
