"use client";

import { cn } from "cn";
import { useTranslations } from "next-intl";
import { Link, usePathname } from "@/app/config/i18n/navigation";
import { useAuth } from "@/shared/lib/auth/auth-provider";
import { ADMIN_ROLE } from "@/shared/lib/auth/roles";

const ITEMS = [
  { href: "/tasks", key: "tasks" },
  { href: "/search", key: "search" },
  { href: "/notifications", key: "notifications" },
] as const;

/**
 * Primary navigation. The admin entry renders only for tokens carrying the
 * admin role — a UI hint; the admin pages and every admin RPC are guarded
 * again by proxy.ts and apps/server.
 */
export function AppNav() {
  const t = useTranslations("common.nav");
  const pathname = usePathname();
  const { hasRole } = useAuth();
  const items = hasRole(ADMIN_ROLE)
    ? [...ITEMS, { href: "/admin", key: "admin" } as const]
    : ITEMS;

  return (
    <nav aria-label={t("label")}>
      <ul className="flex flex-wrap items-center gap-1">
        {items.map((item) => {
          const active =
            pathname === item.href || pathname.startsWith(`${item.href}/`);
          return (
            <li key={item.href}>
              <Link
                href={item.href}
                aria-current={active ? "page" : undefined}
                className={cn(
                  "rounded-md px-3 py-1.5 font-medium text-sm transition-colors hover:bg-muted",
                  active ? "bg-muted text-foreground" : "text-muted-foreground",
                )}
              >
                {t(item.key)}
              </Link>
            </li>
          );
        })}
      </ul>
    </nav>
  );
}
