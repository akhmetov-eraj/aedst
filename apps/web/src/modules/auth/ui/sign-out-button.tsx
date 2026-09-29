"use client";

import { useTranslations } from "next-intl";

import { useRouter } from "@/app/config/i18n/navigation";
import { useAuth } from "@/shared/lib/auth/auth-provider";
import { Button } from "@/shared/ui/button";

export function SignOutButton() {
  const t = useTranslations("auth");
  const { logout } = useAuth();
  const router = useRouter();

  return (
    <Button
      variant="outline"
      onClick={() => {
        logout();
        router.replace("/");
      }}
    >
      {t("signOut")}
    </Button>
  );
}
