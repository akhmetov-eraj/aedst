import { render, renderHook, screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { NextIntlClientProvider } from "next-intl";
import { describe, expect, it, vi } from "vitest";

import { ConsentBanner } from "@/app/providers/consent-banner";
import { ConsentProvider, useConsent } from "@/shared/lib/consent/consent";
import { saveConsentAction } from "@/shared/lib/consent/consent-action";
import { parseConsent } from "@/shared/lib/consent/consent-cookie";
import { messages } from "@/test/render";

vi.mock("@/shared/lib/consent/consent-action", () => ({
  saveConsentAction: vi.fn(),
}));

describe("parseConsent", () => {
  it("accepts only the two recorded answers", () => {
    expect(parseConsent("granted")).toBe("granted");
    expect(parseConsent("denied")).toBe("denied");
    expect(parseConsent("yes")).toBe("unknown");
    expect(parseConsent(undefined)).toBe("unknown");
  });
});

describe("ConsentProvider", () => {
  const wrap =
    (initial: "unknown" | "granted" | "denied") =>
    ({ children }: { children: React.ReactNode }) => (
      <ConsentProvider initial={initial}>{children}</ConsentProvider>
    );

  it("starts from the server-provided answer — no flash for returning visitors", () => {
    const { result } = renderHook(() => useConsent(), {
      wrapper: wrap("granted"),
    });

    expect(result.current.consent).toBe("granted");
  });

  it("applies a choice immediately and persists it through the server action", async () => {
    vi.mocked(saveConsentAction).mockResolvedValue(undefined);
    const { result } = renderHook(() => useConsent(), {
      wrapper: wrap("unknown"),
    });

    result.current.grant();

    await waitFor(() => expect(result.current.consent).toBe("granted"));
    expect(saveConsentAction).toHaveBeenCalledWith("granted");

    result.current.deny();
    await waitFor(() => expect(result.current.consent).toBe("denied"));
    expect(saveConsentAction).toHaveBeenLastCalledWith("denied");
  });

  it("keeps the choice for this page view even when the cookie write fails", async () => {
    vi.mocked(saveConsentAction).mockRejectedValue(new Error("offline"));
    const { result } = renderHook(() => useConsent(), {
      wrapper: wrap("unknown"),
    });

    result.current.deny();

    await waitFor(() => expect(result.current.consent).toBe("denied"));
  });

  it("throws outside the provider", () => {
    const spy = vi.spyOn(console, "error").mockImplementation(() => {});
    expect(() => renderHook(() => useConsent())).toThrow(/ConsentProvider/);
    spy.mockRestore();
  });
});

describe("ConsentBanner", () => {
  it("offers accept/decline and records the answer", async () => {
    vi.mocked(saveConsentAction).mockResolvedValue(undefined);
    const Probe = () => <p data-testid="state">{useConsent().consent}</p>;
    render(
      <NextIntlClientProvider locale="en" messages={messages}>
        <ConsentProvider initial="unknown">
          <Probe />
          <ConsentBanner />
        </ConsentProvider>
      </NextIntlClientProvider>,
    );

    expect(
      screen.getByRole("region", { name: messages.common.consent.title }),
    ).toBeInTheDocument();
    await userEvent.click(
      screen.getByRole("button", { name: messages.common.consent.accept }),
    );

    await waitFor(() =>
      expect(screen.getByTestId("state")).toHaveTextContent("granted"),
    );
  });
});
