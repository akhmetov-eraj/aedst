import type { Metadata } from "next";
import { getTranslations } from "next-intl/server";

import { NotificationsInbox } from "@/modules/notifications/ui/NotificationsInbox";

export async function generateMetadata({
  params,
}: PageProps<"/[locale]/notifications">): Promise<Metadata> {
  const { locale } = await params;
  const t = await getTranslations({ locale, namespace: "notifications.inbox" });
  return { title: t("title") };
}

export default function NotificationsPage() {
  return <NotificationsInbox />;
}
