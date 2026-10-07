import { describe, expect, it } from "vitest";

import { locales } from "@/app/config/i18n/locales";
import { formatDate, formatDateTime } from "@/shared/date/format-date";

const date = new Date(2026, 0, 15, 14, 30);

describe("formatDate / formatDateTime", () => {
  it("formats per locale with date-fns", () => {
    expect(formatDate(date, "en")).toBe("Jan 15, 2026");
    expect(formatDate(date, "ru")).toContain("2026");
    expect(formatDateTime(date, "en")).toContain("2:30 PM");
  });

  it("gives every supported locale a formatter (Tajik falls back to Russian)", () => {
    for (const locale of locales) {
      expect(formatDate(date, locale).length).toBeGreaterThan(4);
    }
    expect(formatDate(date, "tj")).toBe(formatDate(date, "ru"));
  });
});
