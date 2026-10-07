import { render, screen } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";

import { CurrentUser } from "@/modules/auth/ui/CurrentUser";
import { AuthService } from "@/shared/api/connect/generated/aedst/auth/v1/auth_pb";
import { fakeJwt } from "@/test/jwt";
import {
  rpcErrorHandler,
  rpcHandler,
  TEST_API_ORIGIN,
} from "@/test/mocks/connect";
import { server } from "@/test/mocks/server";
import { messages } from "@/test/render";

const jar = new Map<string, string>();

vi.mock("next/headers", () => ({
  cookies: async () => ({
    get: (name: string) =>
      jar.has(name) ? { name, value: jar.get(name) } : undefined,
  }),
}));

vi.mock("next-intl/server", () => ({
  getTranslations: async () => (key: string, values?: { email?: string }) =>
    key === "signedInAs"
      ? messages.auth.signedInAs.replace("{email}", values?.email ?? "")
      : key,
}));

beforeEach(() => {
  jar.clear();
  jar.set("aedst_session", fakeJwt({ expiresInSeconds: 600 }));
});

describe("CurrentUser (Server Component)", () => {
  it("renders the signed-in email from a server-side GetCurrentUser call", async () => {
    let seenAuth: string | null = null;
    server.use(
      rpcHandler(
        AuthService.method.getCurrentUser,
        (_input, request) => {
          seenAuth = request.headers.get("authorization");
          return { user: { id: "u1", email: "ada@example.com" } };
        },
        TEST_API_ORIGIN,
      ),
    );

    render(await CurrentUser());

    expect(screen.getByText("Signed in as ada@example.com")).toBeVisible();
    expect(seenAuth).toMatch(/^Bearer /);
  });

  it("renders nothing rather than breaking the page when the call fails", async () => {
    server.use(
      rpcErrorHandler(
        AuthService.method.getCurrentUser,
        "unauthenticated",
        "expired",
        TEST_API_ORIGIN,
      ),
    );

    const { container } = render(await CurrentUser());

    expect(container).toBeEmptyDOMElement();
  });

  it("renders nothing when the response has no user", async () => {
    server.use(
      rpcHandler(
        AuthService.method.getCurrentUser,
        () => ({}),
        TEST_API_ORIGIN,
      ),
    );

    const { container } = render(await CurrentUser());

    expect(container).toBeEmptyDOMElement();
  });
});
