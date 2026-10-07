import type { Metadata } from "next";
import { getTranslations } from "next-intl/server";

import { NotificationPreferences } from "@/modules/notifications/ui/NotificationPreferences";

export async function generateMetadata({
  params,
}: PageProps<"/[locale]/notifications/preferences">): Promise<Metadata> {
  const { locale } = await params;
  const t = await getTranslations({
    locale,
    namespace: "notifications.preferences",
  });
  return { title: t("metaTitle") };
}

export default function NotificationPreferencesPage() {
  return <NotificationPreferences />;
}
