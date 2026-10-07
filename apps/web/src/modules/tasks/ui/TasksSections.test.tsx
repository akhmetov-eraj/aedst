import { render, screen } from "@testing-library/react";
import { NextIntlClientProvider } from "next-intl";
import { NuqsTestingAdapter } from "nuqs/adapters/testing";
import { beforeEach, describe, expect, it, vi } from "vitest";

import { loadTaskSearchParams } from "@/modules/tasks/lib/task-search-params";
import {
  DashboardStatsFallback,
  DashboardStatsSection,
  TaskDetailsFallback,
  TaskDetailsSection,
  TaskListFallback,
  TaskListSection,
} from "@/modules/tasks/ui/TasksSections";
import { TasksService } from "@/shared/api/connect/generated/aedst/tasks/v1/tasks_pb";
import { makeTask } from "@/test/fixtures";
import { fakeJwt } from "@/test/jwt";
import { rpcHandler, TEST_API_ORIGIN } from "@/test/mocks/connect";
import { server } from "@/test/mocks/server";
import { messages, renderWithProviders } from "@/test/render";

const jar = new Map<string, string>();

vi.mock("next/headers", () => ({
  cookies: async () => ({
    get: (name: string) =>
      jar.has(name) ? { name, value: jar.get(name) } : undefined,
  }),
}));

beforeEach(() => {
  jar.clear();
  jar.set("aedst_session", fakeJwt({ expiresInSeconds: 600 }));
});

/**
 * Server Components can't render inside React Testing Library, but their
 * output can: `await` the component (this runs the real server prefetch
 * against the mocked API), then render the returned tree in the browser
 * providers — exactly what the RSC payload does on the client.
 */
describe("server sections", () => {
  it("DashboardStatsSection ships prefetched stats into the client component", async () => {
    server.use(
      rpcHandler(
        TasksService.method.getTaskStats,
        () => ({ total: 4, todo: 4 }),
        TEST_API_ORIGIN,
      ),
    );

    const tree = await DashboardStatsSection();
    renderWithProviders(tree, { staleTime: Number.POSITIVE_INFINITY });

    // Present synchronously, and no client-origin handler exists in this test:
    // a refetch on mount would fail it (`onUnhandledRequest: "error"`).
    expect(screen.getByText(messages.tasks.dashboard.totalTasks)).toBeVisible();
    expect(screen.getAllByText("4").length).toBeGreaterThan(0);
  });

  it("TaskListSection prefetches the page the URL asks for", async () => {
    let seen: { pageToken?: string; search?: string } = {};
    server.use(
      rpcHandler(
        TasksService.method.listTasks,
        (input) => {
          seen = input;
          return { tasks: [makeTask({ title: "From the server" })] };
        },
        TEST_API_ORIGIN,
      ),
    );

    const tree = await TaskListSection({
      params: loadTaskSearchParams({ q: "srv", cursors: "tok" }),
    });
    renderWithProviders(tree, {
      searchParams: "?q=srv&cursors=tok",
      staleTime: Number.POSITIVE_INFINITY,
    });

    expect(seen).toMatchObject({ search: "srv", pageToken: "tok" });
    expect(screen.getByText("From the server")).toBeVisible();
  });

  it("TaskDetailsSection prefetches one task", async () => {
    const task = makeTask({ title: "Detail from server" });
    server.use(
      rpcHandler(
        TasksService.method.getTask,
        () => ({ task }),
        TEST_API_ORIGIN,
      ),
    );

    const tree = await TaskDetailsSection({ taskId: String(task.id) });
    renderWithProviders(tree, { staleTime: Number.POSITIVE_INFINITY });

    expect(
      screen.getByRole("heading", { name: "Detail from server" }),
    ).toBeVisible();
  });

  it("still renders when the server prefetch fails — the client takes over", async () => {
    // No handler: the prefetch's request fails (unhandled → network error),
    // yet the section must not throw and the client fetches on its own.
    server.use(
      rpcHandler(
        TasksService.method.getTaskStats,
        () => {
          throw new Error("boom");
        },
        TEST_API_ORIGIN,
      ),
    );

    await expect(DashboardStatsSection()).resolves.toBeTruthy();
  });
});

describe("fallbacks", () => {
  it("are labelled busy placeholders", () => {
    const { container } = render(
      <NextIntlClientProvider locale="en" messages={messages}>
        <NuqsTestingAdapter>
          <DashboardStatsFallback />
          <TaskListFallback />
          <TaskDetailsFallback />
        </NuqsTestingAdapter>
      </NextIntlClientProvider>,
    );

    expect(container.querySelectorAll('[aria-busy="true"]').length).toBe(3);
    expect(screen.getAllByRole("status").length).toBe(3);
  });
});
