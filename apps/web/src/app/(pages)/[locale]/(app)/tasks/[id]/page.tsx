import type { Metadata } from "next";
import { Suspense } from "react";

import { TaskAttachments } from "@/modules/media/ui/TaskAttachments";
import {
  TaskDetailsFallback,
  TaskDetailsSection,
} from "@/modules/tasks/ui/TasksSections";
import { ErrorBoundary } from "@/shared/lib/errors/error-boundary";

export const metadata: Metadata = { robots: { index: false, follow: false } };

/**
 * Task detail + its attachments. The two come from different feature
 * modules (tasks, media) and are composed here: modules never import each
 * other, and a failure in the attachments widget is contained by its own
 * error boundary instead of replacing the task.
 */
export default async function TaskDetailsPage({
  params,
}: PageProps<"/[locale]/tasks/[id]">) {
  const { id } = await params;
  return (
    <div className="flex flex-col gap-8">
      <Suspense fallback={<TaskDetailsFallback />}>
        <TaskDetailsSection taskId={id} />
      </Suspense>
      <ErrorBoundary feature="media">
        <TaskAttachments taskId={id} />
      </ErrorBoundary>
    </div>
  );
}
