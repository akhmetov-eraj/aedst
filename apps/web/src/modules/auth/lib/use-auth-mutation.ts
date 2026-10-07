"use client";

import { useMutation } from "@tanstack/react-query";
import { useTranslations } from "next-intl";

import { useRouter } from "@/app/config/i18n/navigation";
import type { AuthActionResult } from "@/modules/auth/lib/auth-actions";
import { getDeviceId } from "@/modules/auth/lib/device-id";
import { useAuth } from "@/shared/lib/auth/auth-provider";
import { safeRedirectPath } from "@/shared/lib/auth/safe-redirect";

type Credentials = { email: string; password: string };

class AuthActionError extends Error {}

/**
 * Shared submit logic for the login/register forms: runs the Server Action
 * (which sets the httpOnly cookies), mirrors the outcome into `AuthProvider`,
 * then navigates to the validated post-login destination. A failed result —
 * including a "success" without tokens — becomes a mutation error the form
 * renders inline, instead of a signed-in state holding blank credentials.
 */
export function useAuthMutation(
  action: (
    input: Credentials & { deviceId: string },
  ) => Promise<AuthActionResult>,
  redirectTo: string | undefined,
) {
  const t = useTranslations("common.errors");
  const router = useRouter();
  const { signIn } = useAuth();

  return useMutation({
    // The form shows its own inline error, so skip the global error toast.
    meta: { silent: true },
    mutationFn: async (credentials: Credentials) => {
      const result = await action({ ...credentials, deviceId: getDeviceId() });
      if (!result.ok) {
        throw new AuthActionError(
          result.error === "rejected" && result.message
            ? result.message
            : t("generic"),
        );
      }
      return result;
    },
    onSuccess: (result) => {
      signIn(result.userId);
      router.replace(safeRedirectPath(redirectTo));
      router.refresh();
    },
  });
}
