import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, expect, it, vi } from "vitest";

import { registerAction } from "@/modules/auth/lib/auth-actions";
import { RegisterForm } from "@/modules/auth/ui/RegisterForm";
import { routerMock } from "@/test/mocks/navigation";
import { messages, renderWithProviders } from "@/test/render";

vi.mock("@/modules/auth/lib/auth-actions", () => ({
  loginAction: vi.fn(),
  registerAction: vi.fn(),
  logoutAction: vi.fn(),
}));

const t = messages.auth.register;

describe("RegisterForm", () => {
  it("requires an 8+ character password before calling the server", async () => {
    const user = userEvent.setup();
    renderWithProviders(<RegisterForm onSwitchToLogin={vi.fn()} />);

    await user.type(screen.getByLabelText(t.emailLabel), "ada@example.com");
    await user.type(screen.getByLabelText(t.passwordLabel), "short");
    await user.click(screen.getByRole("button", { name: t.submit }));

    expect(await screen.findByText(t.passwordTooShort)).toBeInTheDocument();
    expect(screen.getByLabelText(t.passwordLabel)).toHaveAccessibleDescription(
      t.passwordTooShort,
    );
    expect(registerAction).not.toHaveBeenCalled();
  });

  it("creates the account, signs in and navigates", async () => {
    vi.mocked(registerAction).mockResolvedValue({ ok: true, userId: "user-2" });
    const user = userEvent.setup();
    renderWithProviders(
      <RegisterForm onSwitchToLogin={vi.fn()} redirectTo="/tasks/new" />,
    );

    await user.type(screen.getByLabelText(t.emailLabel), "ada@example.com");
    await user.type(screen.getByLabelText(t.passwordLabel), "long enough pw");
    await user.click(screen.getByRole("button", { name: t.submit }));

    await waitFor(() =>
      expect(routerMock.replace).toHaveBeenCalledWith("/tasks/new"),
    );
    expect(registerAction).toHaveBeenCalledWith({
      email: "ada@example.com",
      password: "long enough pw",
      deviceId: expect.stringMatching(/^web-/),
    });
  });

  it("announces a server-side rejection", async () => {
    vi.mocked(registerAction).mockResolvedValue({
      ok: false,
      error: "rejected",
      message: "email already registered",
    });
    const user = userEvent.setup();
    renderWithProviders(<RegisterForm onSwitchToLogin={vi.fn()} />);

    await user.type(screen.getByLabelText(t.emailLabel), "ada@example.com");
    await user.type(screen.getByLabelText(t.passwordLabel), "long enough pw");
    await user.click(screen.getByRole("button", { name: t.submit }));

    expect(await screen.findByRole("alert")).toHaveTextContent(
      "email already registered",
    );
  });

  it("uses new-password autofill and can switch back to sign-in", async () => {
    const onSwitch = vi.fn();
    renderWithProviders(<RegisterForm onSwitchToLogin={onSwitch} />);

    expect(screen.getByLabelText(t.passwordLabel)).toHaveAttribute(
      "autocomplete",
      "new-password",
    );
    await userEvent.click(screen.getByRole("button", { name: t.switchAction }));
    expect(onSwitch).toHaveBeenCalledOnce();
  });
});
