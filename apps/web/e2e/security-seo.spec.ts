import { expect, test } from "./support/fixtures";

test.describe("Content-Security-Policy", () => {
  test("uses a per-request nonce and forbids inline scripts", async ({
    request,
  }) => {
    const first = await request.get("/en");
    const second = await request.get("/en");

    const csp = first.headers()["content-security-policy"] ?? "";
    const script =
      csp.split("; ").find((d) => d.startsWith("script-src")) ?? "";
    expect(script).toMatch(/'nonce-[^']+'/);
    expect(script).toContain("'strict-dynamic'");
    expect(script).not.toContain("'unsafe-inline'");
    expect(csp).toContain("connect-src 'self'");
    expect(csp).toContain("frame-ancestors 'none'");

    const nonceOf = (header: string) => /'nonce-([^']+)'/.exec(header)?.[1];
    expect(nonceOf(csp)).toBeTruthy();
    expect(nonceOf(csp)).not.toBe(
      nonceOf(second.headers()["content-security-policy"] ?? ""),
    );

    // Every script Next renders carries that request's nonce.
    const html = await first.text();
    const nonce = nonceOf(csp) as string;
    const tags = html.match(/<script\b[^>]*>/g) ?? [];
    const executable = tags.filter(
      (tag) => !tag.includes("application/ld+json"),
    );
    expect(executable.length).toBeGreaterThan(0);
    for (const tag of executable) {
      expect(tag).toContain(`nonce="${nonce}"`);
    }
  });

  test("the app runs with the policy enforced: no violations, page hydrates", async ({
    page,
  }) => {
    const violations: string[] = [];
    page.on("console", (message) => {
      if (/content security policy/i.test(message.text())) {
        violations.push(message.text());
      }
    });

    await page.goto("/en");
    // The submit button is disabled until React hydrates — so this proves the
    // scripts really ran under the nonce policy.
    await expect(page.getByRole("button", { name: "Log in" })).toBeEnabled();

    expect(violations).toEqual([]);
  });

  test("injected markup cannot run inline handlers", async ({ page }) => {
    await page.goto("/en");
    // What a stored-XSS payload looks like: markup with an inline handler.
    await page.evaluate(() => {
      document.body.insertAdjacentHTML(
        "beforeend",
        '<img src="x:x" onerror="window.__pwned = true">',
      );
    });
    await page.waitForTimeout(500);

    const executed = await page.evaluate(
      () => (window as unknown as { __pwned?: boolean }).__pwned === true,
    );
    expect(executed).toBe(false);
  });

  test("sends the static hardening headers too", async ({ request }) => {
    const headers = (await request.get("/en")).headers();
    expect(headers["x-content-type-options"]).toBe("nosniff");
    expect(headers["x-frame-options"]).toBe("DENY");
    expect(headers["strict-transport-security"]).toContain("max-age=");
    expect(headers["referrer-policy"]).toBe("strict-origin-when-cross-origin");
    expect(headers["x-powered-by"]).toBeUndefined();
  });
});

test.describe("BFF proxy", () => {
  test("rejects cross-origin and unauthenticated calls", async ({
    request,
  }) => {
    const crossOrigin = await request.post(
      "/api/rpc/aedst.tasks.v1.TasksService/ListTasks",
      { data: {}, headers: { origin: "https://evil.example" } },
    );
    expect(crossOrigin.status()).toBe(403);

    const anonymous = await request.post(
      "/api/rpc/aedst.tasks.v1.TasksService/ListTasks",
      { data: {}, headers: { origin: "http://127.0.0.1:3100" } },
    );
    expect(anonymous.status()).toBe(401);
  });

  test("will not mint tokens for page JavaScript", async ({
    request,
    user,
  }) => {
    const response = await request.post(
      "/api/rpc/aedst.auth.v1.AuthService/Login",
      {
        data: { email: user.email, password: user.password, deviceId: "x" },
        headers: { origin: "http://127.0.0.1:3100" },
      },
    );
    expect(response.status()).toBe(404);
    expect(await response.text()).not.toContain("accessToken");
  });
});

