import { useTranslations } from "next-intl";

import {
  PRIORITY_BADGE_CLASS,
  PRIORITY_KEYS,
} from "@/modules/tasks/lib/labels";
import type { TaskPriority } from "@/shared/api/connect/generated/aedst/tasks/v1/tasks_pb";
import { Badge } from "@/shared/ui/badge";

export function PriorityBadge({ priority }: { priority: TaskPriority }) {
  const t = useTranslations("tasks.priority");
  const key = PRIORITY_KEYS[priority];
  if (!key) {
    return null;
  }
  return <Badge className={PRIORITY_BADGE_CLASS[priority]}>{t(key)}</Badge>;
}
