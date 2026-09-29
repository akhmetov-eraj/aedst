"use client";

import { timestampDate, timestampFromDate } from "@bufbuild/protobuf/wkt";
import { useMutation, useQuery } from "@connectrpc/connect-query";
import { useQueryClient } from "@tanstack/react-query";
import { format } from "date-fns";
import { useLocale, useTranslations } from "next-intl";
import { useState } from "react";
import type { Locale } from "@/app/config/i18n/locales";
import { Link, useRouter } from "@/app/config/i18n/navigation";
import { PriorityBadge } from "@/modules/tasks/ui/priority-badge";
import { StatusBadge } from "@/modules/tasks/ui/status-badge";
import {
  type SettablePriority,
  TaskForm,
  type TaskFormValues,
} from "@/modules/tasks/ui/task-form";
import { TasksService } from "@/shared/api/connect/client";
import { TaskStatus } from "@/shared/api/connect/generated/aedst/tasks/v1/tasks_pb";
import { formatDate, formatDateTime } from "@/shared/date/format-date";
import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
  AlertDialogTrigger,
} from "@/shared/ui/alert-dialog";
import { Button } from "@/shared/ui/button";
import {
  Empty,
  EmptyDescription,
  EmptyHeader,
  EmptyTitle,
} from "@/shared/ui/empty";
import { Skeleton } from "@/shared/ui/skeleton";

function toDateInputValue(date: Date | undefined) {
  return date ? format(date, "yyyy-MM-dd") : "";
}

export function TaskDetails({ taskId }: { taskId: string }) {
  const t = useTranslations("tasks.details");
  const tForm = useTranslations("tasks.form");
  const locale = useLocale() as Locale;
  const router = useRouter();
  const queryClient = useQueryClient();
  const [isEditing, setIsEditing] = useState(false);

  const { data, isLoading, isError } = useQuery(TasksService.method.getTask, {
    id: taskId,
  });

  // Every mutation below invalidates the whole query cache — simple and
  // always correct (this task, the list, and the dashboard stats can all
  // change from any of these actions) rather than hand-maintaining a
  // precise set of query keys for a handful of screens.
  function invalidateAll() {
    return queryClient.invalidateQueries();
  }

  const updateTask = useMutation(TasksService.method.updateTask, {
    onSuccess: () => {
      setIsEditing(false);
      return invalidateAll();
    },
  });
  const completeTask = useMutation(TasksService.method.completeTask, {
    onSuccess: invalidateAll,
  });
  const reopenTask = useMutation(TasksService.method.reopenTask, {
    onSuccess: invalidateAll,
  });
  const deleteTask = useMutation(TasksService.method.deleteTask, {
    onSuccess: () => {
      void invalidateAll();
      router.push("/tasks");
    },
  });

  if (isLoading) {
    return <Skeleton className="h-64 w-full max-w-lg" />;
  }

  if (isError || !data?.task) {
    return (
      <Empty>
        <EmptyHeader>
          <EmptyTitle>{t("notFoundTitle")}</EmptyTitle>
          <EmptyDescription>{t("notFoundDescription")}</EmptyDescription>
        </EmptyHeader>
      </Empty>
    );
  }

  const task = data.task;

  if (isEditing) {
    return (
      <TaskForm
        mode="edit"
        defaultValues={{
          title: task.title,
          description: task.description,
          priority: task.priority as SettablePriority,
          dueDate: toDateInputValue(
            task.dueDate ? timestampDate(task.dueDate) : undefined,
          ),
        }}
        isSubmitting={updateTask.isPending}
        submitError={updateTask.error?.message}
        onSubmit={(values: TaskFormValues) =>
          updateTask.mutate({
            id: task.id,
            title: values.title,
            description: values.description,
            priority: values.priority,
            dueDate: values.dueDate
              ? timestampFromDate(new Date(values.dueDate))
              : undefined,
            updateMask: {
              paths: ["title", "description", "priority", "due_date"],
            },
          })
        }
      />
    );
  }

  return (
    <div className="flex max-w-lg flex-col gap-4">
      <Button
        render={<Link href="/tasks" />}
        nativeButton={false}
        variant="ghost"
        className="w-fit"
      >
        {t("backToList")}
      </Button>

      <div className="flex items-start justify-between gap-3">
        <h1 className="font-semibold text-xl">{task.title}</h1>
        <div className="flex shrink-0 items-center gap-2">
          <PriorityBadge priority={task.priority} />
          <StatusBadge status={task.status} />
        </div>
      </div>

      {task.description && (
        <p className="whitespace-pre-wrap text-muted-foreground">
          {task.description}
        </p>
      )}

      <dl className="grid grid-cols-2 gap-x-4 gap-y-2 text-sm">
        <dt className="text-muted-foreground">{t("dueDate")}</dt>
        <dd>
          {task.dueDate
            ? formatDate(timestampDate(task.dueDate), locale)
            : t("noDueDate")}
        </dd>
        <dt className="text-muted-foreground">{t("createdBy")}</dt>
        <dd>{t("you")}</dd>
        <dt className="text-muted-foreground">{t("createdAt")}</dt>
        <dd>
          {task.createdAt
            ? formatDateTime(timestampDate(task.createdAt), locale)
            : "—"}
        </dd>
        <dt className="text-muted-foreground">{t("updatedAt")}</dt>
        <dd>
          {task.updatedAt
            ? formatDateTime(timestampDate(task.updatedAt), locale)
            : "—"}
        </dd>
      </dl>

      <div className="flex flex-wrap items-center gap-2">
        <Button variant="outline" onClick={() => setIsEditing(true)}>
          {t("edit")}
        </Button>

        {task.status === TaskStatus.DONE ? (
          <Button
            variant="outline"
            onClick={() => reopenTask.mutate({ id: task.id })}
            disabled={reopenTask.isPending}
          >
            {t("reopen")}
          </Button>
        ) : (
          <Button
            onClick={() => completeTask.mutate({ id: task.id })}
            disabled={completeTask.isPending}
          >
            {t("complete")}
          </Button>
        )}

        <AlertDialog>
          <AlertDialogTrigger render={<Button variant="destructive" />}>
            {t("delete")}
          </AlertDialogTrigger>
          <AlertDialogContent>
            <AlertDialogHeader>
              <AlertDialogTitle>{t("deleteConfirmTitle")}</AlertDialogTitle>
              <AlertDialogDescription>
                {t("deleteConfirmDescription")}
              </AlertDialogDescription>
            </AlertDialogHeader>
            <AlertDialogFooter>
              <AlertDialogCancel>{t("deleteCancelAction")}</AlertDialogCancel>
              <AlertDialogAction
                onClick={() => deleteTask.mutate({ id: task.id })}
              >
                {t("deleteConfirmAction")}
              </AlertDialogAction>
            </AlertDialogFooter>
          </AlertDialogContent>
        </AlertDialog>
      </div>

      {(updateTask.isError ||
        completeTask.isError ||
        reopenTask.isError ||
        deleteTask.isError) && (
        <p className="text-destructive text-sm">
          {tForm("error", {
            message:
              (
                updateTask.error ??
                completeTask.error ??
                reopenTask.error ??
                deleteTask.error
              )?.message ?? "",
          })}
        </p>
      )}
    </div>
  );
}
