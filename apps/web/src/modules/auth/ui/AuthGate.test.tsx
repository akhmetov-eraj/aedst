import { screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, expect, it, vi } from "vitest";

import { AuthGate } from "@/modules/auth/ui/AuthGate";
import { messages, renderWithProviders } from "@/test/render";

vi.mock("@/modules/auth/lib/auth-actions", () => ({
  loginAction: vi.fn(),
  registerAction: vi.fn(),
  logoutAction: vi.fn(),
}));

describe("AuthGate", () => {
  it("shows the sign-in form first", () => {
    renderWithProviders(<AuthGate />);

    expect(
      screen.getByRole("heading", { name: messages.auth.login.title }),
    ).toBeInTheDocument();
  });

  it("toggles between sign-in and registration", async () => {
    const user = userEvent.setup();
    renderWithProviders(<AuthGate />);

    await user.click(
      screen.getByRole("button", { name: messages.auth.login.switchAction }),
    );
    expect(
      screen.getByRole("heading", { name: messages.auth.register.title }),
    ).toBeInTheDocument();

    await user.click(
      screen.getByRole("button", { name: messages.auth.register.switchAction }),
    );
    expect(
      screen.getByRole("heading", { name: messages.auth.login.title }),
    ).toBeInTheDocument();
  });
});
