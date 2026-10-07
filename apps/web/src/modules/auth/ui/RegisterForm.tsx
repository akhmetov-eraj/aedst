"use client";

import { zodResolver } from "@hookform/resolvers/zod";
import { useTranslations } from "next-intl";
import { useId } from "react";
import { useForm } from "react-hook-form";
import * as z from "zod/mini";

import { registerAction } from "@/modules/auth/lib/auth-actions";
import { useAuthMutation } from "@/modules/auth/lib/use-auth-mutation";
import { useHydrated } from "@/shared/lib/hooks/use-hydrated";
import { Button } from "@/shared/ui/button";
import { Input } from "@/shared/ui/input";
import { Label } from "@/shared/ui/label";

// Mirrors apps/server's RegisterRequest validation (buf.validate.field.string.min_len
// = 8 on auth.proto) — client-side validation is UX, not a substitute for the
// server's own check (see docs/content/architecture.md's forms guidance).
const registerSchema = z.object({
  email: z.email(),
  password: z.string().check(z.minLength(8)),
});

type RegisterFormValues = z.infer<typeof registerSchema>;

export function RegisterForm({
  onSwitchToLogin,
  redirectTo,
}: {
  onSwitchToLogin: () => void;
  redirectTo?: string;
}) {
  const t = useTranslations("auth.register");
  const hydrated = useHydrated();
  const emailId = useId();
  const passwordId = useId();

  const {
    register,
    handleSubmit,
    formState: { errors },
  } = useForm<RegisterFormValues>({
    resolver: zodResolver(registerSchema),
  });

  const registerMutation = useAuthMutation(registerAction, redirectTo);

  const onSubmit = handleSubmit((values) => registerMutation.mutate(values));

  return (
    <form
      onSubmit={onSubmit}
      // Never GET: a submit that beats hydration must not put the password in
      // the URL (the button is also disabled until hydrated, see useHydrated).
      method="post"
      className="flex w-full max-w-sm flex-col gap-3"
      noValidate
    >
      <h2 className="font-medium text-lg">{t("title")}</h2>

      <div className="flex flex-col gap-1.5">
        <Label htmlFor={emailId}>{t("emailLabel")}</Label>
        <Input
          id={emailId}
          type="email"
          autoComplete="email"
          aria-invalid={errors.email ? true : undefined}
          aria-describedby={errors.email ? `${emailId}-error` : undefined}
          {...register("email")}
        />
        {errors.email && (
          <p id={`${emailId}-error`} className="text-destructive text-sm">
            {t("invalidEmail")}
          </p>
        )}
      </div>

      <div className="flex flex-col gap-1.5">
        <Label htmlFor={passwordId}>{t("passwordLabel")}</Label>
        <Input
          id={passwordId}
          type="password"
          autoComplete="new-password"
          aria-invalid={errors.password ? true : undefined}
          aria-describedby={errors.password ? `${passwordId}-error` : undefined}
          {...register("password")}
        />
        {errors.password && (
          <p id={`${passwordId}-error`} className="text-destructive text-sm">
            {t("passwordTooShort")}
          </p>
        )}
      </div>

      <Button type="submit" disabled={!hydrated || registerMutation.isPending}>
        {t("submit")}
      </Button>

      {registerMutation.isError && (
        <p role="alert" className="text-destructive text-sm">
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
