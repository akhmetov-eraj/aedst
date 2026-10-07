import type { Metadata } from "next";
import { getTranslations } from "next-intl/server";

import { SearchPage as SearchView } from "@/modules/search/ui/SearchPage";

export async function generateMetadata({
  params,
}: PageProps<"/[locale]/search">): Promise<Metadata> {
  const { locale } = await params;
  const t = await getTranslations({ locale, namespace: "search" });
  return { title: t("metaTitle") };
}

export default function SearchPage() {
  return <SearchView />;
}
