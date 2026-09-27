import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import LocaleLoading from "./loading";

describe("LocaleLoading", () => {
  it("renders an accessible loading indicator", () => {
    render(<LocaleLoading />);

    expect(screen.getByRole("status", { name: "Loading" })).toBeInTheDocument();
  });
});
