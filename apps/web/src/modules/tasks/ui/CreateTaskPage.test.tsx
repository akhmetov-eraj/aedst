import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, expect, it, vi } from "vitest";

import { CreateTaskPage } from "@/modules/tasks/ui/CreateTaskPage";
import { TasksService } from "@/shared/api/connect/generated/aedst/tasks/v1/tasks_pb";
import { makeTask } from "@/test/fixtures";
import { rpcErrorHandler, rpcHandler } from "@/test/mocks/connect";
import { routerMock } from "@/test/mocks/navigation";
import { server } from "@/test/mocks/server";
import { messages, renderWithProviders } from "@/test/render";

const t = messages.tasks.form;

describe("CreateTaskPage", () => {
  it("creates the task and opens it", async () => {
    const created = makeTask({ title: "Ship v1" });
    let received: { title?: string; description?: string } = {};
    server.use(
      rpcHandler(TasksService.method.createTask, (input) => {
        received = input;
        return { task: created };
      }),
    );
    renderWithProviders(<CreateTaskPage />);

    await userEvent.type(screen.getByLabelText(t.titleLabel), "Ship v1");
    await userEvent.type(
      screen.getByLabelText(t.descriptionLabel),
      "Before Friday",
    );
    await userEvent.click(screen.getByRole("button", { name: t.create }));

    await waitFor(() =>
      expect(routerMock.push).toHaveBeenCalledWith(`/tasks/${created.id}`),
    );
    expect(received).toMatchObject({
      title: "Ship v1",
      description: "Before Friday",
    });
  });

  it("blocks an empty title with an associated error", async () => {
    renderWithProviders(<CreateTaskPage />);

    await userEvent.click(screen.getByRole("button", { name: t.create }));

    expect(await screen.findByText(t.titleRequired)).toBeVisible();
    expect(screen.getByLabelText(t.titleLabel)).toHaveAccessibleDescription(
      t.titleRequired,
    );
    expect(routerMock.push).not.toHaveBeenCalled();
  });

  it("shows a server rejection inline instead of also toasting it", async () => {
    server.use(
      rpcErrorHandler(
        TasksService.method.createTask,
        "invalid_argument",
        "title too long",
      ),
    );
    const onUserFacingError = vi.fn();
    renderWithProviders(<CreateTaskPage />, {
      callbacks: { onUserFacingError },
    });

    await userEvent.type(screen.getByLabelText(t.titleLabel), "x");
    await userEvent.click(screen.getByRole("button", { name: t.create }));

    expect(await screen.findByRole("alert")).toHaveTextContent(
      "title too long",
    );
    expect(onUserFacingError).not.toHaveBeenCalled();
  });

  it("links back to the list", () => {
    renderWithProviders(<CreateTaskPage />);

    expect(
      screen.getByRole("link", { name: messages.tasks.details.backToList }),
    ).toHaveAttribute("href", "/tasks");
  });
});
