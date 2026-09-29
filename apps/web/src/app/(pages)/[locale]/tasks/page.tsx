import type { Metadata } from "next";
import { getTranslations } from "next-intl/server";

import { buildLanguageAlternates } from "@/app/config/seo";
import { DashboardStats } from "@/modules/tasks/ui/dashboard-stats";
import { TaskList } from "@/modules/tasks/ui/task-list";

export async function generateMetadata({
  params,
}: PageProps<"/[locale]/tasks">): Promise<Metadata> {
  const { locale } = await params;
  const t = await getTranslations({ locale, namespace: "tasks.dashboard" });

  return {
    // Composes with the root locale layout's `title.template` — unlike the
    // home page, this is a genuinely nested segment (`[locale]/tasks`, not
    // `[locale]` itself), so the "same segment" exception doesn't apply
    // here (see (pages)/[locale]/page.tsx's generateMetadata comment).
    title: t("title"),
    alternates: { languages: buildLanguageAlternates("tasks") },
  };
}

export default async function TasksPage({
  params,
}: PageProps<"/[locale]/tasks">) {
  const { locale } = await params;
  const t = await getTranslations({ locale, namespace: "tasks.dashboard" });

  return (
    <div className="flex flex-col gap-6">
      <h1 className="font-semibold text-2xl">{t("title")}</h1>
      <DashboardStats />
      <TaskList />
    </div>
  );
}
