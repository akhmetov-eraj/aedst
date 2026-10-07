import { describe, expect, it } from "vitest";

import { locales, toLanguageTag } from "@/app/config/i18n/locales";
import { buildLanguageAlternates, siteConfig } from "@/app/config/seo";
import robots from "@/app/robots";

describe("buildLanguageAlternates", () => {
  it("emits an absolute hreflang URL for every locale plus x-default", () => {
    const alternates = buildLanguageAlternates("tasks");

    for (const locale of locales) {
      expect(alternates[toLanguageTag(locale)]).toBe(
        `http://localhost:3000/${locale}/tasks`,
      );
    }
    expect(alternates["x-default"]).toBe("http://localhost:3000/en/tasks");
  });

  it("uses real BCP 47 tags: Tajik is tg, not the country code tj", () => {
    const alternates = buildLanguageAlternates("");

    expect(alternates.tg).toBe("http://localhost:3000/tj");
    expect(alternates.tj).toBeUndefined();
  });

  it("handles the root path without a trailing slash", () => {
    expect(buildLanguageAlternates("").ru).toBe("http://localhost:3000/ru");
    expect(buildLanguageAlternates("")["x-default"]).toBe(
      "http://localhost:3000/en",
    );
  });
});

describe("siteConfig", () => {
  it("is neutral template branding with an absolute origin", () => {
    expect(siteConfig.name).toBe("AEDST");
    expect(siteConfig.url.origin).toBe("http://localhost:3000");
    expect(siteConfig.description).not.toMatch(/sign in/i);
  });
});

describe("robots", () => {
  it("hides signed-in areas and the BFF proxy, and points at the sitemap", () => {
    const { rules, sitemap } = robots();
    const rule = Array.isArray(rules) ? rules[0] : rules;

    expect(rule?.disallow).toEqual(
      expect.arrayContaining(["/api/", "/en/tasks", "/ar/tasks"]),
    );
    expect(sitemap).toBe("http://localhost:3000/sitemap.xml");
  });
});
