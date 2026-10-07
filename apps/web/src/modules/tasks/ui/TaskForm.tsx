"use client";

import { zodResolver } from "@hookform/resolvers/zod";
import { useTranslations } from "next-intl";
import { useForm } from "react-hook-form";
import * as z from "zod/mini";

import { PRIORITY_KEYS } from "@/modules/tasks/lib/labels";
import { TaskPriority } from "@/shared/api/connect/generated/aedst/tasks/v1/tasks_pb";
import { useHydrated } from "@/shared/lib/hooks/use-hydrated";
import { Button } from "@/shared/ui/button";
import { Input } from "@/shared/ui/input";
import { Label } from "@/shared/ui/label";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/shared/ui/select";
import { Textarea } from "@/shared/ui/textarea";

// Mirrors apps/server's CreateTaskRequest/UpdateTaskRequest protovalidate
// constraints (tasks.proto) — client-side validation is UX, the server's is
// the actual guarantee; see docs/content/architecture.md's forms guidance.
const taskFormSchema = z.object({
  title: z.string().check(z.trim(), z.minLength(1), z.maxLength(200)),
  description: z.string().check(z.maxLength(4000)),
  priority: z.union([
    z.literal(TaskPriority.LOW),
    z.literal(TaskPriority.MEDIUM),
    z.literal(TaskPriority.HIGH),
    z.literal(TaskPriority.URGENT),
  ]),
  dueDate: z.string(), // yyyy-mm-dd from <input type="date">, or "".
});

export type TaskFormValues = z.infer<typeof taskFormSchema>;
export type SettablePriority = TaskFormValues["priority"];

const PRIORITY_OPTIONS: SettablePriority[] = [
  TaskPriority.LOW,
  TaskPriority.MEDIUM,
  TaskPriority.HIGH,
  TaskPriority.URGENT,
];

export function TaskForm({
  mode,
  defaultValues,
  onSubmit,
  isSubmitting,
  submitError,
}: {
  mode: "create" | "edit";
  defaultValues?: Partial<TaskFormValues>;
  onSubmit: (values: TaskFormValues) => void;
  isSubmitting: boolean;
  submitError?: string;
}) {
  const hydrated = useHydrated();
  const t = useTranslations("tasks.form");
  const tPriority = useTranslations("tasks.priority");

  const {
    register,
    handleSubmit,
    watch,
    setValue,
    formState: { errors },
  } = useForm<TaskFormValues>({
    resolver: zodResolver(taskFormSchema),
    defaultValues: {
      title: "",
      description: "",
      priority: TaskPriority.MEDIUM,
      dueDate: "",
      ...defaultValues,
    },
  });

  const priority = watch("priority");

  function priorityLabel(value: SettablePriority) {
    const key = PRIORITY_KEYS[value];
    return key ? tPriority(key) : "";
  }

  return (
    <form
      onSubmit={handleSubmit(onSubmit)}
      method="post"
      className="flex max-w-lg flex-col gap-4"
      noValidate
    >
      <div className="flex flex-col gap-1.5">
        <Label htmlFor="title">{t("titleLabel")}</Label>
        <Input
          id="title"
          aria-invalid={errors.title ? true : undefined}
          aria-describedby={errors.title ? "title-error" : undefined}
          {...register("title")}
        />
        {errors.title && (
          <p id="title-error" className="text-destructive text-sm">
            {t("titleRequired")}
          </p>
        )}
      </div>

      <div className="flex flex-col gap-1.5">
        <Label htmlFor="description">{t("descriptionLabel")}</Label>
        <Textarea
          id="description"
          rows={4}
          aria-invalid={errors.description ? true : undefined}
          aria-describedby={
            errors.description ? "description-error" : undefined
          }
          {...register("description")}
        />
        {errors.description && (
          <p id="description-error" className="text-destructive text-sm">
            {t("descriptionTooLong")}
          </p>
        )}
      </div>

      <div className="flex flex-col gap-1.5">
        <Label htmlFor="priority">{t("priorityLabel")}</Label>
        <Select
          items={PRIORITY_OPTIONS.map((value) => ({
            value: String(value),
            label: priorityLabel(value),
          }))}
          value={String(priority)}
          onValueChange={(value) =>
            setValue("priority", Number(value) as SettablePriority)
          }
        >
          <SelectTrigger id="priority">
            <SelectValue />
          </SelectTrigger>
          <SelectContent>
            {PRIORITY_OPTIONS.map((value) => (
              <SelectItem key={value} value={String(value)}>
                {priorityLabel(value)}
              </SelectItem>
            ))}
          </SelectContent>
        </Select>
      </div>

      <div className="flex flex-col gap-1.5">
        <Label htmlFor="dueDate">
          {t("dueDateLabel")}{" "}
          <span className="text-muted-foreground">
            ({t("dueDateOptional")})
          </span>
        </Label>
        <Input id="dueDate" type="date" {...register("dueDate")} />
      </div>

      <div className="flex items-center gap-2">
        <Button type="submit" disabled={!hydrated || isSubmitting}>
          {mode === "create" ? t("create") : t("save")}
        </Button>
      </div>

      {submitError && (
        <p role="alert" className="text-destructive text-sm">
          {t("error", { message: submitError })}
        </p>
      )}
    </form>
  );
}
