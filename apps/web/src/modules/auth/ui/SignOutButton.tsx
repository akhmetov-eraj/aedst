"use client";

import { useMutation } from "@tanstack/react-query";
import { useTranslations } from "next-intl";

import { useRouter } from "@/app/config/i18n/navigation";
import { logoutAction } from "@/modules/auth/lib/auth-actions";
import { useAuth } from "@/shared/lib/auth/auth-provider";
import { runBeforeSignOut } from "@/shared/lib/auth/sign-out-hooks";
import { Button } from "@/shared/ui/button";

export function SignOutButton() {
  const t = useTranslations("auth");
  const { signOut } = useAuth();
  const router = useRouter();
  const logout = useMutation({
    meta: { silent: true },
    // Cleanup that needs the session (push unregistration, …) runs first.
    mutationFn: async () => {
      await runBeforeSignOut();
      await logoutAction();
    },
    // Signed out locally whether or not the server-side revoke succeeded:
    // the cookies are already cleared by then.
    onSettled: () => {
      signOut();
      router.replace("/");
      router.refresh();
    },
  });

  return (
    <Button
      variant="outline"
      disabled={logout.isPending}
      onClick={() => logout.mutate()}
    >
      {t("signOut")}
    </Button>
  );
}
