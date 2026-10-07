import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { NextIntlClientProvider } from "next-intl";
import { describe, expect, it, vi } from "vitest";

import tasksMessages from "@/modules/tasks/i18n/en.json";
import { TaskPriority } from "@/shared/api/connect/generated/aedst/tasks/v1/tasks_pb";
import { TaskForm } from "./TaskForm";

function renderForm(onSubmit = vi.fn()) {
  render(
    <NextIntlClientProvider locale="en" messages={{ tasks: tasksMessages }}>
      <TaskForm mode="create" onSubmit={onSubmit} isSubmitting={false} />
    </NextIntlClientProvider>,
  );
  return onSubmit;
}

describe("TaskForm", () => {
  it("shows a validation error and does not submit when the title is empty", async () => {
    const onSubmit = renderForm();
    const user = userEvent.setup();

    await user.click(
      screen.getByRole("button", { name: tasksMessages.form.create }),
    );

    expect(
      await screen.findByText(tasksMessages.form.titleRequired),
    ).toBeInTheDocument();
    expect(onSubmit).not.toHaveBeenCalled();
  });

  it("submits the trimmed title, description, priority and due date", async () => {
    const onSubmit = renderForm();
    const user = userEvent.setup();

    await user.type(
      screen.getByLabelText(tasksMessages.form.titleLabel),
      "  Ship the feature  ",
    );
    await user.type(
      screen.getByLabelText(tasksMessages.form.descriptionLabel),
      "Ready for review",
    );
    await user.click(
      screen.getByRole("button", { name: tasksMessages.form.create }),
    );

    expect(onSubmit).toHaveBeenCalledWith(
      expect.objectContaining({
        title: "Ship the feature",
        description: "Ready for review",
        priority: TaskPriority.MEDIUM,
      }),
      expect.anything(),
    );
  });

  it("disables the submit button while submitting", () => {
    render(
      <NextIntlClientProvider locale="en" messages={{ tasks: tasksMessages }}>
        <TaskForm mode="edit" onSubmit={vi.fn()} isSubmitting />
      </NextIntlClientProvider>,
    );

    expect(
      screen.getByRole("button", { name: tasksMessages.form.save }),
    ).toBeDisabled();
  });
});
