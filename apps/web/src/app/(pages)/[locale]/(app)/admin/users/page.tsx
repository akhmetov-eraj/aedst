import type { Metadata } from "next";
import { getTranslations } from "next-intl/server";

import { AdminUsers } from "@/modules/admin/ui/AdminUsers";

export async function generateMetadata({
  params,
}: PageProps<"/[locale]/admin/users">): Promise<Metadata> {
  const { locale } = await params;
  const t = await getTranslations({ locale, namespace: "admin.users" });
  return { title: t("title") };
}

export default function AdminUsersPage() {
  return <AdminUsers />;
}
