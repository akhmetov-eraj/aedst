import type { Metadata } from "next";
import { getTranslations } from "next-intl/server";

import { AdminOverview } from "@/modules/admin/ui/AdminOverview";

export async function generateMetadata({
  params,
}: PageProps<"/[locale]/admin">): Promise<Metadata> {
  const { locale } = await params;
  const t = await getTranslations({ locale, namespace: "admin.overview" });
  return { title: t("title") };
}

export default function AdminOverviewPage() {
  return <AdminOverview />;
}
