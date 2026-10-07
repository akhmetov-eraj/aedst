"use client";

import { timestampDate } from "@bufbuild/protobuf/wkt";
import { useQuery } from "@connectrpc/connect-query";
import { useLocale, useTranslations } from "next-intl";
import { useQueryStates } from "nuqs";
import { useEffect, useId, useState } from "react";

import type { Locale } from "@/app/config/i18n/locales";
import { Link } from "@/app/config/i18n/navigation";
import {
  ALL_PRIORITIES,
  ALL_STATUSES,
  PRIORITY_KEYS,
  STATUS_KEYS,
} from "@/modules/tasks/lib/labels";
import {
  buildListTasksInput,
  SORT_OPTIONS,
} from "@/modules/tasks/lib/list-tasks-input";
import {
  PRIORITY_FILTERS,
  STATUS_FILTERS,
  taskSearchParsers,
} from "@/modules/tasks/lib/task-search-params";
import { PriorityBadge } from "@/modules/tasks/ui/PriorityBadge";
import { StatusBadge } from "@/modules/tasks/ui/StatusBadge";
import { TasksService } from "@/shared/api/connect/client";
import {
  type TaskPriority,
  TaskStatus,
} from "@/shared/api/connect/generated/aedst/tasks/v1/tasks_pb";
import { formatDate } from "@/shared/date/format-date";
import { useDebouncedValue } from "@/shared/lib/hooks/use-debounced-value";
import { Button, buttonVariants } from "@/shared/ui/button";
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

const SEARCH_DEBOUNCE_MS = 300;

const STATUS_FILTER_BY_ENUM = {
  [TaskStatus.TODO]: "todo",
  [TaskStatus.IN_PROGRESS]: "in-progress",
  [TaskStatus.DONE]: "done",
} as const;

const PRIORITY_FILTER_BY_ENUM = {
  1: "low",
  2: "medium",
  3: "high",
  4: "urgent",
} as const;

