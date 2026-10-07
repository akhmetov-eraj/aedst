import { render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";

import homeMessages from "@/modules/home/i18n/en.json";
import { messages, renderWithProviders } from "@/test/render";
import HomePage from "./page";

vi.mock("@/modules/auth/lib/auth-actions", () => ({
  loginAction: vi.fn(),
  registerAction: vi.fn(),
  logoutAction: vi.fn(),
}));

// The page is an async Server Component; next-intl/server needs a request
// scope, so it is backed by the same English catalogue the client tests use.
vi.mock("next-intl/server", () => ({
  setRequestLocale: vi.fn(),
  getTranslations: async ({
    namespace,
  }: {
    namespace: keyof typeof messages;
  }) => {
    const catalogue = messages[namespace] as Record<string, string>;
    return (key: string) => catalogue[key] ?? key;
  },
}));

async function renderHomePage(query: Record<string, string> = {}) {
  const page = await HomePage({
    params: Promise.resolve({ locale: "en" }),
    searchParams: Promise.resolve(query),
  });
  return renderWithProviders(page);
}

describe("HomePage", () => {
  it("renders the localized title and description", async () => {
    await renderHomePage();

    expect(
      screen.getByRole("heading", { name: homeMessages.title }),
    ).toBeInTheDocument();
    expect(screen.getByText(homeMessages.description)).toBeInTheDocument();
  });

  it("renders the sign-in form for a signed-out visitor", async () => {
    await renderHomePage();

    expect(
      screen.getByRole("heading", { name: messages.auth.login.title }),
    ).toBeInTheDocument();
    expect(
      screen.getByRole("button", { name: messages.auth.login.submit }),
    ).toBeInTheDocument();
  });

  it("emits schema.org JSON-LD describing the app", async () => {
    const { container } = await renderHomePage();

    const script = container.querySelector(
      'script[type="application/ld+json"]',
    );
    expect(script).not.toBeNull();
    expect(JSON.parse(script?.textContent ?? "{}")).toMatchObject({
      "@context": "https://schema.org",
      "@type": "WebApplication",
      name: messages.common.appName,
      inLanguage: "en",
      url: "http://localhost:3000/en",
    });
  });

  it("escapes markup in JSON-LD so it cannot close its script tag", () => {
    // Guards the XSS vector directly through the component the page uses.
    return import("@/app/config/json-ld").then(({ JsonLd }) => {
      const { container } = render(
        <JsonLd data={{ name: "</script><script>alert(1)</script>" }} />,
      );
      expect(container.innerHTML).not.toContain("</script><script>");
      expect(container.querySelector("script")?.textContent).toContain(
        "\\u003c/script>",
      );
    });
  });
});
