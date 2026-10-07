"use client";

import { timestampFromDate } from "@bufbuild/protobuf/wkt";
import { useMutation } from "@connectrpc/connect-query";
import { useTranslations } from "next-intl";

import { Link, useRouter } from "@/app/config/i18n/navigation";
import { TaskForm, type TaskFormValues } from "@/modules/tasks/ui/TaskForm";
import { TasksService } from "@/shared/api/connect/client";
import { TaskPriority } from "@/shared/api/connect/generated/aedst/tasks/v1/tasks_pb";
import { track } from "@/shared/lib/analytics/analytics";
import { buttonVariants } from "@/shared/ui/button";

export function CreateTaskPage() {
  const t = useTranslations("tasks");
  const router = useRouter();
  const createTask = useMutation(TasksService.method.createTask, {
    // The form renders its own inline error.
    meta: { silent: true, feature: "tasks" },
  });

  function handleSubmit(values: TaskFormValues) {
    createTask.mutate(
      {
        title: values.title,
        description: values.description,
        priority: values.priority,
        dueDate: values.dueDate
          ? timestampFromDate(new Date(values.dueDate))
          : undefined,
      },
      {
        onSuccess: (res) => {
          track("task_created", {
            priority: TaskPriority[values.priority] ?? "UNKNOWN",
            has_due_date: Boolean(values.dueDate),
          });
          if (res.task) {
            router.push(`/tasks/${res.task.id}`);
          }
        },
      },
    );
  }

  return (
    <div className="flex flex-col gap-4">
      <Link
        href="/tasks"
        className={buttonVariants({ variant: "ghost", className: "w-fit" })}
      >
        {t("details.backToList")}
      </Link>
      <h1 className="font-semibold text-xl">{t("form.createTitle")}</h1>
      <TaskForm
        mode="create"
        onSubmit={handleSubmit}
        isSubmitting={createTask.isPending}
        submitError={createTask.error?.message}
      />
    </div>
  );
}
