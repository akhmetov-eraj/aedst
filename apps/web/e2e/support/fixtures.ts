import { test as base, expect, type Page } from "@playwright/test";

const MOCK_API_URL = "http://127.0.0.1:8181";

export type SeededUser = { email: string; password: string; userId: string };

async function mockPost<T>(path: string, body: object): Promise<T> {
  const response = await fetch(`${MOCK_API_URL}${path}`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(body),
  });
  if (!response.ok) {
    throw new Error(`mock API ${path} -> ${response.status}`);
  }
  return (await response.json()) as T;
}

export const mockApi = {
  seedUser: (
    email: string,
    tasks = 25,
    options: { roles?: string[]; notifications?: number } = {},
  ) => mockPost<SeededUser>("/__test/seed", { email, tasks, ...options }),
  /** The access token stops working; the refresh token still does. */
  expireAccessToken: (email: string) =>
    mockPost("/__test/expire-access", { email }),
  /** The whole session is revoked server-side. */
  revokeSession: (email: string) =>
    mockPost("/__test/revoke-session", { email }),
};

/**
 * Fills and submits the login form on the current page. It first waits for the
 * submit button to enable — the app keeps it disabled until React has hydrated
 * (see useHydrated) — because text typed into an uncontrolled field before
 * hydration can be reset by the form library taking over the input.
 */
export async function logIn(page: Page, user: SeededUser) {
  const submit = page.getByRole("button", { name: "Log in" });
  await expect(submit).toBeEnabled();
  await page.getByLabel("Email").fill(user.email);
  await page.getByLabel("Password").fill(user.password);
  await submit.click();
}

/** Signs in through the real login form (so it exercises the Server Action). */
export async function signInThroughUi(page: Page, user: SeededUser) {
  await page.goto("/en");
  await logIn(page, user);
  await expect(page).toHaveURL(/\/en\/tasks/);
}

type Fixtures = {
  /**
   * Analytics-consent state the browser starts with. `denied` by default so
   * the (fixed-position) consent banner never covers the controls a test is
   * about to click; the consent spec opts into `unknown` to see the banner.
   */
  consent: "denied" | "unknown";
  /** A user seeded with 25 tasks, unique to this test (safe under parallelism). */
  user: SeededUser;
  /** The same user, already signed in through the UI. */
  signedIn: SeededUser;
  /** A user with the admin role (5 tasks, 3 notifications), signed in. */
  admin: SeededUser;
};

let counter = 0;

export const test = base.extend<Fixtures>({
  consent: ["denied", { option: true }],
  context: async ({ context, consent, baseURL }, use) => {
    if (consent !== "unknown" && baseURL) {
      await context.addCookies([
        { name: "aedst_consent", value: consent, url: baseURL },
      ]);
    }
    await use(context);
  },
  // biome-ignore lint/correctness/noEmptyPattern: Playwright fixtures require destructuring
  user: async ({}, use, testInfo) => {
    counter += 1;
    const email = `e2e-${testInfo.workerIndex}-${counter}-${Date.now()}@example.com`;
    await use(await mockApi.seedUser(email));
  },
  signedIn: async ({ page, user }, use) => {
    await signInThroughUi(page, user);
    await use(user);
  },
  admin: async ({ page }, use, testInfo) => {
    counter += 1;
    const email = `e2e-admin-${testInfo.workerIndex}-${counter}-${Date.now()}@example.com`;
    const admin = await mockApi.seedUser(email, 5, {
      roles: ["user", "admin"],
      notifications: 3,
    });
    await signInThroughUi(page, admin);
    await use(admin);
  },
});

export { expect };
