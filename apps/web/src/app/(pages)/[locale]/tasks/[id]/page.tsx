import { TaskDetails } from "@/modules/tasks/ui/task-details";

export default async function TaskDetailsPage({
  params,
}: PageProps<"/[locale]/tasks/[id]">) {
  const { id } = await params;
  return <TaskDetails taskId={id} />;
}
