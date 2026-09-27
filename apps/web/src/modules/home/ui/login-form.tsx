"use client";

import { zodResolver } from "@hookform/resolvers/zod";
import { useMutation } from "@connectrpc/connect-query";
import { useTranslations } from "next-intl";
import { useId, useState } from "react";
import { useForm } from "react-hook-form";
import { z } from "zod";

import { AuthService } from "@/shared/api/connect/client";
import { Button } from "@/shared/ui/button";
import { Input } from "@/shared/ui/input";
import { Label } from "@/shared/ui/label";

const loginSchema = z.object({
  email: z.email(),
  password: z.string().min(1),
});

type LoginFormValues = z.infer<typeof loginSchema>;

/**
 * A minimal, real example of calling apps/server directly over ConnectRPC
 * from the browser (see docs/decisions/0002-connectrpc.md) — this replaces
 * the placeholder `Health` GraphQL query that used to live here. It exercises
 * `AuthService.Login` via `@connectrpc/connect-query`'s `useMutation`, which
 * wraps the generated client in a TanStack Query mutation.
 */
export function LoginForm() {
  const t = useTranslations("home.auth");
  const emailId = useId();
  const passwordId = useId();
  const [deviceId] = useState(
    () => `web-${Math.random().toString(36).slice(2)}`,
  );

  const {
    register,
    handleSubmit,
    formState: { errors },
  } = useForm<LoginFormValues>({
    resolver: zodResolver(loginSchema),
  });

  const login = useMutation(AuthService.method.login);

  const onSubmit = handleSubmit((values) => {
    login.mutate({ ...values, deviceId });
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

      <Button type="submit" disabled={login.isPending}>
        {t("submit")}
      </Button>

      {login.isSuccess && (
        <p className="text-sm">{t("success", { userId: login.data.userId })}</p>
      )}
      {login.isError && (
        <p className="text-destructive text-sm">
          {t("error", { message: login.error.message })}
        </p>
      )}
    </form>
  );
}
