import { TransportProvider } from "@connectrpc/connect-query";
import { createConnectTransport } from "@connectrpc/connect-web";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { render, screen } from "@testing-library/react";
import { NextIntlClientProvider } from "next-intl";
import { describe, expect, it } from "vitest";

import homeMessages from "@/modules/home/i18n/en.json";
import HomePage from "./page";

function renderHomePage() {
  const queryClient = new QueryClient();
  const transport = createConnectTransport({ baseUrl: "http://localhost:8080" });

  return render(
    <NextIntlClientProvider locale="en" messages={{ home: homeMessages }}>
      <TransportProvider transport={transport}>
        <QueryClientProvider client={queryClient}>
          <HomePage />
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

  it("renders the ConnectRPC login example form", () => {
    renderHomePage();

    expect(
      screen.getByRole("heading", { name: homeMessages.auth.title }),
    ).toBeInTheDocument();
    expect(
      screen.getByRole("button", { name: homeMessages.auth.submit }),
    ).toBeInTheDocument();
  });
});
