import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, describe, expect, it } from "vitest";

import { AuthProvider, useAuth } from "./auth-provider";
import { loadTokens } from "./token-storage";

function Probe() {
  const { isAuthenticated, isLoading, login, logout } = useAuth();
  if (isLoading) {
    return <p>loading</p>;
  }
  return (
    <div>
      <p>{isAuthenticated ? "signed-in" : "signed-out"}</p>
      <button
        type="button"
        onClick={() => login({ accessToken: "a", refreshToken: "r" })}
      >
        login
      </button>
      <button type="button" onClick={logout}>
        logout
      </button>
    </div>
  );
}

afterEach(() => {
  window.localStorage.clear();
});

describe("AuthProvider", () => {
  it("starts signed out and persists tokens to localStorage on login", async () => {
    const user = userEvent.setup();
    render(
      <AuthProvider>
        <Probe />
      </AuthProvider>,
    );

    expect(await screen.findByText("signed-out")).toBeInTheDocument();
    expect(loadTokens()).toBeNull();

    await user.click(screen.getByRole("button", { name: "login" }));

    expect(await screen.findByText("signed-in")).toBeInTheDocument();
    expect(loadTokens()).toEqual({ accessToken: "a", refreshToken: "r" });
  });

  it("clears the session on logout", async () => {
    const user = userEvent.setup();
    render(
      <AuthProvider>
        <Probe />
      </AuthProvider>,
    );

    await user.click(await screen.findByRole("button", { name: "login" }));
    await screen.findByText("signed-in");

    await user.click(screen.getByRole("button", { name: "logout" }));

    expect(await screen.findByText("signed-out")).toBeInTheDocument();
    expect(loadTokens()).toBeNull();
  });

  it("reads an existing session from localStorage on mount", async () => {
    window.localStorage.setItem(
      "aedst.auth.tokens",
      JSON.stringify({ accessToken: "x", refreshToken: "y" }),
    );

    render(
      <AuthProvider>
        <Probe />
      </AuthProvider>,
    );

    expect(await screen.findByText("signed-in")).toBeInTheDocument();
  });
});
