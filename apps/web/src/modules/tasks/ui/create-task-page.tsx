"use client";

import { timestampFromDate } from "@bufbuild/protobuf/wkt";
import { useMutation } from "@connectrpc/connect-query";
import { useTranslations } from "next-intl";

import { Link, useRouter } from "@/app/config/i18n/navigation";
import { TaskForm, type TaskFormValues } from "@/modules/tasks/ui/task-form";
import { TasksService } from "@/shared/api/connect/client";
import { Button } from "@/shared/ui/button";

export function CreateTaskPage() {
  const t = useTranslations("tasks");
  const router = useRouter();
  const createTask = useMutation(TasksService.method.createTask);

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
          if (res.task) {
            router.push(`/tasks/${res.task.id}`);
          }
        },
      },
    );
  }

  return (
    <div className="flex flex-col gap-4">
      <Button
        render={<Link href="/tasks" />}
        nativeButton={false}
        variant="ghost"
        className="w-fit"
      >
        {t("details.backToList")}
      </Button>
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
