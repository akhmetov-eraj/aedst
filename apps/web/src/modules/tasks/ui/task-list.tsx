"use client";

import { timestampDate } from "@bufbuild/protobuf/wkt";
import { useQuery } from "@connectrpc/connect-query";
import { useLocale, useTranslations } from "next-intl";
import { useState } from "react";
import type { Locale } from "@/app/config/i18n/locales";
import { Link } from "@/app/config/i18n/navigation";
import {
  ALL_PRIORITIES,
  ALL_STATUSES,
  PRIORITY_KEYS,
  STATUS_KEYS,
} from "@/modules/tasks/lib/labels";
import { PriorityBadge } from "@/modules/tasks/ui/priority-badge";
import { StatusBadge } from "@/modules/tasks/ui/status-badge";
import { TasksService } from "@/shared/api/connect/client";
import {
  SortDirection,
  type TaskPriority,
  TaskSortField,
  type TaskStatus,
} from "@/shared/api/connect/generated/aedst/tasks/v1/tasks_pb";
import { formatDate } from "@/shared/date/format-date";
import { Button } from "@/shared/ui/button";
import {
  Empty,
  EmptyContent,
  EmptyDescription,
  EmptyHeader,
  EmptyTitle,
} from "@/shared/ui/empty";
import { Input } from "@/shared/ui/input";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/shared/ui/select";
import { Skeleton } from "@/shared/ui/skeleton";

const SORT_OPTIONS = [
  {
    value: "created-desc",
    sortBy: TaskSortField.CREATED_AT,
    sortDirection: SortDirection.DESC,
    labelKey: "sortCreatedDesc",
  },
  {
    value: "created-asc",
    sortBy: TaskSortField.CREATED_AT,
    sortDirection: SortDirection.ASC,
    labelKey: "sortCreatedAsc",
  },
  {
    value: "due-asc",
    sortBy: TaskSortField.DUE_DATE,
    sortDirection: SortDirection.ASC,
    labelKey: "sortDueAsc",
  },
  {
    value: "due-desc",
    sortBy: TaskSortField.DUE_DATE,
    sortDirection: SortDirection.DESC,
    labelKey: "sortDueDesc",
  },
] as const;

