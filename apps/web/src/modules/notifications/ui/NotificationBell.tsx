"use client";

import { useQuery } from "@connectrpc/connect-query";
import { Notification03Icon } from "@hugeicons/core-free-icons";
import { HugeiconsIcon } from "@hugeicons/react";
import { useTranslations } from "next-intl";
import { useEffect } from "react";

import { Link } from "@/app/config/i18n/navigation";
import { useMarkNotifications } from "@/modules/notifications/lib/use-notifications";
import { usePushSignOutCleanup } from "@/modules/notifications/lib/use-push-sign-out-cleanup";
import { NotificationsService } from "@/shared/api/connect/client";
import { track } from "@/shared/lib/analytics/analytics";
import { useOnlineStatus } from "@/shared/lib/hooks/use-online-status";

const POLL_MS = 60_000;

/**
 * Header bell with the unread count. Polls while the tab is visible and
 * online (TanStack Query pauses `refetchInterval` in background tabs), and
 * refreshes immediately when the service worker reports a push was clicked.
 */
export function NotificationBell() {
  const t = useTranslations("notifications.bell");
  const online = useOnlineStatus();
  const { invalidate } = useMarkNotifications();
  usePushSignOutCleanup();
  const { data } = useQuery(
    NotificationsService.method.getUnreadCount,
    {},
    {
      refetchInterval: online ? POLL_MS : false,
      refetchOnWindowFocus: true,
      meta: { feature: "notifications" },
    },
  );
  const unread = data?.unread ?? 0;

  useEffect(() => {
    const container = navigator.serviceWorker;
    if (!container) {
      return;
    }
    const onMessage = (event: MessageEvent) => {
      if (event.data?.type === "NOTIFICATION_CLICKED") {
        track("notification_opened", {
          kind: String(event.data.kind ?? "unknown"),
          via: "push",
        });
        void invalidate();
      }
    };
    container.addEventListener("message", onMessage);
    return () => container.removeEventListener("message", onMessage);
  }, [invalidate]);

  const label =
    unread > 0 ? t("labelWithCount", { count: unread }) : t("label");

  return (
    <Link
      href="/notifications"
      aria-label={label}
      title={label}
      className="relative inline-flex size-9 items-center justify-center rounded-md hover:bg-muted"
    >
      <HugeiconsIcon
        icon={Notification03Icon}
        strokeWidth={2}
        className="size-5"
      />
      {unread > 0 && (
        <span
          aria-hidden="true"
          className="absolute -end-0.5 -top-0.5 min-w-5 rounded-full bg-destructive px-1 text-center font-semibold text-[11px] text-white leading-5"
        >
          {unread > 99 ? "99+" : unread}
        </span>
      )}
    </Link>
  );
}
