import { render, screen } from "@testing-library/react";
import { NextIntlClientProvider } from "next-intl";
import { describe, expect, it, vi } from "vitest";

import {
  disableSessionReplay,
  enableSessionReplay,
} from "@/app/config/sentry/client";
import { ConsentLayer } from "@/app/providers/consent-layer";
import {
  disableAnalytics,
  enableAnalytics,
} from "@/shared/lib/analytics/analytics";
import { ConsentProvider } from "@/shared/lib/consent/consent";
import type { ConsentState } from "@/shared/lib/consent/consent-cookie";
import { messages } from "@/test/render";

vi.mock("@/app/config/sentry/client", () => ({
  enableSessionReplay: vi.fn(),
  disableSessionReplay: vi.fn(),
}));
vi.mock("@/shared/lib/analytics/analytics", () => ({
  enableAnalytics: vi.fn(),
  disableAnalytics: vi.fn(),
}));
vi.mock("@/shared/lib/consent/consent-action", () => ({
  saveConsentAction: vi.fn(),
}));

function renderLayer(initial: ConsentState) {
  return render(
    <NextIntlClientProvider locale="en" messages={messages}>
      <ConsentProvider initial={initial}>
        <ConsentLayer />
      </ConsentProvider>
    </NextIntlClientProvider>,
  );
}

describe("ConsentLayer", () => {
  it("asks first, and measures nothing, while the answer is unknown", () => {
    renderLayer("unknown");

    expect(
      screen.getByRole("region", { name: messages.common.consent.title }),
    ).toBeVisible();
    expect(enableAnalytics).not.toHaveBeenCalled();
    expect(enableSessionReplay).not.toHaveBeenCalled();
  });

  it("starts analytics and session replay once granted — and stops asking", () => {
    renderLayer("granted");

    expect(enableAnalytics).toHaveBeenCalledOnce();
    expect(enableSessionReplay).toHaveBeenCalledOnce();
    expect(screen.queryByRole("region")).not.toBeInTheDocument();
  });

  it("stays fully off, and quiet, after a refusal", () => {
    renderLayer("denied");

    expect(enableAnalytics).not.toHaveBeenCalled();
    expect(enableSessionReplay).not.toHaveBeenCalled();
    expect(disableAnalytics).toHaveBeenCalled();
    expect(disableSessionReplay).toHaveBeenCalled();
    expect(screen.queryByRole("region")).not.toBeInTheDocument();
  });
});
