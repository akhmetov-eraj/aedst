import { beforeEach, describe, expect, it, vi } from "vitest";

import { saveConsentAction } from "@/shared/lib/consent/consent-action";

const set = vi.fn();

vi.mock("next/headers", () => ({
  cookies: async () => ({ set }),
}));

beforeEach(() => set.mockReset());

describe("saveConsentAction", () => {
  it("stores the answer in a year-long first-party cookie the server can read", async () => {
    await saveConsentAction("granted");

    expect(set).toHaveBeenCalledWith("aedst_consent", "granted", {
      httpOnly: false,
      secure: false,
      sameSite: "lax",
      path: "/",
      maxAge: 31_536_000,
    });
  });

  it.each(["yes", "", null, undefined, 1, { a: 1 }])(
    "ignores anything but granted/denied: %j",
    async (choice) => {
      await saveConsentAction(choice);

      expect(set).not.toHaveBeenCalled();
    },
  );
});
