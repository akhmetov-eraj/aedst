"use client";

import { zodResolver } from "@hookform/resolvers/zod";
import { useTranslations } from "next-intl";
import { useId } from "react";
import { useForm } from "react-hook-form";
import * as z from "zod/mini";

import { loginAction } from "@/modules/auth/lib/auth-actions";
import { useAuthMutation } from "@/modules/auth/lib/use-auth-mutation";
import { useHydrated } from "@/shared/lib/hooks/use-hydrated";
import { Button } from "@/shared/ui/button";
import { Input } from "@/shared/ui/input";
import { Label } from "@/shared/ui/label";

// `zod/mini` (not `zod`): the browser bundle only needs parse + a few checks, and
// the full classic API is ~90 KB gzipped more. Same schemas, same resolver.
const loginSchema = z.object({
  email: z.email(),
  password: z.string().check(z.minLength(1)),
});

type LoginFormValues = z.infer<typeof loginSchema>;

export function LoginForm({
  onSwitchToRegister,
  redirectTo,
}: {
  onSwitchToRegister: () => void;
  redirectTo?: string;
}) {
  const t = useTranslations("auth.login");
  const hydrated = useHydrated();
  const emailId = useId();
  const passwordId = useId();

  const {
    register,
    handleSubmit,
    formState: { errors },
  } = useForm<LoginFormValues>({
    resolver: zodResolver(loginSchema),
  });

  const loginMutation = useAuthMutation(loginAction, redirectTo);

  const onSubmit = handleSubmit((values) => loginMutation.mutate(values));

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
          autoComplete="current-password"
          aria-invalid={errors.password ? true : undefined}
          aria-describedby={errors.password ? `${passwordId}-error` : undefined}
          {...register("password")}
        />
        {errors.password && (
          <p id={`${passwordId}-error`} className="text-destructive text-sm">
            {t("invalidPassword")}
          </p>
        )}
      </div>

      <Button type="submit" disabled={!hydrated || loginMutation.isPending}>
        {t("submit")}
      </Button>

      {loginMutation.isError && (
        <p role="alert" className="text-destructive text-sm">
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
