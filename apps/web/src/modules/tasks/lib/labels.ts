import {
  TaskPriority,
  TaskStatus,
} from "@/shared/api/connect/generated/aedst/tasks/v1/tasks_pb";

export const STATUS_KEYS: Record<
  TaskStatus,
  "todo" | "inProgress" | "done" | null
> = {
  [TaskStatus.UNSPECIFIED]: null,
  [TaskStatus.TODO]: "todo",
  [TaskStatus.IN_PROGRESS]: "inProgress",
  [TaskStatus.DONE]: "done",
};

export const PRIORITY_KEYS: Record<
  TaskPriority,
  "low" | "medium" | "high" | "urgent" | null
> = {
  [TaskPriority.UNSPECIFIED]: null,
  [TaskPriority.LOW]: "low",
  [TaskPriority.MEDIUM]: "medium",
  [TaskPriority.HIGH]: "high",
  [TaskPriority.URGENT]: "urgent",
};

export const ALL_STATUSES = [
  TaskStatus.TODO,
  TaskStatus.IN_PROGRESS,
  TaskStatus.DONE,
] as const;
export const ALL_PRIORITIES = [
  TaskPriority.LOW,
  TaskPriority.MEDIUM,
  TaskPriority.HIGH,
  TaskPriority.URGENT,
] as const;

/** Tailwind classes per status/priority — a small, fixed palette, not a design-system abstraction this app doesn't otherwise need. */
export const STATUS_BADGE_CLASS: Record<TaskStatus, string> = {
  [TaskStatus.UNSPECIFIED]: "",
  [TaskStatus.TODO]: "bg-muted text-muted-foreground",
  [TaskStatus.IN_PROGRESS]:
    "bg-blue-100 text-blue-800 dark:bg-blue-950 dark:text-blue-300",
  [TaskStatus.DONE]:
    "bg-green-100 text-green-800 dark:bg-green-950 dark:text-green-300",
};

export const PRIORITY_BADGE_CLASS: Record<TaskPriority, string> = {
  [TaskPriority.UNSPECIFIED]: "",
  [TaskPriority.LOW]: "bg-muted text-muted-foreground",
  [TaskPriority.MEDIUM]:
    "bg-yellow-100 text-yellow-800 dark:bg-yellow-950 dark:text-yellow-300",
  [TaskPriority.HIGH]:
    "bg-orange-100 text-orange-800 dark:bg-orange-950 dark:text-orange-300",
  [TaskPriority.URGENT]:
    "bg-red-100 text-red-800 dark:bg-red-950 dark:text-red-300",
};
