import { timestampFromDate } from "@bufbuild/protobuf/wkt";
import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, expect, it } from "vitest";

import { NotificationsInbox } from "@/modules/notifications/ui/NotificationsInbox";
import {
  NotificationKind,
  NotificationsService,
} from "@/shared/api/connect/generated/aedst/notifications/v1/notifications_pb";
import { rpcHandler } from "@/test/mocks/connect";
import { routerMock } from "@/test/mocks/navigation";
import { server } from "@/test/mocks/server";
import { messages, renderWithProviders } from "@/test/render";

const t = messages.notifications.inbox;
const ID = "00000000-0000-4000-8000-000000000001";

describe("NotificationsInbox", () => {
  it("lists notifications, marks one read when opened and follows its in-app link", async () => {
    const marked: string[][] = [];
    let unreadOnly: boolean | undefined;
    server.use(
      rpcHandler(NotificationsService.method.listNotifications, (input) => {
        unreadOnly = input.unreadOnly;
        return {
          notifications: [
            {
              id: ID,
              kind: NotificationKind.TASK_COMPLETED,
              title: "Task completed",
              body: "Write docs",
              link: "/tasks/1",
              createdAt: timestampFromDate(new Date()),
            },
          ],
        };
      }),
      rpcHandler(NotificationsService.method.markRead, (input) => {
        marked.push(input.ids);
        return {};
      }),
    );

    renderWithProviders(<NotificationsInbox />, {
      searchParams: "?unread=true",
    });

    await userEvent.click(
      await screen.findByRole(
        "button",
        { name: /Task completed/ },
        { timeout: 5_000 },
      ),
    );
    await waitFor(() => expect(marked).toEqual([[ID]]));
    expect(routerMock.push).toHaveBeenCalledWith("/tasks/1");
    expect(unreadOnly).toBe(true);
    // The unread dot is announced to screen readers (besides the tab label).
    expect(screen.getAllByText(t.unreadMarker)).toHaveLength(2);
  });

  it("marks everything read", async () => {
    let markedAll = false;
    server.use(
      rpcHandler(NotificationsService.method.listNotifications, () => ({
        notifications: [
          { id: ID, title: "Hello", link: "https://evil.example" },
        ],
      })),
      rpcHandler(NotificationsService.method.markAllRead, () => {
        markedAll = true;
        return { marked: 1 };
      }),
    );
    renderWithProviders(<NotificationsInbox />);

    await userEvent.click(
      await screen.findByRole("button", { name: t.markAllRead }),
    );
    await waitFor(() => expect(markedAll).toBe(true));

    // Off-site links are never followed.
    await userEvent.click(screen.getByRole("button", { name: /Hello/ }));
    expect(routerMock.push).not.toHaveBeenCalled();
  });

  it("shows the empty state", async () => {
    server.use(
      rpcHandler(NotificationsService.method.listNotifications, () => ({
        notifications: [],
      })),
    );
    renderWithProviders(<NotificationsInbox />);
    expect(await screen.findByText(t.emptyTitle)).toBeInTheDocument();
  });
});
