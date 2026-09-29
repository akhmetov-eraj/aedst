import { TransportProvider } from "@connectrpc/connect-query";
import { createConnectTransport } from "@connectrpc/connect-web";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { render, screen } from "@testing-library/react";
import { NextIntlClientProvider } from "next-intl";
import { describe, expect, it, vi } from "vitest";

import authMessages from "@/modules/auth/i18n/en.json";
import homeMessages from "@/modules/home/i18n/en.json";
import { AuthProvider } from "@/shared/lib/auth/auth-provider";
import HomePage from "./page";

// `next-intl`'s `useRouter` (used by `AuthGate` to redirect once signed in)
// wraps `next/navigation`'s, which needs a real App Router tree to resolve —
// not present in this jsdom render. Mocked here rather than globally in
// vitest.config.mts since no other test in this suite exercises navigation
// yet; promote it to a shared setup helper if that changes.
vi.mock("@/app/config/i18n/navigation", () => ({
  useRouter: () => ({ replace: vi.fn(), push: vi.fn() }),
}));

function renderHomePage() {
  const queryClient = new QueryClient();
  const transport = createConnectTransport({
    baseUrl: "http://localhost:8080",
  });

  return render(
    <NextIntlClientProvider
      locale="en"
      messages={{ home: homeMessages, auth: authMessages }}
    >
      <TransportProvider transport={transport}>
        <QueryClientProvider client={queryClient}>
          <AuthProvider>
            <HomePage />
          </AuthProvider>
        </QueryClientProvider>
      </TransportProvider>
    </NextIntlClientProvider>,
  );
}

describe("HomePage", () => {
  it("renders the localized title and description", () => {
    renderHomePage();

    expect(
      screen.getByRole("heading", { name: homeMessages.title }),
    ).toBeInTheDocument();
    expect(screen.getByText(homeMessages.description)).toBeInTheDocument();
  });

  it("renders the ConnectRPC login example form for a signed-out visitor", async () => {
    renderHomePage();

    expect(
      await screen.findByRole("heading", { name: authMessages.login.title }),
    ).toBeInTheDocument();
    expect(
      screen.getByRole("button", { name: authMessages.login.submit }),
    ).toBeInTheDocument();
  });

  it("lets a signed-out visitor switch to the register form", async () => {
    const { default: userEvent } = await import("@testing-library/user-event");
    renderHomePage();

    await userEvent.click(
      await screen.findByRole("button", {
        name: authMessages.login.switchAction,
      }),
    );

    expect(
      screen.getByRole("heading", { name: authMessages.register.title }),
    ).toBeInTheDocument();
  });
});
