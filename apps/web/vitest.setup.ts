import "@testing-library/jest-dom/vitest";
import { cleanup, configure } from "@testing-library/react";
import { afterAll, afterEach, beforeAll, vi } from "vitest";
import { routerMock } from "./src/test/mocks/navigation";
import { server } from "./src/test/mocks/server";

// findBy*/waitFor default to 1 s, which a loaded CI runner (coverage, many
// workers) can exceed for real Connect round trips through MSW.
configure({ asyncUtilTimeout: 3_000 });

// next-intl's navigation needs a mounted App Router; swap it for a spy-able stub.
vi.mock("@/app/config/i18n/navigation", async () => {
  return await import("./src/test/mocks/navigation");
});

// jsdom lacks these; Base UI (Select, Dialog) and layout code probe for them.
if (typeof window !== "undefined") {
  globalThis.ResizeObserver ??= class {
    observe() {}
    unobserve() {}
    disconnect() {}
  };
  window.matchMedia ??= ((query: string) => ({
    matches: false,
    media: query,
    onchange: null,
    addEventListener() {},
    removeEventListener() {},
    addListener() {},
    removeListener() {},
    dispatchEvent: () => false,
  })) as typeof window.matchMedia;
  Element.prototype.scrollIntoView ??= () => {};
}

beforeAll(() => {
  server.listen({
    onUnhandledFrame: "error",
  });
});

afterEach(() => {
  cleanup();
  server.resetHandlers();
  for (const fn of Object.values(routerMock)) {
    fn.mockReset();
  }
  if (typeof window !== "undefined") {
    window.localStorage.clear();
  }
});

afterAll(() => {
  server.close();
});
