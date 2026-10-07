import type { Metadata } from "next";
import { getTranslations } from "next-intl/server";

import { BroadcastForm } from "@/modules/admin/ui/BroadcastForm";

export async function generateMetadata({
  params,
}: PageProps<"/[locale]/admin/broadcast">): Promise<Metadata> {
  const { locale } = await params;
  const t = await getTranslations({ locale, namespace: "admin.broadcast" });
  return { title: t("title") };
}

export default function AdminBroadcastPage() {
  return <BroadcastForm />;
}
