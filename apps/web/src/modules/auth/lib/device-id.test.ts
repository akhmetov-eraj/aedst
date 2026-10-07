import { describe, expect, it, vi } from "vitest";

// The module caches the id in memory, so each test gets a fresh copy.
async function freshDeviceId() {
  vi.resetModules();
  return (await import("./device-id")).getDeviceId;
}

describe("getDeviceId", () => {
  it("creates a UUID-based id and persists it", async () => {
    const getDeviceId = await freshDeviceId();

    const id = getDeviceId();

    expect(id).toMatch(/^web-[0-9a-f-]{36}$/);
    expect(window.localStorage.getItem("aedst.device-id")).toBe(id);
  });

  it("reuses the persisted id across page loads", async () => {
    window.localStorage.setItem("aedst.device-id", "web-existing");
    const getDeviceId = await freshDeviceId();

    expect(getDeviceId()).toBe("web-existing");
  });

  it("returns the same id on repeated calls", async () => {
    const getDeviceId = await freshDeviceId();

    expect(getDeviceId()).toBe(getDeviceId());
  });

  it("falls back to an in-memory id when storage is unavailable", async () => {
    vi.spyOn(Storage.prototype, "getItem").mockImplementation(() => {
      throw new Error("blocked");
    });
    vi.spyOn(Storage.prototype, "setItem").mockImplementation(() => {
      throw new Error("blocked");
    });
    const getDeviceId = await freshDeviceId();

    const id = getDeviceId();

    expect(id).toMatch(/^web-/);
    expect(getDeviceId()).toBe(id);
  });
});
