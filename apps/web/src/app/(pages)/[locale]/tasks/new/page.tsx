import type { Metadata } from "next";
import { getTranslations } from "next-intl/server";

import { CreateTaskPage } from "@/modules/tasks/ui/create-task-page";

export async function generateMetadata({
  params,
}: PageProps<"/[locale]/tasks/new">): Promise<Metadata> {
  const { locale } = await params;
  const t = await getTranslations({ locale, namespace: "tasks.form" });
  return { title: t("createTitle") };
}

export default function NewTaskPage() {
  return <CreateTaskPage />;
}
