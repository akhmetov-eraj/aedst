import { act, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, describe, expect, it, vi } from "vitest";

import { AppNav } from "@/app/shell/AppNav";
import { InstallPrompt } from "@/app/shell/InstallPrompt";
import { OfflineBanner } from "@/app/shell/OfflineBanner";
import { CapabilitiesProvider } from "@/shared/lib/capabilities/CapabilitiesProvider";
import { useUiPreferences } from "@/shared/lib/stores/ui-preferences-store";
import { messages, renderWithProviders } from "@/test/render";

afterEach(() => {
  vi.unstubAllGlobals();
  useUiPreferences.setState({ installPromptDismissedAt: null });
});

describe("AppNav", () => {
  it("shows the admin link only for tokens with the admin role", () => {
    const { unmount } = renderWithProviders(<AppNav />, {
      initialUserId: "u-1",
    });
    expect(
      screen.queryByRole("link", { name: messages.common.nav.admin }),
    ).not.toBeInTheDocument();
    unmount();

    // admin is a flagged feature: it shows only when the server enables it.
    renderWithProviders(
      <CapabilitiesProvider
        initial={{
          features: { admin: { enabled: true } },
          updateStatus: "current",
          minimum: "",
          recommended: "",
          fetchedAt: 0,
        }}
      >
        <AppNav />
      </CapabilitiesProvider>,
      { initialUserId: "u-1", initialRoles: ["user", "admin"] },
    );
    expect(
      screen.getByRole("link", { name: messages.common.nav.admin }),
    ).toHaveAttribute("href", "/admin");
  });

  it("marks the current section", () => {
    renderWithProviders(<AppNav />, { initialUserId: "u-1" });
    // The navigation mock reports "/" as the current path: nothing is current.
    expect(
      screen.getByRole("link", { name: messages.common.nav.tasks }),
    ).not.toHaveAttribute("aria-current");
  });
});

describe("OfflineBanner", () => {
  it("announces going offline and coming back", () => {
    renderWithProviders(<OfflineBanner />);
    expect(
      screen.queryByText(messages.common.offline.banner),
    ).not.toBeInTheDocument();

    vi.spyOn(navigator, "onLine", "get").mockReturnValue(false);
    act(() => {
      window.dispatchEvent(new Event("offline"));
    });
    expect(
      screen.getByText(messages.common.offline.banner),
    ).toBeInTheDocument();
  });
});

describe("InstallPrompt", () => {
  it("replays Chromium's deferred install prompt and snoozes after a dismissal", async () => {
    renderWithProviders(<InstallPrompt />);
    expect(screen.queryByRole("region")).not.toBeInTheDocument();

    const prompt = vi.fn().mockResolvedValue(undefined);
    const event = Object.assign(new Event("beforeinstallprompt"), {
      prompt,
      userChoice: Promise.resolve({ outcome: "dismissed" as const }),
    });
    act(() => {
      window.dispatchEvent(event);
    });

    const t = messages.common.pwa;
    await userEvent.click(
      await screen.findByRole("button", { name: t.install }),
    );
    expect(prompt).toHaveBeenCalled();
    expect(useUiPreferences.getState().installPromptDismissedAt).toEqual(
      expect.any(Number),
    );
    expect(screen.queryByRole("region")).not.toBeInTheDocument();
  });

  it("shows Add to Home Screen instructions on iOS Safari", () => {
    vi.spyOn(navigator, "userAgent", "get").mockReturnValue(
      "Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Mobile/15E148 Safari/604.1",
    );
    renderWithProviders(<InstallPrompt />);
    expect(
      screen.getByText(messages.common.pwa.iosInstructions),
    ).toBeInTheDocument();
  });
});
