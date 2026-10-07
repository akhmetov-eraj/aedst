import { describe, expect, it } from "vitest";

import {
  base64UrlToBytes,
  describeBrowser,
  rememberDeviceId,
  rememberedDeviceId,
  subscriptionKeys,
} from "@/modules/notifications/lib/web-push";

describe("base64UrlToBytes", () => {
  it("decodes base64url without padding", () => {
    expect([...base64UrlToBytes("AQID_-8")]).toEqual([1, 2, 3, 255, 239]);
  });
});

describe("subscriptionKeys", () => {
  it("extracts endpoint and keys, and rejects incomplete subscriptions", () => {
    const subscription = (json: object) =>
      ({ toJSON: () => json }) as unknown as PushSubscription;
    expect(
      subscriptionKeys(
        subscription({
          endpoint: "https://push.example/1",
          keys: { p256dh: "p", auth: "a" },
        }),
      ),
    ).toEqual({ endpoint: "https://push.example/1", p256dh: "p", auth: "a" });
    expect(() =>
      subscriptionKeys(subscription({ endpoint: "https://push.example/1" })),
    ).toThrow();
  });
});

describe("device id memory", () => {
  it("round-trips and clears", () => {
    rememberDeviceId("dev-1");
    expect(rememberedDeviceId()).toBe("dev-1");
    rememberDeviceId(null);
    expect(rememberedDeviceId()).toBeNull();
  });
});

describe("describeBrowser", () => {
  it.each([
    [
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0 Safari/537.36",
      "Chrome · Windows",
    ],
    [
      "Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Mobile/15E148 Safari/604.1",
      "Safari · iOS",
    ],
    [
      "Mozilla/5.0 (X11; Linux x86_64; rv:140.0) Gecko/20100101 Firefox/140.0",
      "Firefox · Linux",
    ],
    ["curl/8", "Browser"],
  ])("%s → %s", (ua, expected) => {
    expect(describeBrowser(ua)).toBe(expected);
  });
});