test.describe("SEO", () => {
  test("the home page publishes JSON-LD, canonical alternates and a description", async ({
    page,
  }) => {
    await page.goto("/en");

    const jsonLd = await page
      .locator('script[type="application/ld+json"]')
      .textContent();
    expect(JSON.parse(jsonLd ?? "{}")).toMatchObject({
      "@context": "https://schema.org",
      "@type": "WebApplication",
      name: "AEDST",
      inLanguage: "en",
    });

    await expect(page.locator('meta[name="description"]')).toHaveAttribute(
      "content",
      /task manager/i,
    );
    const alternates = await page
      .locator('link[rel="alternate"][hreflang]')
      .evaluateAll((links) => links.map((l) => l.getAttribute("hreflang")));
    expect(alternates).toEqual(
      expect.arrayContaining(["en", "ru", "tg", "uz", "ar", "x-default"]),
    );
    await expect(page.locator("html")).toHaveAttribute("lang", "en");
  });

  test("signed-in pages are explicitly noindex", async ({ page, signedIn }) => {
    expect(signedIn.email).toBeTruthy();
    await expect(page.locator('meta[name="robots"]')).toHaveAttribute(
      "content",
      /noindex/,
    );
  });

  test("robots.txt hides private areas and lists the sitemap", async ({
    request,
  }) => {
    const body = await (await request.get("/robots.txt")).text();
    expect(body).toContain("Disallow: /api/");
    expect(body).toContain("Disallow: /en/tasks");
    expect(body).toContain("Sitemap:");
  });

  test("sitemap lists every locale", async ({ request }) => {
    const body = await (await request.get("/sitemap.xml")).text();
    for (const locale of ["en", "ru", "tj", "uz", "ar"]) {
      expect(body).toContain(`/${locale}<`);
    }
  });
});

test.describe("accessibility affordances", () => {
  test("a skip link is the first tab stop and moves focus to the content", async ({
    page,
    browserName,
  }) => {
    test.skip(
      browserName === "webkit",
      "WebKit does not tab to links by default on macOS; covered on the other engines",
    );
    await page.goto("/en");

    await page.keyboard.press("Tab");
    const skip = page.getByRole("link", { name: "Skip to main content" });
    await expect(skip).toBeFocused();
    await expect(skip).toBeVisible();

    await page.keyboard.press("Enter");
    await expect(page.locator("#main-content")).toBeFocused();
  });

  test("the page announces its language and direction", async ({ page }) => {
    await page.goto("/ar");
    await expect(page.locator("html")).toHaveAttribute("lang", "ar");
    await expect(page.locator("html")).toHaveAttribute("dir", "rtl");
  });
});

test.describe("analytics consent", () => {
  test.use({ consent: "unknown" });

  test("measurement scripts stay off until the visitor opts in", async ({
    page,
  }) => {
    const measured: string[] = [];
    page.on("request", (request) => {
      // PostHog (via the /ingest proxy or directly) is the measuring traffic.
      if (/\/ingest\/|posthog\.com/.test(request.url())) {
        measured.push(request.url());
      }
    });

    await page.goto("/en");
    const banner = page.getByRole("region", { name: "Analytics consent" });
    await expect(banner).toBeVisible();
    expect(measured).toEqual([]);

    await banner.getByRole("button", { name: "No thanks" }).click();
    await expect(banner).toBeHidden();
    // The choice is saved by a Server Action; wait for its cookie before
    // reloading, or the reload races the write.
    await expect
      .poll(
        async () =>
          (await page.context().cookies()).find(
            (c) => c.name === "aedst_consent",
          )?.value,
      )
      .toBe("denied");
    await page.reload();
    await expect(banner).toBeHidden(); // the choice is remembered
    expect(measured).toEqual([]);
  });
});

test.describe("hardening for the new surfaces", () => {
  test("restricts powerful features and cross-origin reads", async ({
    request,
  }) => {
    const headers = (await request.get("/en")).headers();
    expect(headers["permissions-policy"]).toContain("geolocation=()");
    expect(headers["permissions-policy"]).toContain("camera=(self)");
    expect(headers["cross-origin-resource-policy"]).toBe("same-origin");
    expect(headers["cross-origin-opener-policy"]).toBe("same-origin");
  });

  test("lets the browser talk only to itself and object storage", async ({
    request,
  }) => {
    const csp = (await request.get("/en")).headers()["content-security-policy"];
    const connect =
      csp?.split("; ").find((d) => d.startsWith("connect-src")) ?? "";
    // Same origin plus MEDIA_ORIGINS (the mock doubles as object storage);
    // no analytics or error-tracking hosts — those go through /ingest and
    // /monitoring on this origin.
    expect(connect).toMatch(/^connect-src 'self' http:\/\/127\.0\.0\.1:8181/);
    expect(csp).not.toContain("posthog");
    expect(csp).not.toContain("sentry.io/api");
    expect(csp).toContain("frame-src 'none'");
  });

  test("never caches API responses", async ({ request }) => {
    const response = await request.post(
      "/api/rpc/aedst.tasks.v1.TasksService/GetTaskStats",
      { data: {}, headers: { origin: "http://127.0.0.1:3100" } },
    );
    expect(response.headers()["cache-control"]).toContain("no-store");
  });
});