export function TaskList() {
  const t = useTranslations("tasks.list");
  const tStatus = useTranslations("tasks.status");
  const tPriority = useTranslations("tasks.priority");
  const locale = useLocale() as Locale;
  const searchId = useId();

  // All view state lives in the URL (see task-search-params.ts).
  const [params, setParams] = useQueryStates(taskSearchParsers);

  // The box types instantly; the URL (and therefore the request) only
  // follows after a pause, so a fast typist sends one request, not one per key.
  const [searchInput, setSearchInput] = useState(params.q);
  const debouncedSearch = useDebouncedValue(searchInput, SEARCH_DEBOUNCE_MS);

  // biome-ignore lint/correctness/useExhaustiveDependencies: must fire only when the *debounced input* changes; depending on params.q would re-apply a stale debounced value after back/forward navigation.
  useEffect(() => {
    if (debouncedSearch !== params.q) {
      void setParams({ q: debouncedSearch, cursors: [] });
    }
  }, [debouncedSearch]);

  // Back/forward (or a shared link) changes `q` from outside the box.
  useEffect(() => {
    setSearchInput(params.q);
  }, [params.q]);

  const { data, isLoading, isError, refetch } = useQuery(
    TasksService.method.listTasks,
    buildListTasksInput(params),
  );

  const hasFilters =
    params.q !== "" || params.status !== "all" || params.priority !== "all";
  const hasPreviousPage = params.cursors.length > 0;

  function statusLabel(value: TaskStatus) {
    const key = STATUS_KEYS[value];
    return key ? tStatus(key) : "";
  }

  function priorityLabel(value: TaskPriority) {
    const key = PRIORITY_KEYS[value];
    return key ? tPriority(key) : "";
  }

  return (
    <section aria-label={t("resultsLabel")} className="flex flex-col gap-4">
      {/* biome-ignore lint/a11y/useSemanticElements: role="search" on a div works in every browser and in jsdom; the <search> element does not (Safari < 17, jsdom) */}
      <div role="search" className="flex flex-wrap items-center gap-2">
        <label htmlFor={searchId} className="sr-only">
          {t("searchLabel")}
        </label>
        <Input
          id={searchId}
          type="search"
          value={searchInput}
          onChange={(event) => setSearchInput(event.target.value)}
          placeholder={t("searchPlaceholder")}
          className="max-w-xs"
        />

        <Select
          items={[
            { value: "all", label: t("allStatuses") },
            ...ALL_STATUSES.map((value) => ({
              value: STATUS_FILTER_BY_ENUM[value],
              label: statusLabel(value),
            })),
          ]}
          value={params.status}
          onValueChange={(value) => {
            if (
              STATUS_FILTERS.includes(value as (typeof STATUS_FILTERS)[number])
            ) {
              void setParams({
                status: value as (typeof STATUS_FILTERS)[number],
                cursors: [],
              });
            }
          }}
        >
          <SelectTrigger className="w-40" aria-label={t("statusFilterLabel")}>
            <SelectValue placeholder={t("allStatuses")} />
          </SelectTrigger>
          <SelectContent>
            <SelectItem value="all">{t("allStatuses")}</SelectItem>
            {ALL_STATUSES.map((value) => (
              <SelectItem key={value} value={STATUS_FILTER_BY_ENUM[value]}>
                {statusLabel(value)}
              </SelectItem>
            ))}
          </SelectContent>
        </Select>

        <Select
          items={[
            { value: "all", label: t("allPriorities") },
            ...ALL_PRIORITIES.map((value) => ({
              value: PRIORITY_FILTER_BY_ENUM[value],
              label: priorityLabel(value),
            })),
          ]}
          value={params.priority}
          onValueChange={(value) => {
            if (
              PRIORITY_FILTERS.includes(
                value as (typeof PRIORITY_FILTERS)[number],
              )
            ) {
              void setParams({
                priority: value as (typeof PRIORITY_FILTERS)[number],
                cursors: [],
              });
            }
          }}
        >
          <SelectTrigger className="w-40" aria-label={t("priorityFilterLabel")}>
            <SelectValue placeholder={t("allPriorities")} />
          </SelectTrigger>
          <SelectContent>
            <SelectItem value="all">{t("allPriorities")}</SelectItem>
            {ALL_PRIORITIES.map((value) => (
              <SelectItem key={value} value={PRIORITY_FILTER_BY_ENUM[value]}>
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
          value={params.sort}
          onValueChange={(value) => {
            const option = SORT_OPTIONS.find((o) => o.value === value);
            if (option) {
              void setParams({ sort: option.value, cursors: [] });
            }
          }}
        >
          <SelectTrigger className="w-48" aria-label={t("sortLabel")}>
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

        <Link
          href="/tasks/new"
          className={buttonVariants({ className: "ms-auto" })}
        >
          {t("newTask")}
        </Link>
      </div>

      {isLoading ? (
        <div className="flex flex-col gap-2" aria-busy="true">
          {[0, 1, 2].map((i) => (
            <Skeleton key={i} className="h-16 w-full" />
          ))}
        </div>
      ) : isError ? (
        <Empty role="alert">
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
              <Link href="/tasks/new" className={buttonVariants()}>
                {t("newTask")}
              </Link>
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

      {data && (data.tasks.length > 0 || hasPreviousPage) && (
        <nav
          aria-label={t("paginationLabel")}
          className="flex items-center justify-end gap-2"
        >
          <Button
            variant="outline"
            size="sm"
            disabled={!hasPreviousPage}
            onClick={() =>
              void setParams({ cursors: params.cursors.slice(0, -1) })
            }
          >
            {t("previousPage")}
          </Button>
          <Button
            variant="outline"
            size="sm"
            disabled={!data.nextPageToken}
            onClick={() =>
              void setParams(
                { cursors: [...params.cursors, data.nextPageToken] },
                { history: "push" },
              )
            }
          >
            {t("nextPage")}
          </Button>
        </nav>
      )}
    </section>
  );
}
