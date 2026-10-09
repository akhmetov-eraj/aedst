import { ADMIN_ROLE } from "@/shared/lib/auth/roles";
import { defineModule } from "@/shared/modules/manifest";

export const manifest = defineModule({
  id: "admin",
  i18nNamespaces: ["admin"],
  routes: [{ path: "/admin", access: "session", requiredRole: ADMIN_ROLE }],
  nav: [
    { href: "/admin", labelKey: "admin", order: 90, requiredRole: ADMIN_ROLE },
  ],
  // Hidden until the server enables it (same set on every client: docs/content/decisions/cross-cutting.md).
  flagged: true,
  featureFlags: [],
  capabilities: [],
  permissions: [ADMIN_ROLE],
});
