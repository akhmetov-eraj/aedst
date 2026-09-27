import { expect, test } from "@playwright/test";

test("redirects to the default locale and renders the home page", async ({
  page,
}) => {
  await page.goto("/");

  await expect(page).toHaveURL(/\/en\/?$/);
  await expect(
    page.getByRole("heading", { name: "Hello, World!" }),
  ).toBeVisible();
});
