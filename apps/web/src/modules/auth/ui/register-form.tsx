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

// Mirrors apps/server's RegisterRequest validation (buf.validate.field.string.min_len
// = 8 on auth.proto) — client-side validation is UX, not a substitute for the
// server's own check (see docs/content/architecture.md's forms guidance).
const registerSchema = z.object({
  email: z.email(),
  password: z.string().min(8),
});

type RegisterFormValues = z.infer<typeof registerSchema>;

export function RegisterForm({
  onSwitchToLogin,
}: {
  onSwitchToLogin: () => void;
}) {
  const t = useTranslations("auth.register");
  const { login } = useAuth();
  const emailId = useId();
  const passwordId = useId();

  const {
    register,
    handleSubmit,
    formState: { errors },
  } = useForm<RegisterFormValues>({
    resolver: zodResolver(registerSchema),
  });

  const registerMutation = useMutation(AuthService.method.register);

  const onSubmit = handleSubmit((values) => {
    registerMutation.mutate(
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
          <p className="text-destructive text-sm">{t("passwordTooShort")}</p>
        )}
      </div>

      <Button type="submit" disabled={registerMutation.isPending}>
        {t("submit")}
      </Button>

      {registerMutation.isError && (
        <p className="text-destructive text-sm">
          {t("error", { message: registerMutation.error.message })}
        </p>
      )}

      <p className="text-muted-foreground text-sm">
        {t("switchPrompt")}{" "}
        <Button
          type="button"
          variant="link"
          onClick={onSwitchToLogin}
          className="h-auto p-0 font-medium"
        >
          {t("switchAction")}
        </Button>
      </p>
    </form>
  );
}
