"use client";

import { timestampDate } from "@bufbuild/protobuf/wkt";
import {
  createConnectQueryKey,
  useMutation,
  useQuery,
} from "@connectrpc/connect-query";
import { zodResolver } from "@hookform/resolvers/zod";
import { keepPreviousData, useQueryClient } from "@tanstack/react-query";
import { useLocale, useTranslations } from "next-intl";
import { useId, useMemo, useState } from "react";
import { useForm } from "react-hook-form";
import * as z from "zod/mini";

import type { Locale } from "@/app/config/i18n/locales";
import { useCursorPages } from "@/modules/admin/lib/use-cursor-pages";
import { type AdminColumn, DataTable } from "@/modules/admin/ui/DataTable";
import { AdminService } from "@/shared/api/connect/client";
import type { AdminTask } from "@/shared/api/connect/generated/aedst/admin/v1/admin_pb";
import { TaskStatus } from "@/shared/api/connect/generated/aedst/tasks/v1/tasks_pb";
import { formatDate } from "@/shared/date/format-date";
import { track } from "@/shared/lib/analytics/analytics";
import { useDebouncedValue } from "@/shared/lib/hooks/use-debounced-value";
import { Button } from "@/shared/ui/button";
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/shared/ui/dialog";
import { Input } from "@/shared/ui/input";
import { Textarea } from "@/shared/ui/textarea";

const PAGE_SIZE = 25;

// Mirrors admin.proto's DeleteTaskRequest.reason (3..500 characters).
const deleteSchema = z.object({
  reason: z.string().check(z.trim(), z.minLength(3), z.maxLength(500)),
});
type DeleteValues = z.infer<typeof deleteSchema>;

const STATUS_KEYS: Partial<Record<number, "todo" | "inProgress" | "done">> = {
  [TaskStatus.TODO]: "todo",
  [TaskStatus.IN_PROGRESS]: "inProgress",
  [TaskStatus.DONE]: "done",
};

/** Every user's tasks (cross-tenant read) with delete-with-reason (audited). */
export function AdminTasks() {
  const t = useTranslations("admin.tasks");
  const locale = useLocale() as Locale;
  const searchId = useId();
  const [search, setSearch] = useState("");
  const query = useDebouncedValue(search.trim(), 300);
  const pages = useCursorPages();
  const [deleting, setDeleting] = useState<AdminTask | null>(null);

  const tasks = useQuery(
    AdminService.method.listTasks,
    { query, pageSize: PAGE_SIZE, pageToken: pages.pageToken },
    { placeholderData: keepPreviousData, meta: { feature: "admin" } },
  );

  const columns = useMemo<AdminColumn<AdminTask>[]>(
    () => [
      { accessorKey: "title", header: t("taskTitle") },
      {
        id: "owner",
        header: t("owner"),
        cell: ({ row }) => (
          <code className="text-xs">{row.original.ownerId.slice(0, 8)}</code>
        ),
      },
      {
        id: "status",
        header: t("status"),
        cell: ({ row }) => {
          const key = STATUS_KEYS[row.original.status];
          return key ? t(`statuses.${key}`) : "—";
        },
      },
      {
        id: "createdAt",
        header: t("createdAt"),
        cell: ({ row }) =>
          row.original.createdAt
            ? formatDate(timestampDate(row.original.createdAt), locale)
            : "—",
      },
      {
        id: "actions",
        header: () => <span className="sr-only">{t("actions")}</span>,
        cell: ({ row }) => (
          <Button
            variant="destructive"
            size="sm"
            onClick={() => setDeleting(row.original)}
          >
            {t("delete")}
          </Button>
        ),
      },
    ],
    [t, locale],
  );

  return (
    <div className="flex flex-col gap-3">
      <label
        htmlFor={searchId}
        className="flex max-w-sm flex-col gap-1 text-sm"
      >
        {t("search")}
        <Input
          id={searchId}
          type="search"
          value={search}
          onChange={(event) => {
            setSearch(event.target.value);
            pages.reset();
          }}
        />
      </label>
      <DataTable
        label={t("title")}
        columns={columns}
        data={tasks.data?.tasks}
        isLoading={tasks.isFetching}
        emptyText={tasks.isError ? t("loadError") : t("empty")}
        hasPrevious={pages.hasPrevious}
        hasNext={Boolean(tasks.data?.nextPageToken)}
        onPrevious={pages.previous}
        onNext={() => pages.next(tasks.data?.nextPageToken ?? "")}
      />
      <DeleteTaskDialog task={deleting} onClose={() => setDeleting(null)} />
    </div>
  );
}

function DeleteTaskDialog({
  task,
  onClose,
}: {
  task: AdminTask | null;
  onClose: () => void;
}) {
  const t = useTranslations("admin.tasks");
  const reasonId = useId();
  const queryClient = useQueryClient();
  const form = useForm<DeleteValues>({
    resolver: zodResolver(deleteSchema),
    defaultValues: { reason: "" },
  });
  const remove = useMutation(AdminService.method.deleteTask, {
    meta: { feature: "admin" },
    onSuccess: async () => {
      track("admin_task_deleted");
      form.reset();
      onClose();
      await queryClient.invalidateQueries({
        queryKey: createConnectQueryKey({
          schema: AdminService,
          cardinality: undefined,
        }),
      });
    },
  });

  return (
    <Dialog
      open={task !== null}
      onOpenChange={(open) => {
        if (!open) {
          form.reset();
          onClose();
        }
      }}
    >
      <DialogContent>
        <form
          noValidate
          onSubmit={form.handleSubmit((values) => {
            if (task) {
              remove.mutate({ id: task.id, reason: values.reason.trim() });
            }
          })}
          className="flex flex-col gap-4"
        >
          <DialogHeader>
            <DialogTitle>{t("deleteTitle")}</DialogTitle>
            <DialogDescription>
              {t("deleteDescription", { title: task?.title ?? "" })}
            </DialogDescription>
          </DialogHeader>
          <div className="flex flex-col gap-1">
            <label htmlFor={reasonId} className="font-medium text-sm">
              {t("reason")}
            </label>
            <Textarea
              id={reasonId}
              aria-invalid={Boolean(form.formState.errors.reason)}
              aria-describedby={`${reasonId}-error`}
              {...form.register("reason")}
            />
            <p id={`${reasonId}-error`} className="text-destructive text-xs">
              {form.formState.errors.reason ? t("reasonInvalid") : null}
            </p>
          </div>
          <DialogFooter>
            <Button type="button" variant="outline" onClick={onClose}>
              {t("cancel")}
            </Button>
            <Button
              type="submit"
              variant="destructive"
              disabled={remove.isPending}
            >
              {t("confirmDelete")}
            </Button>
          </DialogFooter>
        </form>
      </DialogContent>
    </Dialog>
  );
}