export function TaskList() {
  const t = useTranslations("tasks.list");
  const tStatus = useTranslations("tasks.status");
  const tPriority = useTranslations("tasks.priority");
  const locale = useLocale() as Locale;

  const [search, setSearch] = useState("");
  const [status, setStatus] = useState<TaskStatus | "all">("all");
  const [priority, setPriority] = useState<TaskPriority | "all">("all");
  const [sortValue, setSortValue] =
    useState<(typeof SORT_OPTIONS)[number]["value"]>("created-desc");
  // A stack of page tokens visited so far — ListTasksResponse only gives a
  // *next* token (see tasks.proto), so "previous" replays this stack rather
  // than the server needing to support backward pagination too.
  const [pageStack, setPageStack] = useState<string[]>([""]);

  const sort =
    SORT_OPTIONS.find((option) => option.value === sortValue) ??
    SORT_OPTIONS[0];
  const pageToken = pageStack[pageStack.length - 1] ?? "";

  function statusLabel(value: TaskStatus) {
    const key = STATUS_KEYS[value];
    return key ? tStatus(key) : "";
  }

  function priorityLabel(value: TaskPriority) {
    const key = PRIORITY_KEYS[value];
    return key ? tPriority(key) : "";
  }

  const { data, isLoading, isError, refetch } = useQuery(
    TasksService.method.listTasks,
    {
      statuses: status === "all" ? [] : [status],
      priorities: priority === "all" ? [] : [priority],
      search,
      sortBy: sort.sortBy,
      sortDirection: sort.sortDirection,
      pageSize: 20,
      pageToken,
    },
  );

  function resetPaging() {
    setPageStack([""]);
  }

  const hasFilters = search !== "" || status !== "all" || priority !== "all";

  return (
    <div className="flex flex-col gap-4">
      <div className="flex flex-wrap items-center gap-2">
        <Input
          value={search}
          onChange={(event) => {
            setSearch(event.target.value);
            resetPaging();
          }}
          placeholder={t("searchPlaceholder")}
          className="max-w-xs"
        />

        <Select
          items={[
            { value: "all", label: t("allStatuses") },
            ...ALL_STATUSES.map((value) => ({
              value: String(value),
              label: statusLabel(value),
            })),
          ]}
          value={status === "all" ? "all" : String(status)}
          onValueChange={(value) => {
            setStatus(value === "all" ? "all" : (Number(value) as TaskStatus));
            resetPaging();
          }}
        >
          <SelectTrigger className="w-40">
            <SelectValue placeholder={t("allStatuses")} />
          </SelectTrigger>
          <SelectContent>
            <SelectItem value="all">{t("allStatuses")}</SelectItem>
            {ALL_STATUSES.map((value) => (
              <SelectItem key={value} value={String(value)}>
                {statusLabel(value)}
              </SelectItem>
            ))}
          </SelectContent>
        </Select>

        <Select
          items={[
            { value: "all", label: t("allPriorities") },
            ...ALL_PRIORITIES.map((value) => ({
              value: String(value),
              label: priorityLabel(value),
            })),
          ]}
          value={priority === "all" ? "all" : String(priority)}
          onValueChange={(value) => {
            setPriority(
              value === "all" ? "all" : (Number(value) as TaskPriority),
            );
            resetPaging();
          }}
        >
          <SelectTrigger className="w-40">
            <SelectValue placeholder={t("allPriorities")} />
          </SelectTrigger>
          <SelectContent>
            <SelectItem value="all">{t("allPriorities")}</SelectItem>
            {ALL_PRIORITIES.map((value) => (
              <SelectItem key={value} value={String(value)}>
                {priorityLabel(value)}
              </SelectItem>
            ))}
          </SelectContent>
        </Select>

        <Select
          items={SORT_OPTIONS.map((option) => ({
            value: option.value,
            label: t(option.labelKey),
          }))}
          value={sortValue}
          onValueChange={(value) => {
            setSortValue(value as typeof sortValue);
            resetPaging();
          }}
        >
          <SelectTrigger className="w-48">
            <SelectValue />
          </SelectTrigger>
          <SelectContent>
            {SORT_OPTIONS.map((option) => (
              <SelectItem key={option.value} value={option.value}>
                {t(option.labelKey)}
              </SelectItem>
            ))}
          </SelectContent>
        </Select>

        <Button
          render={<Link href="/tasks/new" />}
          nativeButton={false}
          className="ms-auto"
        >
          {t("newTask")}
        </Button>
      </div>

      {isLoading ? (
        <div className="flex flex-col gap-2">
          {[0, 1, 2].map((i) => (
            <Skeleton key={i} className="h-16 w-full" />
          ))}
        </div>
      ) : isError ? (
        <Empty>
          <EmptyHeader>
            <EmptyTitle>{t("errorTitle")}</EmptyTitle>
          </EmptyHeader>
          <EmptyContent>
            <Button variant="outline" onClick={() => refetch()}>
              {t("retry")}
            </Button>
          </EmptyContent>
        </Empty>
      ) : data && data.tasks.length === 0 ? (
        <Empty>
          <EmptyHeader>
            <EmptyTitle>
              {hasFilters ? t("noResultsTitle") : t("emptyTitle")}
            </EmptyTitle>
            <EmptyDescription>
              {hasFilters ? t("noResultsDescription") : t("emptyDescription")}
            </EmptyDescription>
          </EmptyHeader>
          {!hasFilters && (
            <EmptyContent>
              <Button render={<Link href="/tasks/new" />} nativeButton={false}>
                {t("newTask")}
              </Button>
            </EmptyContent>
          )}
        </Empty>
      ) : (
        <ul className="flex flex-col gap-2">
          {data?.tasks.map((task) => (
            <li key={task.id}>
              <Link
                href={`/tasks/${task.id}`}
                className="flex items-center justify-between gap-3 rounded-lg border p-3 hover:bg-accent"
              >
                <div className="flex min-w-0 flex-col gap-1">
                  <span className="truncate font-medium">{task.title}</span>
                  {task.dueDate && (
                    <span className="text-muted-foreground text-xs">
                      {t("due", {
                        date: formatDate(timestampDate(task.dueDate), locale),
                      })}
                    </span>
                  )}
                </div>
                <div className="flex shrink-0 items-center gap-2">
                  <PriorityBadge priority={task.priority} />
                  <StatusBadge status={task.status} />
                </div>
              </Link>
            </li>
          ))}
        </ul>
      )}

      {data && (data.tasks.length > 0 || pageStack.length > 1) && (
        <div className="flex items-center justify-end gap-2">
          <Button
            variant="outline"
            size="sm"
            disabled={pageStack.length <= 1}
            onClick={() => setPageStack((stack) => stack.slice(0, -1))}
          >
            {t("previousPage")}
          </Button>
          <Button
            variant="outline"
            size="sm"
            disabled={!data.nextPageToken}
            onClick={() =>
              setPageStack((stack) => [...stack, data.nextPageToken])
            }
          >
            {t("nextPage")}
          </Button>
        </div>
      )}
    </div>
  );
}
