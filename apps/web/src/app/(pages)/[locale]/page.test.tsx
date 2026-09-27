import { render, screen } from "@testing-library/react";
import { NextIntlClientProvider } from "next-intl";
import { describe, expect, it } from "vitest";

import homeMessages from "@/modules/home/i18n/en.json";
import HomePage from "./page";

describe("HomePage", () => {
  it("renders the localized title and description", () => {
    render(
      <NextIntlClientProvider locale="en" messages={{ home: homeMessages }}>
        <HomePage />
      </NextIntlClientProvider>,
    );

    expect(
      screen.getByRole("heading", { name: homeMessages.title }),
    ).toBeInTheDocument();
    expect(screen.getByText(homeMessages.description)).toBeInTheDocument();
  });
});
