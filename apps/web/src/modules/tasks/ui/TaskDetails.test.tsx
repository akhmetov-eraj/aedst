import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, expect, it, vi } from "vitest";

import { TaskDetails } from "@/modules/tasks/ui/TaskDetails";
import {
  TaskPriority,
  TaskStatus,
  TasksService,
} from "@/shared/api/connect/generated/aedst/tasks/v1/tasks_pb";
import { makeTask } from "@/test/fixtures";
import { rpcErrorHandler, rpcHandler } from "@/test/mocks/connect";
import { routerMock } from "@/test/mocks/navigation";
import { server } from "@/test/mocks/server";
import { messages, renderWithProviders } from "@/test/render";

const t = messages.tasks.details;

describe("TaskDetails", () => {
  it("shows the task with its facts", async () => {
    const task = makeTask({
      title: "Ship it",
      description: "Before Friday",
      priority: TaskPriority.HIGH,
      status: TaskStatus.IN_PROGRESS,
    });
    server.use(rpcHandler(TasksService.method.getTask, () => ({ task })));

    renderWithProviders(<TaskDetails taskId={String(task.id)} />);

    expect(
      await screen.findByRole("heading", { name: "Ship it" }),
    ).toBeVisible();
    expect(screen.getByText("Before Friday")).toBeVisible();
    expect(screen.getByText(t.noDueDate)).toBeVisible();
    expect(screen.getByRole("link", { name: t.backToList })).toHaveAttribute(
      "href",
      "/tasks",
    );
  });

  it("says 'not found' for a missing (or someone else's) task", async () => {
    server.use(
      rpcErrorHandler(TasksService.method.getTask, "not_found", "no such task"),
    );

    renderWithProviders(<TaskDetails taskId="x" />);

    expect(await screen.findByText(t.notFoundTitle)).toBeVisible();
    expect(screen.queryByRole("alert")).not.toBeInTheDocument();
  });

  it("distinguishes a load failure from 'not found', and offers a retry", async () => {
    server.use(
      rpcErrorHandler(TasksService.method.getTask, "internal", "boom"),
    );
    renderWithProviders(<TaskDetails taskId="x" />);

    expect(await screen.findByRole("alert")).toHaveTextContent(
      t.loadErrorTitle,
    );
    expect(screen.queryByText(t.notFoundTitle)).not.toBeInTheDocument();

    const task = makeTask({ title: "Recovered" });
    server.use(rpcHandler(TasksService.method.getTask, () => ({ task })));
    await userEvent.click(
      screen.getByRole("button", { name: messages.tasks.list.retry }),
    );

    expect(
      await screen.findByRole("heading", { name: "Recovered" }),
    ).toBeVisible();
  });

  it("completes a task and reflects the refreshed status", async () => {
    const task = makeTask({ title: "Do it" });
    let status = TaskStatus.TODO;
    server.use(
      rpcHandler(TasksService.method.getTask, () => ({
        task: { ...task, status },
      })),
      rpcHandler(TasksService.method.completeTask, (input) => {
        expect(input.id).toBe(task.id);
        status = TaskStatus.DONE;
        return { task: { ...task, status } };
      }),
    );
    renderWithProviders(<TaskDetails taskId={String(task.id)} />);

    await userEvent.click(
      await screen.findByRole("button", { name: t.complete }),
    );

    expect(await screen.findByRole("button", { name: t.reopen })).toBeVisible();
  });

  it("shows a mutation failure inline (and not also as a toast)", async () => {
    const task = makeTask();
    server.use(
      rpcHandler(TasksService.method.getTask, () => ({ task })),
      rpcErrorHandler(
        TasksService.method.completeTask,
        "internal",
        "disk full",
      ),
    );
    const onUserFacingError = vi.fn();
    renderWithProviders(<TaskDetails taskId={String(task.id)} />, {
      callbacks: { onUserFacingError },
    });

    await userEvent.click(
      await screen.findByRole("button", { name: t.complete }),
    );

    expect(await screen.findByRole("alert")).toHaveTextContent("disk full");
    expect(onUserFacingError).not.toHaveBeenCalled();
  });

  it("asks for confirmation before deleting, then returns to the list", async () => {
    const task = makeTask();
    let deleted: string | undefined;
    server.use(
      rpcHandler(TasksService.method.getTask, () => ({ task })),
      rpcHandler(TasksService.method.deleteTask, (input) => {
        deleted = input.id;
        return {};
      }),
    );
    renderWithProviders(<TaskDetails taskId={String(task.id)} />);

    await userEvent.click(
      await screen.findByRole("button", { name: t.delete }),
    );
    expect(await screen.findByText(t.deleteConfirmTitle)).toBeVisible();
    expect(deleted).toBeUndefined();

    await userEvent.click(
      screen.getByRole("button", { name: t.deleteConfirmAction }),
    );

    await waitFor(() => expect(routerMock.push).toHaveBeenCalledWith("/tasks"));
    expect(deleted).toBe(task.id);
  });

  it("switches to the edit form", async () => {
    const task = makeTask({ title: "Edit me" });
    server.use(rpcHandler(TasksService.method.getTask, () => ({ task })));
    renderWithProviders(<TaskDetails taskId={String(task.id)} />);

    await userEvent.click(await screen.findByRole("button", { name: t.edit }));

    expect(screen.getByLabelText(messages.tasks.form.titleLabel)).toHaveValue(
      "Edit me",
    );
  });
});
