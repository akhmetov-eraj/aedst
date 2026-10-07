import { getTranslations } from "next-intl/server";

import { redirect } from "@/app/config/i18n/navigation";
import { AdminNav } from "@/modules/admin/ui/AdminNav";
import { ADMIN_ROLE } from "@/shared/lib/auth/roles";
import { getSessionRoles } from "@/shared/lib/auth/session";

/**
 * Admin area. `proxy.ts` already redirects non-admins; this re-check keeps
 * the area closed even if the proxy matcher changes. Neither is the real
 * guard — apps/server's RoleInterceptor rejects every AdminService call
 * without the admin role — but non-admins should not see an empty shell.
 */
export default async function AdminLayout({
  children,
  params,
}: LayoutProps<"/[locale]/admin">) {
  const { locale } = await params;
  if (!(await getSessionRoles()).includes(ADMIN_ROLE)) {
    redirect({ href: "/forbidden", locale });
  }
  const t = await getTranslations({ locale, namespace: "admin" });
  return (
    <div className="flex flex-col gap-4">
      <h1 className="font-semibold text-2xl">{t("title")}</h1>
      <AdminNav />
      {children}
    </div>
  );
}
