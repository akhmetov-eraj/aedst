import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, expect, it, vi } from "vitest";

import { loginAction } from "@/modules/auth/lib/auth-actions";
import { LoginForm } from "@/modules/auth/ui/LoginForm";
import { routerMock } from "@/test/mocks/navigation";
import { messages, renderWithProviders } from "@/test/render";

vi.mock("@/modules/auth/lib/auth-actions", () => ({
  loginAction: vi.fn(),
  registerAction: vi.fn(),
  logoutAction: vi.fn(),
}));

const t = messages.auth.login;

async function fillAndSubmit(email = "ada@example.com", password = "secret") {
  const user = userEvent.setup();
  await user.type(screen.getByLabelText(t.emailLabel), email);
  await user.type(screen.getByLabelText(t.passwordLabel), password);
  await user.click(screen.getByRole("button", { name: t.submit }));
}

describe("LoginForm", () => {
  it("validates before calling the server, and wires errors to their fields", async () => {
    const user = userEvent.setup();
    renderWithProviders(<LoginForm onSwitchToRegister={vi.fn()} />);

    await user.type(screen.getByLabelText(t.emailLabel), "nope");
    await user.click(screen.getByRole("button", { name: t.submit }));

    const email = screen.getByLabelText(t.emailLabel);
    expect(await screen.findByText(t.invalidEmail)).toBeInTheDocument();
    expect(screen.getByText(t.invalidPassword)).toBeInTheDocument();
    expect(email).toHaveAttribute("aria-invalid", "true");
    expect(email).toHaveAccessibleDescription(t.invalidEmail);
    expect(loginAction).not.toHaveBeenCalled();
  });

  it("signs in through the server action and lands on /tasks", async () => {
    vi.mocked(loginAction).mockResolvedValue({ ok: true, userId: "user-1" });
    renderWithProviders(<LoginForm onSwitchToRegister={vi.fn()} />);

    await fillAndSubmit();

    await waitFor(() =>
      expect(routerMock.replace).toHaveBeenCalledWith("/tasks"),
    );
    expect(routerMock.refresh).toHaveBeenCalled();
    expect(loginAction).toHaveBeenCalledWith({
      email: "ada@example.com",
      password: "secret",
      deviceId: expect.stringMatching(/^web-/),
    });
  });

  it("returns to the page the visitor was headed to", async () => {
    vi.mocked(loginAction).mockResolvedValue({ ok: true, userId: "user-1" });
    renderWithProviders(
      <LoginForm
        onSwitchToRegister={vi.fn()}
        redirectTo="/tasks?status=done"
      />,
    );

    await fillAndSubmit();

    await waitFor(() =>
      expect(routerMock.replace).toHaveBeenCalledWith("/tasks?status=done"),
    );
  });

  it("refuses an off-site redirect target", async () => {
    vi.mocked(loginAction).mockResolvedValue({ ok: true, userId: "user-1" });
    renderWithProviders(
      <LoginForm
        onSwitchToRegister={vi.fn()}
        redirectTo="https://evil.example"
      />,
    );

    await fillAndSubmit();

    await waitFor(() =>
      expect(routerMock.replace).toHaveBeenCalledWith("/tasks"),
    );
  });

  it("announces the server's rejection and stays on the page", async () => {
    vi.mocked(loginAction).mockResolvedValue({
      ok: false,
      error: "rejected",
      message: "invalid credentials",
    });
    renderWithProviders(<LoginForm onSwitchToRegister={vi.fn()} />);

    await fillAndSubmit();

    expect(await screen.findByRole("alert")).toHaveTextContent(
      t.error.replace("{message}", "invalid credentials"),
    );
    expect(routerMock.replace).not.toHaveBeenCalled();
  });

  it("shows a generic error — not a signed-in state — when no tokens came back", async () => {
    vi.mocked(loginAction).mockResolvedValue({ ok: false, error: "noTokens" });
    renderWithProviders(<LoginForm onSwitchToRegister={vi.fn()} />);

    await fillAndSubmit();

    expect(await screen.findByRole("alert")).toHaveTextContent(
      messages.common.errors.generic,
    );
    expect(routerMock.replace).not.toHaveBeenCalled();
  });

  it("disables the submit button while the request is in flight", async () => {
    let finish: (value: { ok: true; userId: string }) => void = () => {};
    vi.mocked(loginAction).mockReturnValue(
      new Promise((resolve) => {
        finish = resolve;
      }),
    );
    renderWithProviders(<LoginForm onSwitchToRegister={vi.fn()} />);

    await fillAndSubmit();

    expect(screen.getByRole("button", { name: t.submit })).toBeDisabled();
    finish({ ok: true, userId: "user-1" });
    await waitFor(() => expect(routerMock.replace).toHaveBeenCalled());
  });

  it("offers browsers the right autofill hints", () => {
    renderWithProviders(<LoginForm onSwitchToRegister={vi.fn()} />);

    expect(screen.getByLabelText(t.emailLabel)).toHaveAttribute(
      "autocomplete",
      "email",
    );
    expect(screen.getByLabelText(t.passwordLabel)).toHaveAttribute(
      "autocomplete",
      "current-password",
    );
  });

  it("lets the visitor switch to registration", async () => {
    const onSwitch = vi.fn();
    renderWithProviders(<LoginForm onSwitchToRegister={onSwitch} />);

    await userEvent.click(screen.getByRole("button", { name: t.switchAction }));

    expect(onSwitch).toHaveBeenCalledOnce();
  });
});
