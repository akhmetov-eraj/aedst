import {
  expect,
  logIn,
  mockApi,
  signInThroughUi,
  test,
} from "./support/fixtures";

test.describe("route protection", () => {
  test("sends an anonymous visitor to login and back to where they were headed", async ({
    page,
    user,
  }) => {
    await page.goto("/en/tasks?status=done&q=Seed");

    // Redirected by proxy.ts before any page HTML renders.
    await expect(page).toHaveURL(
      /\/en\?next=%2Ftasks%3Fstatus%3Ddone%26q%3DSeed/,
    );

    await logIn(page, user);

    await expect(page).toHaveURL(/\/en\/tasks\?status=done&q=Seed/);
  });

  test("never redirects off-site through the next parameter", async ({
    page,
    user,
  }) => {
    await page.goto("/en?next=https%3A%2F%2Fevil.example%2Fphish");
    await logIn(page, user);

    await expect(page).toHaveURL(/\/en\/tasks$/);
  });

  test("skips the login page for an already signed-in visitor", async ({
    page,
    signedIn,
  }) => {
    expect(signedIn.email).toBeTruthy();
    await page.goto("/en");
    await expect(page).toHaveURL(/\/en\/tasks$/);
  });

  test("protects nested routes in every locale", async ({ page }) => {
    await page.goto("/ru/tasks/new");
    await expect(page).toHaveURL(/\/ru\?next=%2Ftasks%2Fnew/);
  });
});

test.describe("sign in / out", () => {
  test("shows an accessible error for wrong credentials", async ({
    page,
    user,
  }) => {
    await page.goto("/en");
    await page.getByLabel("Email").fill(user.email);
    await page.getByLabel("Password").fill("wrong-password");
    await page.getByRole("button", { name: "Log in" }).click();

    await expect(
      page.getByRole("alert").filter({ hasText: "invalid credentials" }),
    ).toBeVisible();
    await expect(page).toHaveURL(/\/en$/);
  });

  test("validates the form before submitting", async ({ page }) => {
    await page.goto("/en");
    await page.getByRole("button", { name: "Log in" }).click();

    await expect(page.getByText("Enter a valid email address.")).toBeVisible();
    await expect(page.getByLabel("Email")).toHaveAttribute(
      "aria-invalid",
      "true",
    );
  });

  test("registers a new account and lands on the dashboard", async ({
    page,
  }) => {
    await page.goto("/en");
    await page.getByRole("button", { name: "Create one" }).click();
    await page
      .getByLabel("Email")
      .fill(`new-${Date.now()}-${Math.random()}@example.com`);
    await page.getByLabel("Password").fill("a-long-password");
    await page.getByRole("button", { name: "Create account" }).click();

    await expect(page).toHaveURL(/\/en\/tasks$/);
    await expect(page.getByText("No tasks yet")).toBeVisible();
  });

  test("signing out ends the session for good", async ({ page, signedIn }) => {
    expect(signedIn.email).toBeTruthy();
    await page.getByRole("button", { name: "Sign out" }).click();
    await expect(page).toHaveURL(/\/en$/);

    await page.goto("/en/tasks");
    await expect(page).toHaveURL(/\/en\?next=/);
  });
});

test.describe("session storage", () => {
  test("tokens live only in httpOnly cookies, out of reach of page scripts", async ({
    page,
    context,
    browserName,
    signedIn,
  }) => {
    expect(signedIn.email).toBeTruthy();

    const cookies = await context.cookies();
    const session = cookies.filter((c) => c.name.startsWith("aedst_session"));
    expect(session.map((c) => c.name).sort()).toEqual([
      "aedst_session",
      "aedst_session_refresh",
    ]);
    for (const cookie of session) {
      expect(cookie.httpOnly).toBe(true);
      // WebKit's automation API reports Lax cookies as "None"; the header
      // itself is asserted by the unit tests.
      if (browserName !== "webkit") {
        expect(cookie.sameSite).toBe("Lax");
      }
    }

    const visibleToScripts = await page.evaluate(() => ({
      cookie: document.cookie,
      local: JSON.stringify({ ...localStorage }),
      session: JSON.stringify({ ...sessionStorage }),
    }));
    expect(visibleToScripts.cookie).not.toContain("aedst_session");
    expect(visibleToScripts.local).not.toMatch(/token|access|refresh/i);
    expect(visibleToScripts.session).not.toMatch(/token|access|refresh/i);
  });

  test("the browser never calls the API directly — only the same-origin proxy", async ({
    page,
    user,
  }) => {
    const origins = new Set<string>();
    page.on("request", (request) => {
      if (request.resourceType() === "fetch") {
        origins.add(new URL(request.url()).origin);
      }
    });

    await signInThroughUi(page, user);
    await page.getByRole("button", { name: "Next", exact: true }).click();
    await expect(page).toHaveURL(/cursors=/);

    for (const origin of origins) {
      expect(origin).toBe("http://127.0.0.1:3100");
    }
  });
});

test.describe("session refresh", () => {
  test("an expired access token is renewed transparently on a full page load", async ({
    page,
    signedIn,
  }) => {
    await mockApi.expireAccessToken(signedIn.email);

    await page.reload();

    await expect(page).toHaveURL(/\/en\/tasks/);
    await expect(page.getByText("Seed task 25")).toBeVisible();
  });

  test("an expired access token is renewed transparently on a client-side request", async ({
    page,
    signedIn,
  }) => {
    await mockApi.expireAccessToken(signedIn.email);

    // A client-side navigation: goes through the /api/rpc BFF, which
    // retries once after refreshing.
    await page.getByRole("button", { name: "Next", exact: true }).click();

    await expect(page.getByText("Seed task 05")).toBeVisible();
    await expect(page).toHaveURL(/\/en\/tasks/);
  });

  test("a session revoked server-side ends cleanly: explanation, login page, way back", async ({
    page,
    signedIn,
  }) => {
    await mockApi.revokeSession(signedIn.email);

    await page.getByRole("button", { name: "Next", exact: true }).click();

    await expect(page).toHaveURL(/\/en\?next=/);
    await expect(page.getByText("Your session expired")).toBeVisible();
    await expect(page.getByRole("heading", { name: "Sign in" })).toBeVisible();
  });
});
