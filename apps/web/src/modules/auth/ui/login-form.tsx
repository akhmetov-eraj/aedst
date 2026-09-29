"use client";

import { useMutation } from "@connectrpc/connect-query";
import { zodResolver } from "@hookform/resolvers/zod";
import { useTranslations } from "next-intl";
import { useId } from "react";
import { useForm } from "react-hook-form";
import { z } from "zod";

import { deviceId } from "@/modules/auth/lib/device-id";
import { AuthService } from "@/shared/api/connect/client";
import { useAuth } from "@/shared/lib/auth/auth-provider";
import { Button } from "@/shared/ui/button";
import { Input } from "@/shared/ui/input";
import { Label } from "@/shared/ui/label";

const loginSchema = z.object({
  email: z.email(),
  password: z.string().min(1),
});

type LoginFormValues = z.infer<typeof loginSchema>;

/**
 * Exercises `AuthService.Login` via `@connectrpc/connect-query`'s
 * `useMutation` (see docs/content/decisions/0002-connectrpc.md), then
 * persists the returned tokens through `useAuth()` — see
 * shared/lib/auth/token-storage.ts for why localStorage, not a cookie.
 */
export function LoginForm({
  onSwitchToRegister,
}: {
  onSwitchToRegister: () => void;
}) {
  const t = useTranslations("auth.login");
  const { login } = useAuth();
  const emailId = useId();
  const passwordId = useId();

  const {
    register,
    handleSubmit,
    formState: { errors },
  } = useForm<LoginFormValues>({
    resolver: zodResolver(loginSchema),
  });

  const loginMutation = useMutation(AuthService.method.login);

  const onSubmit = handleSubmit((values) => {
    loginMutation.mutate(
      { ...values, deviceId: deviceId() },
      {
        onSuccess: (res) =>
          login({
            accessToken: res.tokens?.accessToken ?? "",
            refreshToken: res.tokens?.refreshToken ?? "",
          }),
      },
    );
  });

  return (
    <form
      onSubmit={onSubmit}
      className="flex w-full max-w-sm flex-col gap-3"
      noValidate
    >
      <h2 className="font-medium text-lg">{t("title")}</h2>

      <div className="flex flex-col gap-1.5">
        <Label htmlFor={emailId}>{t("emailLabel")}</Label>
        <Input id={emailId} type="email" {...register("email")} />
        {errors.email && (
          <p className="text-destructive text-sm">{t("invalidEmail")}</p>
        )}
      </div>

      <div className="flex flex-col gap-1.5">
        <Label htmlFor={passwordId}>{t("passwordLabel")}</Label>
        <Input id={passwordId} type="password" {...register("password")} />
        {errors.password && (
          <p className="text-destructive text-sm">{t("invalidPassword")}</p>
        )}
      </div>

      <Button type="submit" disabled={loginMutation.isPending}>
        {t("submit")}
      </Button>

      {loginMutation.isError && (
        <p className="text-destructive text-sm">
          {t("error", { message: loginMutation.error.message })}
        </p>
      )}

      <p className="text-muted-foreground text-sm">
        {t("switchPrompt")}{" "}
        <Button
          type="button"
          variant="link"
          onClick={onSwitchToRegister}
          className="h-auto p-0 font-medium"
        >
          {t("switchAction")}
        </Button>
      </p>
    </form>
  );
}
