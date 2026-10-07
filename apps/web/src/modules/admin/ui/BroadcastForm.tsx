"use client";

import { useMutation } from "@connectrpc/connect-query";
import { zodResolver } from "@hookform/resolvers/zod";
import { useTranslations } from "next-intl";
import { useId } from "react";
import { useForm } from "react-hook-form";
import { toast } from "sonner";
import * as z from "zod/mini";

import { AdminService } from "@/shared/api/connect/client";
import { track } from "@/shared/lib/analytics/analytics";
import { useHydrated } from "@/shared/lib/hooks/use-hydrated";
import { Button } from "@/shared/ui/button";
import { Input } from "@/shared/ui/input";
import { Textarea } from "@/shared/ui/textarea";

// Mirrors admin.proto's BroadcastNotificationRequest constraints.
const UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const broadcastSchema = z.object({
  userId: z.string().check(
    z.trim(),
    z.refine((value) => value === "" || UUID.test(value), "userId"),
  ),
  title: z.string().check(z.trim(), z.minLength(1), z.maxLength(120)),
  body: z.string().check(z.maxLength(1000)),
  link: z.string().check(
    z.maxLength(500),
    z.refine((value) => value === "" || /^\/[^/].*$/.test(value), "link"),
  ),
});
type BroadcastValues = z.infer<typeof broadcastSchema>;

/** Sends a SYSTEM notification to one user, or everyone when no user id is given. */
export function BroadcastForm() {
  const t = useTranslations("admin.broadcast");
  const hydrated = useHydrated();
  const ids = { user: useId(), title: useId(), body: useId(), link: useId() };
  const form = useForm<BroadcastValues>({
    resolver: zodResolver(broadcastSchema),
    defaultValues: { userId: "", title: "", body: "", link: "" },
  });
  const broadcast = useMutation(AdminService.method.broadcastNotification, {
    meta: { feature: "admin" },
    onSuccess: (data, variables) => {
      track("admin_broadcast_sent", {
        audience: variables.userId ? "user" : "everyone",
      });
      toast.success(t("sent", { count: Number(data.recipients) }));
      form.reset();
    },
  });
  const errors = form.formState.errors;

  return (
    <form
      noValidate
      className="flex max-w-xl flex-col gap-4"
      onSubmit={form.handleSubmit((values) =>
        broadcast.mutate({
          userId: values.userId.trim(),
          title: values.title.trim(),
          body: values.body,
          link: values.link,
        }),
      )}
    >
      <Field
        id={ids.user}
        label={t("userId")}
        hint={t("userIdHint")}
        error={errors.userId ? t("userIdInvalid") : undefined}
      >
        <Input id={ids.user} autoComplete="off" {...form.register("userId")} />
      </Field>
      <Field
        id={ids.title}
        label={t("titleLabel")}
        error={errors.title ? t("titleInvalid") : undefined}
      >
        <Input id={ids.title} {...form.register("title")} />
      </Field>
      <Field
        id={ids.body}
        label={t("body")}
        error={errors.body ? t("bodyInvalid") : undefined}
      >
        <Textarea id={ids.body} {...form.register("body")} />
      </Field>
      <Field
        id={ids.link}
        label={t("link")}
        hint={t("linkHint")}
        error={errors.link ? t("linkInvalid") : undefined}
      >
        <Input id={ids.link} placeholder="/tasks" {...form.register("link")} />
      </Field>
      <Button
        type="submit"
        className="w-fit"
        disabled={!hydrated || broadcast.isPending}
      >
        {t("send")}
      </Button>
    </form>
  );
}

function Field({
  id,
  label,
  hint,
  error,
  children,
}: {
  id: string;
  label: string;
  hint?: string;
  error?: string;
  children: React.ReactNode;
}) {
  return (
    <div className="flex flex-col gap-1">
      <label htmlFor={id} className="font-medium text-sm">
        {label}
      </label>
      {children}
      {hint && <p className="text-muted-foreground text-xs">{hint}</p>}
      {error && (
        <p role="alert" className="text-destructive text-xs">
          {error}
        </p>
      )}
    </div>
  );
}
