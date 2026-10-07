import type { Metadata } from "next";
import { getTranslations } from "next-intl/server";

import { AdminTasks } from "@/modules/admin/ui/AdminTasks";

export async function generateMetadata({
  params,
}: PageProps<"/[locale]/admin/tasks">): Promise<Metadata> {
  const { locale } = await params;
  const t = await getTranslations({ locale, namespace: "admin.tasks" });
  return { title: t("title") };
}

export default function AdminTasksPage() {
  return <AdminTasks />;
}
